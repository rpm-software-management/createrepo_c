/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <glib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <zlib.h>
#include <assert.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "error.h"
#include "logging.h"
#include "misc.h"
#include "checksum.h"
#include "repomd.h"
#include "repomd_internal.h"
#include "compression_wrapper.h"

#define LOCATION_HREF_PREFIX        "repodata/"
#define DEFAULT_DATABASE_VERSION    10
#define BUFFER_SIZE                 8192

typedef struct _contentStat {
    char *checksum;
    gint64 size;
} contentStat;

cr_DistroTag *
cr_distrotag_new()
{
    return (cr_DistroTag *) g_malloc0(sizeof(cr_DistroTag));
}

cr_RepomdRecord *
cr_repomd_record_new(const char *type, const char *path)
{
    cr_RepomdRecord *md = g_malloc0(sizeof(*md));
    md->chunk = g_string_chunk_new(128);
    md->type  = cr_safe_string_chunk_insert(md->chunk, type);
    if (path) {
        gchar *filename = cr_get_filename(path);
        gchar *location_href = g_strconcat(LOCATION_HREF_PREFIX, filename, NULL);
        md->location_real = g_string_chunk_insert(md->chunk, path);
        md->location_href = g_string_chunk_insert(md->chunk, location_href);
        g_free(location_href);
    }
    return md;
}

void
cr_repomd_record_free(cr_RepomdRecord *md)
{
    if (!md)
        return;

    g_string_chunk_free(md->chunk);
    g_free(md);
}

cr_RepomdRecord *
cr_repomd_record_copy(const cr_RepomdRecord *orig)
{
    cr_RepomdRecord *rec;

    if (!orig) return NULL;

    rec = cr_repomd_record_new(orig->type, NULL);
    rec->location_real      = cr_safe_string_chunk_insert(rec->chunk,
                                                orig->location_real);
    rec->location_href      = cr_safe_string_chunk_insert(rec->chunk,
                                                orig->location_href);
    rec->location_base      = cr_safe_string_chunk_insert(rec->chunk,
                                                orig->location_base);
    rec->checksum           = cr_safe_string_chunk_insert(rec->chunk,
                                                orig->checksum);
    rec->checksum_type      = cr_safe_string_chunk_insert(rec->chunk,
                                                orig->checksum_type);
    rec->checksum_open      = cr_safe_string_chunk_insert(rec->chunk,
                                                orig->checksum_open);
    rec->checksum_open_type = cr_safe_string_chunk_insert(rec->chunk,
                                                orig->checksum_open_type);
    rec->timestamp = orig->timestamp;
    rec->size      = orig->size;
    rec->size_open = orig->size_open;
    rec->db_ver    = orig->db_ver;

    return rec;
}

contentStat *
cr_get_compressed_content_stat(const char *filename,
                               cr_ChecksumType checksum_type,
                               GError **err)
{
    GError *tmp_err = NULL;

    assert(filename);
    assert(!err || *err == NULL);

    if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_NOFILE,
                    "File %s doesn't exists or not a regular file", filename);
        return NULL;
    }


    // Open compressed file

    CR_FILE *cwfile = cr_open(filename,
                              CR_CW_MODE_READ,
                              CR_CW_AUTO_DETECT_COMPRESSION,
                              &tmp_err);
    if (!cwfile) {
        g_propagate_prefixed_error(err, tmp_err,
                                   "Cannot open a file %s: ", filename);
        return NULL;
    }


    // Read compressed file and calculate checksum and size

    cr_ChecksumCtx *checksum = cr_checksum_new(checksum_type, &tmp_err);
    if (tmp_err) {
        g_critical("%s: g_checksum_new() failed", __func__);
        g_propagate_prefixed_error(err, tmp_err,
                "Error while checksum calculation: ");
        return NULL;
    }

    gint64 size = 0;
    long readed;
    unsigned char buffer[BUFFER_SIZE];

    do {
        readed = cr_read(cwfile, (void *) buffer, BUFFER_SIZE, &tmp_err);
        if (readed == CR_CW_ERR) {
            g_debug("%s: Error while read compressed file %s: %s",
                    __func__, filename, tmp_err->message);
            g_propagate_prefixed_error(err, tmp_err,
                        "Error while read compressed file %s: ",
                        filename);
            break;
        }
        cr_checksum_update(checksum, buffer, readed, NULL);
        size += readed;
    } while (readed == BUFFER_SIZE);

    if (readed == CR_CW_ERR)
        return NULL;

    // Create result structure

    contentStat* result = g_malloc(sizeof(contentStat));
    if (result) {
        result->checksum = cr_checksum_final(checksum, NULL);
        result->size = size;
    } else {
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_MEMORY,
                    "Cannot allocate memory");
    }

    cr_close(cwfile, NULL);

    return result;
}

int
cr_repomd_record_fill(cr_RepomdRecord *md,
                      cr_ChecksumType checksum_type,
                      GError **err)
{
    const char *checksum_str;
    cr_ChecksumType checksum_t;
    gchar *path;
    GError *tmp_err = NULL;

    assert(md);
    assert(!err || *err == NULL);

    if (!(md->location_real) || !strlen(md->location_real)) {
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_BADARG,
                    "Empty locations in repomd record object.");
        return CRE_BADARG;
    }

    path = md->location_real;

    checksum_str = cr_checksum_name_str(checksum_type);
    checksum_t = checksum_type;

    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        // File doesn't exists
        g_warning("%s: File %s doesn't exists", __func__, path);
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_NOFILE,
                    "File %s doesn't exists or not a regular file", path);
        return CRE_NOFILE;
    }


    // Compute checksum of compressed file

    if (!md->checksum_type || !md->checksum) {
        gchar *chksum;

        chksum = cr_checksum_file(path, checksum_t, &tmp_err);
        if (tmp_err) {
            int code = tmp_err->code;
            g_propagate_prefixed_error(err, tmp_err,
                "Error while checksum calculation of %s:", path);
            return code;
        }

        md->checksum_type = g_string_chunk_insert(md->chunk, checksum_str);
        md->checksum = g_string_chunk_insert(md->chunk, chksum);
        g_free(chksum);
    }


    // Compute checksum of non compressed content and its size

    if (!md->checksum_open_type || !md->checksum_open || !md->size_open) {
        cr_CompressionType com_type = cr_detect_compression(path, &tmp_err);
        if (tmp_err) {
            int code = tmp_err->code;
            g_propagate_prefixed_error(err, tmp_err,
                    "Cannot detect compression type of %s: ", path);
            return code;
        }

        if (com_type != CR_CW_UNKNOWN_COMPRESSION &&
            com_type != CR_CW_NO_COMPRESSION)
        {
            // File compressed by supported algorithm
            contentStat *open_stat = NULL;

            open_stat = cr_get_compressed_content_stat(path, checksum_t, &tmp_err);
            if (tmp_err) {
                int code = tmp_err->code;
                g_propagate_prefixed_error(err, tmp_err,
                    "Error while computing stat of compressed content of %s:",
                    path);
                return code;
            }

            md->checksum_open_type = g_string_chunk_insert(md->chunk, checksum_str);
            md->checksum_open = g_string_chunk_insert(md->chunk, open_stat->checksum);
            if (!md->size_open)
                md->size_open = open_stat->size;
            g_free(open_stat->checksum);
            g_free(open_stat);
        } else {
            if (com_type != CR_CW_NO_COMPRESSION) {
                // Unknown compression
                g_warning("%s: File \"%s\" compressed by an unsupported type"
                          " of compression", __func__, path);
            }
            md->checksum_open_type = NULL;
            md->checksum_open = NULL;
            md->size_open = -1;
        }
    }


    // Get timestamp and size of compressed file

    if (!md->timestamp || !md->size) {
        struct stat buf;
        if (!stat(path, &buf)) {
            if (!md->timestamp) {
                md->timestamp = buf.st_mtime;
            }
            if (!md->size) {
                md->size = buf.st_size;
            }
        } else {
            g_warning("%s: Stat on file \"%s\" failed", __func__, path);
            g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_STAT,
                        "Stat() on %s failed: %s", path, strerror(errno));
            return CRE_STAT;
        }
    }


    // Set db version

    if (!md->db_ver)
        md->db_ver = DEFAULT_DATABASE_VERSION;

    return CRE_OK;
}

int
cr_repomd_record_compress_and_fill(cr_RepomdRecord *record,
                                   cr_RepomdRecord *crecord,
                                   cr_ChecksumType checksum_type,
                                   cr_CompressionType record_compression,
                                   GError **err)
{
    const char *suffix;
    gchar *path, *cpath;
    gchar *clocation_real, *clocation_href;
    gchar *checksum, *cchecksum;
    int readed;
    char buf[BUFFER_SIZE];
    CR_FILE *cw_plain;
    CR_FILE *cw_compressed;
    gint64 gf_size = -1, cgf_size = -1;
    gint64 gf_time = -1, cgf_time = -1;
    struct stat gf_stat, cgf_stat;
    const char *checksum_str = cr_checksum_name_str(checksum_type);
    GError *tmp_err = NULL;

    assert(record);
    assert(crecord);
    assert(!err || *err == NULL);

    if (!(record->location_real) || !strlen(record->location_real)) {
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_BADARG,
                    "Empty locations in repomd record object");
        return CRE_BADARG;
    }

    if (!g_file_test(record->location_real, G_FILE_TEST_IS_REGULAR)) {
        // File doesn't exists
        g_warning("%s: File %s doesn't exists", __func__, record->location_real);
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_NOFILE,
                    "File %s doesn't exists or not a regular file",
                    record->location_real);
        return CRE_NOFILE;;
    }

    // Paths

    suffix = cr_compression_suffix(record_compression);

    clocation_real = g_strconcat(record->location_real, suffix, NULL);
    clocation_href = g_strconcat(record->location_href, suffix, NULL);
    crecord->location_real = g_string_chunk_insert(crecord->chunk,
                                                   clocation_real);
    crecord->location_href = g_string_chunk_insert(crecord->chunk,
                                                   clocation_href);
    g_free(clocation_real);
    g_free(clocation_href);

    path  = record->location_real;
    cpath = crecord->location_real;


    // Compress file + get size of non compressed file

    cw_plain = cr_open(path,
                       CR_CW_MODE_READ,
                       CR_CW_NO_COMPRESSION,
                       &tmp_err);
    if (!cw_plain) {
        int code = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Cannot open %s: ", path);
        return code;
    }

    cw_compressed = cr_open(cpath,
                            CR_CW_MODE_WRITE,
                            record_compression,
                            &tmp_err);
    if (!cw_compressed) {
        int code = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Cannot open %s: ", cpath);
        return code;
    }

    while ((readed = cr_read(cw_plain, buf, BUFFER_SIZE, &tmp_err)) > 0) {
        cr_write(cw_compressed, buf, (unsigned int) readed, &tmp_err);
        if (tmp_err)
            break;
    }

    cr_close(cw_plain, NULL);

    if (tmp_err) {
        int code = tmp_err->code;
        cr_close(cw_compressed, NULL);
        g_debug("%s: Error while repomd record compression: %s", __func__,
                tmp_err->message);
        g_propagate_prefixed_error(err, tmp_err,
                "Error while compression %s -> %s:", path, cpath);
        return code;
    }

    cr_close(cw_compressed, &tmp_err);
    if (tmp_err) {
        int code = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err,
                "Error while closing %s: ", path);
        return code;
    }


    // Compute checksums

    checksum  = cr_checksum_file(path, checksum_type, &tmp_err);
    if (tmp_err) {
        int code = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err,
                                   "Error while checksum calculation:");
        return code;
    }

    cchecksum = cr_checksum_file(cpath, checksum_type, &tmp_err);
    if (tmp_err) {
        int code = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err,
                                   "Error while checksum calculation:");
        return code;
    }


    // Get stats

    if (stat(path, &gf_stat)) {
        g_debug("%s: Error while stat() on %s", __func__, path);
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_IO,
                    "Cannot stat %s", path);
        return CRE_IO;
    } else {
        gf_size = gf_stat.st_size;
        gf_time = gf_stat.st_mtime;
    }

    if (stat(cpath, &cgf_stat)) {
        g_debug("%s: Error while stat() on %s", __func__, cpath);
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_IO,
                    "Cannot stat %s", cpath);
        return CRE_IO;
    } else {
        cgf_size = cgf_stat.st_size;
        cgf_time = cgf_stat.st_mtime;
    }


    // Results

    record->checksum = g_string_chunk_insert(record->chunk, checksum);
    record->checksum_type = g_string_chunk_insert(record->chunk, checksum_str);
    record->checksum_open = NULL;
    record->checksum_open_type = NULL;
    record->timestamp = gf_time;
    record->size = gf_size;
    record->size_open = -1;

    crecord->checksum = g_string_chunk_insert(crecord->chunk, cchecksum);
    crecord->checksum_type = g_string_chunk_insert(crecord->chunk, checksum_str);
    crecord->checksum_open = g_string_chunk_insert(record->chunk, checksum);
    crecord->checksum_open_type = g_string_chunk_insert(record->chunk, checksum_str);
    crecord->timestamp = cgf_time;
    crecord->size = cgf_size;
    crecord->size_open = gf_size;

    g_free(checksum);
    g_free(cchecksum);

    return CRE_OK;
}

int
cr_repomd_record_rename_file(cr_RepomdRecord *md, GError **err)
{
    int x, len;
    gchar *location_prefix = NULL;
    const gchar *location_filename = NULL;
    gchar *new_location_href;
    gchar *new_location_real;

    assert(!err || *err == NULL);

    if (!md)
        return CRE_OK;

    if (!(md->location_real) || !strlen(md->location_real)) {
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_BADARG,
                    "Empty locations in repomd record object");
        return CRE_BADARG;
    }

    if (!md->checksum) {
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_BADARG,
                    "Record doesn't contain checksum");
        return CRE_BADARG;
    }

    x = strlen(md->location_real);
    for (; x > 0; x--) {
        if (md->location_real[x] == '/') {
            location_prefix = g_strndup(md->location_real, x+1);
            location_filename = cr_get_filename(md->location_real+x+1);
            break;
        }
    }

    // Check if the rename is necessary
    // During update with --keep-all-metadata some files (groupfile,
    // updateinfo, ..) could already have checksum in filenames
    if (g_str_has_prefix(location_filename, md->checksum)) {
        // The filename constains valid checksum
        g_free(location_prefix);
        return CRE_OK;
    }

    // Skip existing obsolete checksum in the name if there is any
    len = strlen(location_filename);
    if (len > 32) {
        // The filename is long -> it could contains a checksum
        for (x = 0; x < len; x++) {
            if (location_filename[x] == '-' && (
                   x == 32  // Prefix is MD5 checksum
                || x == 40  // Prefix is SHA1 checksum
                || x == 64  // Prefix is SHA256 checksum
                || x == 128 // Prefix is SHA512 checksum
               ))
            {
                location_filename = location_filename + x + 1;
                break;
            }
        }
    }

    // Prepare new name
    new_location_real = g_strconcat(location_prefix,
                                    md->checksum,
                                    "-",
                                    location_filename,
                                    NULL);
    g_free(location_prefix);

    // Rename file
    if (g_file_test (new_location_real, G_FILE_TEST_EXISTS)) {
        if (remove(new_location_real)) {
            g_critical("%s: Cannot delete old %s",
                       __func__,
                       new_location_real);
            g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_IO,
                        "File with name %s already exists and cannot be deleted",
                        new_location_real);
            g_free(new_location_real);
            return CRE_IO;
        }
    }
    if (rename(md->location_real, new_location_real)) {
        g_critical("%s: Cannot rename %s to %s",
                   __func__,
                   md->location_real,
                   new_location_real);
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_IO,
                    "Cannot rename %s to %s", md->location_real,
                    new_location_real);
        g_free(new_location_real);
        return CRE_IO;
    }

    // Update locations in repomd record
    md->location_real = g_string_chunk_insert(md->chunk, new_location_real);
    new_location_href = g_strconcat(LOCATION_HREF_PREFIX,
                                    md->checksum,
                                    "-",
                                    location_filename,
                                    NULL);
    md->location_href = g_string_chunk_insert(md->chunk, new_location_href);

    g_free(new_location_real);
    g_free(new_location_href);

    return CRE_OK;
}

cr_Repomd *
cr_repomd_new()
{
   cr_Repomd *repomd = g_malloc0(sizeof(cr_Repomd));
   repomd->chunk = g_string_chunk_new(0);
   return repomd;
}

void
cr_repomd_free(cr_Repomd *repomd)
{
    if (!repomd) return;
    cr_slist_free_full(repomd->records, (GDestroyNotify) cr_repomd_record_free );
    g_slist_free(repomd->repo_tags);
    cr_slist_free_full(repomd->distro_tags, (GDestroyNotify) g_free);
    g_slist_free(repomd->content_tags);
    g_string_chunk_free(repomd->chunk);
    g_free(repomd);
}

void
cr_repomd_set_record(cr_Repomd *repomd,
                     cr_RepomdRecord *record)
{
    if (!repomd || !record) return;
    repomd->records = g_slist_append(repomd->records, record);
}

void
cr_repomd_set_revision(cr_Repomd *repomd, const char *revision)
{
    if (!repomd) return;
    repomd->revision = cr_safe_string_chunk_insert(repomd->chunk, revision);
}

void
cr_repomd_set_repoid(cr_Repomd *repomd, const char *repoid, const char *type)
{
    if (!repomd) return;
    repomd->repoid = cr_safe_string_chunk_insert(repomd->chunk, repoid);
    repomd->repoid_type = cr_safe_string_chunk_insert(repomd->chunk, type);
}

void
cr_repomd_add_distro_tag(cr_Repomd *repomd,
                         const char *cpeid,
                         const char *tag)
{
    cr_DistroTag *distro;
    if (!repomd || !tag) return;
    distro = cr_distrotag_new();
    distro->cpeid = cr_safe_string_chunk_insert(repomd->chunk, cpeid);
    distro->val   = cr_safe_string_chunk_insert(repomd->chunk, tag);
    repomd->distro_tags = g_slist_append(repomd->distro_tags,
                                         (gpointer) distro);
}

void
cr_repomd_add_repo_tag(cr_Repomd *repomd, const char *tag)
{
    if (!repomd || !tag) return;
    repomd->repo_tags = g_slist_append(repomd->repo_tags,
                            cr_safe_string_chunk_insert(repomd->chunk, tag));
}

void
cr_repomd_add_content_tag(cr_Repomd *repomd, const char *tag)
{
    if (!repomd || !tag) return;
    repomd->content_tags = g_slist_append(repomd->content_tags,
                            cr_safe_string_chunk_insert(repomd->chunk, tag));
}

