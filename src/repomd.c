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
#include <sys/types.h>
#include <utime.h>
#include <time.h>
#include <zlib.h>
#include <assert.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "cleanup.h"
#include "error.h"
#include "misc.h"
#include "checksum.h"
#include "repomd.h"
#include "repomd_internal.h"
#include "compression_wrapper.h"

#define ERR_DOMAIN                  CREATEREPO_C_ERROR
#define LOCATION_HREF_PREFIX        "repodata/"
#define DEFAULT_DATABASE_VERSION    10
#define BUFFER_SIZE                 8192

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
    md->size_open = G_GINT64_CONSTANT(-1);
    md->size_header = G_GINT64_CONSTANT(-1);

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
    rec->timestamp   = orig->timestamp;
    rec->size        = orig->size;
    rec->size_open   = orig->size_open;
    rec->size_header = orig->size_header;
    rec->db_ver      = orig->db_ver;
    if (orig->checksum_header)
        rec->checksum_header      = cr_safe_string_chunk_insert(rec->chunk,
                                                    orig->checksum_header);
    if (orig->checksum_header_type)
        rec->checksum_header_type = cr_safe_string_chunk_insert(rec->chunk,
                                                    orig->checksum_header_type);

    return rec;
}

cr_ContentStat *
cr_get_compressed_content_stat(const char *filename,
                               cr_ChecksumType checksum_type,
                               GError **err)
{
    GError *tmp_err = NULL;

    assert(filename);
    assert(!err || *err == NULL);

    if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        g_set_error(err, ERR_DOMAIN, CRE_NOFILE,
                    "File %s doesn't exists or not a regular file", filename);
        return NULL;
    }


    // Open compressed file
    cr_ContentStat *read_stat = g_malloc0(sizeof(cr_ContentStat));

    CR_FILE *cwfile = cr_sopen(filename,
                               CR_CW_MODE_READ,
                               CR_CW_AUTO_DETECT_COMPRESSION,
                               read_stat,
                               &tmp_err);
    if (!cwfile) {
        cr_contentstat_free(read_stat, NULL);
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
        cr_close(cwfile, NULL);
        return NULL;
    }

    gint64 size = G_GINT64_CONSTANT(0);
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

    if (readed == CR_CW_ERR) {
        cr_close(cwfile, NULL);
        g_free(checksum);
        return NULL;
    }

    // Create result structure

    cr_ContentStat* result = g_malloc0(sizeof(cr_ContentStat));
    if (result) {
        if (cwfile->stat) {
            result->hdr_checksum = cwfile->stat->hdr_checksum;
            result->hdr_checksum_type = cwfile->stat->hdr_checksum_type;
            result->hdr_size = cwfile->stat->hdr_size;
        } else {
            result->hdr_checksum = NULL;
            result->hdr_checksum_type = 0;
            result->hdr_size = G_GINT64_CONSTANT(-1);
        }
        result->checksum = cr_checksum_final(checksum, NULL);
        result->size = size;
    } else {
        g_set_error(err, ERR_DOMAIN, CRE_MEMORY,
                    "Cannot allocate memory");
        g_free(checksum);
    }

    cr_close(cwfile, NULL);
    cr_contentstat_free(read_stat, NULL);

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
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Empty locations in repomd record object.");
        return CRE_BADARG;
    }

    path = md->location_real;

    checksum_str = cr_checksum_name_str(checksum_type);
    checksum_t = checksum_type;

    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        // File doesn't exists
        g_warning("%s: File %s doesn't exists", __func__, path);
        g_set_error(err, ERR_DOMAIN, CRE_NOFILE,
                    "File %s doesn't exists or not a regular file", path);
        return CRE_NOFILE;
    }

    // Compute checksum of compressed file

    if (!md->checksum_type || !md->checksum) {
        gchar *chksum;

        chksum = cr_checksum_file(path, checksum_t, &tmp_err);
        if (!chksum) {
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

    if (!md->checksum_open_type || !md->checksum_open || md->size_open == G_GINT64_CONSTANT(-1)) {
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
            cr_ContentStat *open_stat = NULL;

            open_stat = cr_get_compressed_content_stat(path, checksum_t, &tmp_err);
            if (tmp_err) {
                int code = tmp_err->code;
                g_propagate_prefixed_error(err, tmp_err,
                    "Error while computing stat of compressed content of %s:",
                    path);
                cr_contentstat_free(open_stat, NULL);
                return code;
            }
            md->checksum_open_type = g_string_chunk_insert(md->chunk, checksum_str);
            md->checksum_open = g_string_chunk_insert(md->chunk, open_stat->checksum);
            if (md->size_open == G_GINT64_CONSTANT(-1))
                md->size_open = open_stat->size;
            if (open_stat->hdr_checksum != NULL) {
                const char *hdr_checksum_str = cr_checksum_name_str(open_stat->hdr_checksum_type);

                md->checksum_header_type = g_string_chunk_insert(md->chunk, hdr_checksum_str);
                md->checksum_header = g_string_chunk_insert(md->chunk, open_stat->hdr_checksum);
                if (md->size_header == G_GINT64_CONSTANT(-1))
                    md->size_header = open_stat->hdr_size;
                g_free(open_stat->hdr_checksum);
            }
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
            md->size_open = G_GINT64_CONSTANT(-1);
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
            g_set_error(err, ERR_DOMAIN, CRE_STAT,
                        "Stat() on %s failed: %s", path, g_strerror(errno));
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
                                   const char *zck_dict_dir,
                                   GError **err)
{
    int ret = CRE_OK;
    const char *suffix;
    gchar *path, *cpath;
    gchar *clocation_real, *clocation_href;
    gchar *checksum = NULL;
    gchar *cchecksum = NULL;
    gchar *hdrchecksum = NULL;
    int readed;
    char buf[BUFFER_SIZE];
    CR_FILE *cw_plain;
    CR_FILE *cw_compressed;
    gint64 gf_size = G_GINT64_CONSTANT(-1), cgf_size = G_GINT64_CONSTANT(-1);
    gint64 gf_time = G_GINT64_CONSTANT(-1), cgf_time = G_GINT64_CONSTANT(-1);
    gint64 cgf_hdrsize = G_GINT64_CONSTANT(-1);
    struct stat gf_stat, cgf_stat;
    const char *checksum_str = cr_checksum_name_str(checksum_type);
    const char *hdr_checksum_str = NULL;
    GError *tmp_err = NULL;

    assert(record);
    assert(crecord);
    assert(!err || *err == NULL);

    if (!(record->location_real) || !strlen(record->location_real)) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Empty locations in repomd record object");
        return CRE_BADARG;
    }

    if (!g_file_test(record->location_real, G_FILE_TEST_IS_REGULAR)) {
        // File doesn't exists
        g_warning("%s: File %s doesn't exists", __func__, record->location_real);
        g_set_error(err, ERR_DOMAIN, CRE_NOFILE,
                    "File %s doesn't exists or not a regular file",
                    record->location_real);
        return CRE_NOFILE;;
    }

    // Paths

    suffix = cr_compression_suffix(record_compression);

    // Only update locations, if they are not set yet
    if (!crecord->location_real){
        clocation_real = g_strconcat(record->location_real, suffix, NULL);
        crecord->location_real = g_string_chunk_insert(crecord->chunk,
                                                       clocation_real);
        g_free(clocation_real);
    }
    if (!crecord->location_href){
        clocation_href = g_strconcat(record->location_href, suffix, NULL);
        crecord->location_href = g_string_chunk_insert(crecord->chunk,
                                                       clocation_href);
        g_free(clocation_href);
    }

    path  = record->location_real;
    cpath = crecord->location_real;

    // Compress file + get size of non compressed file

    int mode = CR_CW_NO_COMPRESSION;
    if (record_compression == CR_CW_ZCK_COMPRESSION)
        mode = CR_CW_AUTO_DETECT_COMPRESSION;

    cw_plain = cr_open(path,
                       CR_CW_MODE_READ,
                       mode,
                       &tmp_err);
    if (!cw_plain) {
        ret = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Cannot open %s: ", path);
        return ret;
    }

    _cleanup_free_ gchar *dict = NULL;
    size_t dict_size = 0;
    if (record_compression == CR_CW_ZCK_COMPRESSION && zck_dict_dir) {
        /* Find zdict */
        _cleanup_free_ gchar *file_basename = NULL;
        _cleanup_free_ gchar *dict_base = NULL;
        if (g_str_has_suffix(cpath, ".zck"))
            dict_base = g_strndup(cpath, strlen(cpath)-4);
        else
            dict_base = g_strdup(cpath);
        file_basename = g_path_get_basename(dict_base); 
        _cleanup_free_ gchar *dict_file = cr_get_dict_file(zck_dict_dir, file_basename);
        /* Read dictionary from file */
        if (dict_file && !g_file_get_contents(dict_file, &dict,
                                             &dict_size, &tmp_err)) {
            ret = tmp_err->code;
            g_propagate_prefixed_error(err, tmp_err, "Error reading zchunk dict %s:", dict_file);
            return ret;
        }
    }

    _cleanup_free_ cr_ContentStat *out_stat = g_malloc0(sizeof(cr_ContentStat));
    cw_compressed = cr_sopen(cpath,
                             CR_CW_MODE_WRITE,
                             record_compression,
                             out_stat,
                             &tmp_err);
    if (!cw_compressed) {
        ret = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Cannot open %s: ", cpath);
        return ret;
    }

    if (record_compression == CR_CW_ZCK_COMPRESSION) {
        if (dict && cr_set_dict(cw_compressed, dict, dict_size, &tmp_err) != CRE_OK) {
            ret = tmp_err->code;
            g_propagate_prefixed_error(err, tmp_err, "Unable to set zdict for %s: ", cpath);
            return ret;
        }
        if (cr_set_autochunk(cw_compressed, TRUE, &tmp_err) != CRE_OK) {
            ret = tmp_err->code;
            g_propagate_prefixed_error(err, tmp_err, "Unable to set auto-chunking for %s: ", cpath);
            return ret;
        }
    }

    while ((readed = cr_read(cw_plain, buf, BUFFER_SIZE, &tmp_err)) > 0) {
        cr_write(cw_compressed, buf, (unsigned int) readed, &tmp_err);
        if (tmp_err)
            break;
    }

    cr_close(cw_plain, NULL);

    if (tmp_err) {
        ret = tmp_err->code;
        cr_close(cw_compressed, NULL);
        g_debug("%s: Error while repomd record compression: %s", __func__,
                tmp_err->message);
        g_propagate_prefixed_error(err, tmp_err,
                "Error while compression %s -> %s:", path, cpath);
        return ret;
    }

    cr_close(cw_compressed, &tmp_err);
    if (tmp_err) {
        ret = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err,
                "Error while closing %s: ", path);
        return ret;
    }

    // Compute checksums

    checksum  = cr_checksum_file(path, checksum_type, &tmp_err);
    if (!checksum) {
        ret = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err,
                                   "Error while checksum calculation:");
        goto end;
    }

    cchecksum = cr_checksum_file(cpath, checksum_type, &tmp_err);
    if (!cchecksum) {
        ret = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err,
                                   "Error while checksum calculation:");
        goto end;
    }


    // Get stats

    if (stat(path, &gf_stat)) {
        g_debug("%s: Error while stat() on %s", __func__, path);
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot stat %s", path);
        ret = CRE_IO;
        goto end;
    }

    gf_size = gf_stat.st_size;
    gf_time = gf_stat.st_mtime;

    if (stat(cpath, &cgf_stat)) {
        g_debug("%s: Error while stat() on %s", __func__, cpath);
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot stat %s", cpath);
        ret = CRE_IO;
        goto end;
    }

    cgf_size = cgf_stat.st_size;
    cgf_time = cgf_stat.st_mtime;
    if (out_stat->hdr_checksum) {
        cgf_hdrsize = out_stat->hdr_size;
        hdr_checksum_str = cr_checksum_name_str(out_stat->hdr_checksum_type);
        hdrchecksum = out_stat->hdr_checksum;
    }

    // Results

    record->checksum = g_string_chunk_insert(record->chunk, checksum);
    record->checksum_type = g_string_chunk_insert(record->chunk, checksum_str);
    record->checksum_open = NULL;
    record->checksum_open_type = NULL;
    record->checksum_header = NULL;
    record->checksum_header_type = NULL;
    record->timestamp = gf_time;
    record->size = gf_size;
    record->size_open = G_GINT64_CONSTANT(-1);
    record->size_header = G_GINT64_CONSTANT(-1);

    crecord->checksum = g_string_chunk_insert(crecord->chunk, cchecksum);
    crecord->checksum_type = g_string_chunk_insert(crecord->chunk, checksum_str);
    crecord->checksum_open = g_string_chunk_insert(record->chunk, checksum);
    crecord->checksum_open_type = g_string_chunk_insert(record->chunk, checksum_str);
    if (hdr_checksum_str) {
        crecord->checksum_header = g_string_chunk_insert(crecord->chunk, hdrchecksum);
        crecord->checksum_header_type = g_string_chunk_insert(crecord->chunk, hdr_checksum_str);
    } else {
        crecord->checksum_header = NULL;
        crecord->checksum_header_type = 0;
    }
    crecord->timestamp = cgf_time;
    crecord->size = cgf_size;
    crecord->size_open = gf_size;
    crecord->size_header = cgf_hdrsize;

end:
    g_free(checksum);
    g_free(cchecksum);
    g_free(hdrchecksum);

    return ret;
}

static int
rename_file(gchar **location_real, gchar **location_href, char *checksum,
            cr_RepomdRecord *md, GError **err)
{
    int x, len;
    gchar *location_prefix = NULL;
    const gchar *location_filename = NULL;
    gchar *new_location_href;
    gchar *new_location_real;

    assert(!err || *err == NULL);
    assert(*location_real && *location_href);

    location_filename = *location_real;

    x = strlen(*location_real);
    for (; x > 0; x--) {
        if ((*location_real)[x] == '/') {
            location_prefix = g_strndup(*location_real, x+1);
            location_filename = cr_get_filename(*location_real+x+1);
            break;
        }
    }

    if (!location_prefix)
        // In case that the location_real doesn't contain '/'
        location_prefix = g_strdup("");

    // Check if the rename is necessary
    // During update with --keep-all-metadata some files (groupfile,
    // updateinfo, ..) could already have checksum in filenames
    if (g_str_has_prefix(location_filename, checksum)) {
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
                                    checksum,
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
            g_set_error(err, ERR_DOMAIN, CRE_IO,
                        "File with name %s already exists and cannot be deleted",
                        new_location_real);
            g_free(new_location_real);
            return CRE_IO;
        }
    }
    if (rename(*location_real, new_location_real)) {
        g_critical("%s: Cannot rename %s to %s",
                   __func__,
                   *location_real,
                   new_location_real);
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot rename %s to %s", *location_real,
                    new_location_real);
        g_free(new_location_real);
        return CRE_IO;
    }

    // Update locations in repomd record
    *location_real = g_string_chunk_insert(md->chunk, new_location_real);
    new_location_href = g_strconcat(LOCATION_HREF_PREFIX,
                                    checksum,
                                    "-",
                                    location_filename,
                                    NULL);
    *location_href = g_string_chunk_insert(md->chunk, new_location_href);

    g_free(new_location_real);
    g_free(new_location_href);

    return CRE_OK;
}

int
cr_repomd_record_rename_file(cr_RepomdRecord *md, GError **err)
{
    assert(!err || *err == NULL);

    if (!md)
        return CRE_OK;

    if (!(md->location_real) || !strlen(md->location_real)) {
        g_debug("Empty locations in repomd record object");
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Empty locations in repomd record object");
        return CRE_BADARG;
    }

    if (!md->checksum) {
        g_debug("Record doesn't contain checksum");
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Record doesn't contain checksum");
        return CRE_BADARG;
    }
    char *checksum = md->checksum;

    int retval = rename_file(&(md->location_real), &(md->location_href),
                             checksum, md, err);
    return retval;
}

void
cr_repomd_record_set_timestamp(cr_RepomdRecord *record,
                               gint64 timestamp)
{
    struct utimbuf times = { timestamp, timestamp };

    if (!record)
        return;
    record->timestamp = timestamp;
    // intentionally ignore error
    utime(record->location_real, &times);
}

void
cr_repomd_record_load_contentstat(cr_RepomdRecord *record,
                                  cr_ContentStat *stats)
{
    if (!stats)
        return;

    record->checksum_open = cr_safe_string_chunk_insert(record->chunk,
                                                        stats->checksum);
    record->checksum_open_type = cr_safe_string_chunk_insert(record->chunk,
                                cr_checksum_name_str(stats->checksum_type));
    record->size_open = stats->size;
}

void
cr_repomd_record_load_zck_contentstat(cr_RepomdRecord *record,
                                      cr_ContentStat *stats)
{
    if (!stats)
        return;

    record->checksum_header = cr_safe_string_chunk_insert(record->chunk,
                                                          stats->hdr_checksum);
    record->checksum_header_type = cr_safe_string_chunk_insert(record->chunk,
                                   cr_checksum_name_str(stats->hdr_checksum_type));
    record->size_header = stats->hdr_size;
}

cr_Repomd *
cr_repomd_new()
{
   cr_Repomd *repomd = g_malloc0(sizeof(cr_Repomd));
   repomd->chunk = g_string_chunk_new(0);
   return repomd;
}

cr_Repomd *
cr_repomd_copy(cr_Repomd *orig)
{
    cr_Repomd *new = cr_repomd_new();

    cr_safe_string_chunk_insert(new->chunk, orig->revision);
    cr_safe_string_chunk_insert(new->chunk, orig->repoid);
    cr_safe_string_chunk_insert(new->chunk, orig->repoid_type);
    cr_safe_string_chunk_insert(new->chunk, orig->contenthash);
    cr_safe_string_chunk_insert(new->chunk, orig->contenthash_type);

    for (GSList *elem = orig->repo_tags; elem; elem = g_slist_next(elem)) {
        gchar *str = elem->data;
        cr_repomd_add_repo_tag(new, str);
    }
    new->repo_tags = g_slist_reverse(new->repo_tags);

    for (GSList *elem = orig->content_tags; elem; elem = g_slist_next(elem)) {
        gchar *str = elem->data;
        cr_repomd_add_content_tag(new, str);
    }
    new->content_tags = g_slist_reverse(new->content_tags);

    for (GSList *elem = orig->distro_tags; elem; elem = g_slist_next(elem)) {
        cr_DistroTag *tag = elem->data;
        cr_repomd_add_distro_tag(new, tag->cpeid, tag->val);
    }
    new->distro_tags = g_slist_reverse(new->distro_tags);

    for (GSList *elem = orig->records; elem; elem = g_slist_next(elem)) {
        cr_RepomdRecord *rec = elem->data;
        rec = cr_repomd_record_copy(rec);
        cr_repomd_set_record(new, rec);
    }
    new->records = g_slist_reverse(new->records);

    return new;
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

    cr_RepomdRecord *delrec = NULL;
    // Remove all existing record of the same type
    while ((delrec = cr_repomd_get_record(repomd, record->type)) != NULL) {
	cr_repomd_detach_record(repomd, delrec);
	cr_repomd_record_free(delrec);
    }

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
cr_repomd_set_contenthash(cr_Repomd *repomd, const char *hash, const char *type)
{
    if (!repomd) return;
    repomd->contenthash = cr_safe_string_chunk_insert(repomd->chunk, hash);
    repomd->contenthash_type = cr_safe_string_chunk_insert(repomd->chunk, type);
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

void
cr_repomd_detach_record(cr_Repomd *repomd, cr_RepomdRecord *rec)
{
    if (!repomd || !rec) return;
    repomd->records = g_slist_remove(repomd->records, rec);
}

void
cr_repomd_remove_record(cr_Repomd *repomd, const char *type)
{
    cr_RepomdRecord *rec = cr_repomd_get_record(repomd, type);
    if (!rec) return;
    cr_repomd_detach_record(repomd, rec);
    cr_repomd_record_free(rec);
}

cr_RepomdRecord *
cr_repomd_get_record(cr_Repomd *repomd, const char *type)
{
    if (!repomd || !type)
        return NULL;

    for (GSList *elem = repomd->records; elem; elem = g_slist_next(elem)) {
        cr_RepomdRecord *rec = elem->data;
        assert(rec);
        if (!g_strcmp0(rec->type, type))
            return rec;
    }
    return NULL;
}

static gint
record_type_value(const char *type) {
    if (!g_strcmp0(type, "primary"))
        return 1;
    if (!g_strcmp0(type, "filelists"))
        return 2;
    if (!g_strcmp0(type, "other"))
        return 3;
    if (!g_strcmp0(type, "primary_db"))
        return 4;
    if (!g_strcmp0(type, "filelists_db"))
        return 5;
    if (!g_strcmp0(type, "other_db"))
        return 6;
    if (!g_strcmp0(type, "primary_zck"))
        return 7;
    if (!g_strcmp0(type, "filelists_zck"))
        return 8;
    if (!g_strcmp0(type, "other_zck"))
        return 9;
    return 10;
}

static gint
record_cmp(gconstpointer _a, gconstpointer _b)
{
    const cr_RepomdRecord *a = _a;
    const cr_RepomdRecord *b = _b;

    gint a_val = record_type_value(a->type);
    gint b_val = record_type_value(b->type);

    // Keep base metadata files sorted by logical order (primary, filelists, ..)
    if (a_val < b_val) return -1;
    if (a_val > b_val) return 1;

    // Other metadta sort by the type
    gint ret = g_strcmp0(a->type, b->type);
    if (ret)
        return ret;

    // If even the type is not sufficient, use location href
    ret = g_strcmp0(a->location_href, b->location_href);

    // If even the location href is not sufficient, use the location base
    return ret ? ret : g_strcmp0(a->location_base, b->location_base);
}

void
cr_repomd_sort_records(cr_Repomd *repomd)
{
    if (!repomd)
        return;

    repomd->records = g_slist_sort(repomd->records, record_cmp);
}
