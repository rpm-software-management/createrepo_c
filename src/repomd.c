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
#include "repomd.h"
#include "compression_wrapper.h"

#define LOCATION_HREF_PREFIX            "repodata/"

#define DEFAULT_DATABASE_VERSION        10

#define STR_BUFFER_SIZE      32
#define BUFFER_SIZE          8192

#define RPM_NS          "http://linux.duke.edu/metadata/rpm"
#define XMLNS_NS        "http://linux.duke.edu/metadata/repo"

#define XML_ENC         "UTF-8"
#define FORMAT_XML      1


typedef struct _contentStat {
    char *checksum;
    long size;
} contentStat;


typedef struct _cr_Distro * cr_Distro;
struct _cr_Distro {
    gchar *cpeid;
    gchar *val;
};


cr_Distro
cr_new_distro()
{
    return (cr_Distro) g_malloc0(sizeof(struct _cr_Distro));
}


void
cr_free_distro(cr_Distro distro)
{
    if (!distro) return;
    g_free(distro->cpeid);
    g_free(distro->val);
    g_free(distro);
}


cr_RepomdRecord
cr_repomd_record_new(const char *path)
{
    cr_RepomdRecord md = (cr_RepomdRecord) g_malloc0(sizeof(*md));
    md->chunk = g_string_chunk_new(1024);
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
cr_repomd_record_free(cr_RepomdRecord md)
{
    if (!md)
        return;

    g_string_chunk_free(md->chunk);
    g_free(md);
}


contentStat *
cr_get_compressed_content_stat(const char *filename,
                               cr_ChecksumType checksum_type,
                               GError **err)
{
    GError *tmp_err = NULL;

    assert(filename);
    assert(!err || *err == NULL);

    if (!g_file_test(filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_NOFILE,
                    "File doesn't exists");
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


    // Create checksum structure

    GChecksumType gchecksumtype;
    switch (checksum_type) {
        case CR_CHECKSUM_MD5:
            gchecksumtype = G_CHECKSUM_MD5;
            break;
        case CR_CHECKSUM_SHA1:
            gchecksumtype = G_CHECKSUM_SHA1;
            break;
        case CR_CHECKSUM_SHA256:
            gchecksumtype = G_CHECKSUM_SHA256;
            break;
        default:
            g_critical("%s: Unknown checksum type", __func__);
            g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_UNKNOWNCHECKSUMTYPE,
                        "Unknown checksum type: %d", checksum_type);
            return NULL;
    };


    // Read compressed file and calculate checksum and size

    GChecksum *checksum = g_checksum_new(gchecksumtype);
    if (!checksum) {
        g_critical("%s: g_checksum_new() failed", __func__);
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_UNKNOWNCHECKSUMTYPE,
                    "g_checksum_new() failed - Unknown checksum type");
        return NULL;
    }

    long size = 0;
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
        g_checksum_update (checksum, buffer, readed);
        size += readed;
    } while (readed == BUFFER_SIZE);

    if (readed == CR_CW_ERR)
        return NULL;

    // Create result structure

    contentStat* result = g_malloc(sizeof(contentStat));
    if (result) {
        result->checksum = g_strdup(g_checksum_get_string(checksum));
        result->size = size;
    } else {
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_MEMORY,
                    "Cannot allocate memory");
    }


    // Clean up

    g_checksum_free(checksum);
    cr_close(cwfile, NULL);

    return result;
}



int
cr_repomd_record_fill(cr_RepomdRecord md,
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

    if (!g_file_test(path, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR)) {
        // File doesn't exists
        g_warning("%s: File %s doesn't exists", __func__, path);
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_NOFILE,
                    "File %s doesn't exists", path);
        return CRE_NOFILE;
    }


    // Compute checksum of compressed file

    if (!md->checksum_type || !md->checksum) {
        gchar *chksum;

        chksum = cr_compute_file_checksum(path, checksum_t, &tmp_err);
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
            // Unknown compression
            g_warning("%s: File \"%s\" compressed by an unsupported type"
                      " of compression", __func__, path);
            md->checksum_open_type = g_string_chunk_insert(md->chunk, "UNKNOWN");
            md->checksum_open = g_string_chunk_insert(md->chunk,
                        "file_compressed_by_an_unsupported_type_of_compression");
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
cr_repomd_record_compress_and_fill(cr_RepomdRecord record,
                                   cr_RepomdRecord crecord,
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
    long gf_size = -1, cgf_size = -1;
    long gf_time = -1, cgf_time = -1;
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

    if (!g_file_test(record->location_real, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR)) {
        // File doesn't exists
        g_warning("%s: File %s doesn't exists", __func__, record->location_real);
        g_set_error(err, CR_REPOMD_RECORD_ERROR, CRE_NOFILE,
                    "File %s doesn't exists", record->location_real);
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

    checksum  = cr_compute_file_checksum(path, checksum_type, &tmp_err);
    if (tmp_err) {
        int code = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err,
                                   "Error while checksum calculation:");
        return code;
    }

    cchecksum = cr_compute_file_checksum(cpath, checksum_type, &tmp_err);
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
cr_repomd_record_rename_file(cr_RepomdRecord md, GError **err)
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


void
cr_repomd_dump_data_items(xmlNodePtr root, cr_RepomdRecord md, const xmlChar *type)
{
    xmlNodePtr data, node;
    gchar str_buffer[STR_BUFFER_SIZE];

    if (!root || !md || !type) {
        return;
    }

    data = xmlNewChild(root, NULL, BAD_CAST "data", NULL);
    xmlNewProp(data, BAD_CAST "type", BAD_CAST type);


    node = xmlNewTextChild(data, NULL, BAD_CAST "checksum",
                           BAD_CAST md->checksum);
    xmlNewProp(node, BAD_CAST "type", BAD_CAST md->checksum_type);

    if (md->checksum_open) {
        node = xmlNewTextChild(data, NULL, BAD_CAST "open-checksum",
                               BAD_CAST md->checksum_open);
        xmlNewProp(node, BAD_CAST "type", BAD_CAST md->checksum_open_type);
    }

    node = xmlNewChild(data, NULL, BAD_CAST "location", NULL);
    xmlNewProp(node, BAD_CAST "href", BAD_CAST md->location_href);

    g_snprintf(str_buffer, STR_BUFFER_SIZE, "%ld", md->timestamp);
    xmlNewChild(data, NULL, BAD_CAST "timestamp", BAD_CAST str_buffer);

    g_snprintf(str_buffer, STR_BUFFER_SIZE, "%ld", md->size);
    xmlNewChild(data, NULL, BAD_CAST "size", BAD_CAST str_buffer);

    if (md->size_open != -1) {
        g_snprintf(str_buffer, STR_BUFFER_SIZE, "%ld", md->size_open);
        xmlNewChild(data, NULL, BAD_CAST "open-size", BAD_CAST str_buffer);
    }

    if (g_str_has_suffix((char *)type, "_db")) {
        g_snprintf(str_buffer, STR_BUFFER_SIZE, "%d", md->db_ver);
        xmlNewChild(data, NULL, BAD_CAST "database_version",
                    BAD_CAST str_buffer);
    }
}


char *
cr_repomd_xml_dump(cr_Repomd repomd)
{
    xmlDocPtr doc;
    xmlNodePtr root;
    GList *keys, *element;


    // Start of XML document

    doc = xmlNewDoc(BAD_CAST "1.0");
    root = xmlNewNode(NULL, BAD_CAST "repomd");
    xmlNewNs(root, BAD_CAST XMLNS_NS, BAD_CAST NULL);
    xmlNewNs(root, BAD_CAST RPM_NS, BAD_CAST "rpm");
    xmlDocSetRootElement(doc, root);

    if (repomd->revision) {
        xmlNewChild(root, NULL, BAD_CAST "revision", BAD_CAST repomd->revision);
    } else {
        // Use the current time if no revision was explicitly specified
        gchar *rev = g_strdup_printf("%ld", time(NULL));
        xmlNewChild(root, NULL, BAD_CAST "revision", BAD_CAST rev);
        g_free(rev);
    }


    // Tags

    if (repomd->repo_tags || repomd->distro_tags || repomd->content_tags) {
        GSList *element;
        xmlNodePtr tags = xmlNewChild(root, NULL, BAD_CAST "tags", NULL);
        element = repomd->content_tags;
        for (; element; element = g_slist_next(element))
            xmlNewChild(tags, NULL,
                        BAD_CAST "content",
                        BAD_CAST (element->data ? element->data : ""));
        element = repomd->repo_tags;
        for (; element; element = g_slist_next(element))
            xmlNewChild(tags, NULL,
                        BAD_CAST "repo",
                        BAD_CAST (element->data ? element->data : ""));
        element = repomd->distro_tags;
        for (; element; element = g_slist_next(element)) {
            xmlNodePtr distro_elem;
            cr_Distro distro = (cr_Distro) element->data;
            distro_elem = xmlNewChild(tags,
                                      NULL,
                                      BAD_CAST "distro",
                                      BAD_CAST (distro->val ? distro->val : ""));
            if (distro->cpeid)
                xmlNewProp(distro_elem,
                           BAD_CAST "cpeid",
                           BAD_CAST distro->cpeid);
        }
    }

    // Records

    keys = g_hash_table_get_keys(repomd->records);
    keys = g_list_sort(keys, (GCompareFunc) g_strcmp0);

    for (element = keys; element; element = g_list_next(element)) {
        char *type = element->data;
        cr_RepomdRecord rec = g_hash_table_lookup(repomd->records, type);
        cr_repomd_dump_data_items(root, rec, (const xmlChar *) type);
    }

    g_list_free(keys);

    // Dump IT!

    char *result;
    xmlDocDumpFormatMemoryEnc(doc,
                              (xmlChar **) &result,
                              NULL,
                              XML_ENC,
                              FORMAT_XML);


    // Clean up

    xmlFreeDoc(doc);

    return result;
}


cr_Repomd
cr_repomd_new()
{
   cr_Repomd repomd = g_malloc0(sizeof(struct _cr_Repomd));
   repomd->records = g_hash_table_new_full(g_str_hash,
                                           g_str_equal,
                                           g_free,
                                           (GDestroyNotify) cr_repomd_record_free);
   return repomd;
}


void
cr_repomd_free(cr_Repomd repomd)
{
    if (!repomd) return;
    g_hash_table_destroy(repomd->records);
    cr_slist_free_full(repomd->repo_tags, g_free);
    cr_slist_free_full(repomd->distro_tags, (GDestroyNotify) cr_free_distro);
    cr_slist_free_full(repomd->content_tags, g_free);
    g_free(repomd->revision);
    g_free(repomd);
}


void
cr_repomd_set_record(cr_Repomd repomd,
                     cr_RepomdRecord record,
                     const char *type)
{
    if (!repomd || !record) return;
    g_hash_table_replace(repomd->records, g_strdup(type), record);
}


void
cr_repomd_set_revision(cr_Repomd repomd, const char *revision)
{
    if (!repomd) return;
    if (repomd->revision)  // A revision string already exists
        g_free(repomd->revision);
    repomd->revision = g_strdup(revision);
}


void
cr_repomd_add_distro_tag(cr_Repomd repomd, const char *cpeid, const char *tag)
{
    cr_Distro distro;
    if (!repomd || !tag) return;
    distro = cr_new_distro();
    distro->cpeid = g_strdup(cpeid);
    distro->val   = g_strdup(tag);
    repomd->distro_tags = g_slist_prepend(repomd->distro_tags,
                                          (gpointer) distro);
}


void
cr_repomd_add_repo_tag(cr_Repomd repomd, const char *tag)
{
    if (!repomd) return;
    repomd->repo_tags = g_slist_append(repomd->repo_tags, g_strdup(tag));
}


void
cr_repomd_add_content_tag(cr_Repomd repomd, const char *tag)
{
    if (!repomd) return;
    repomd->content_tags = g_slist_append(repomd->content_tags, g_strdup(tag));
}

