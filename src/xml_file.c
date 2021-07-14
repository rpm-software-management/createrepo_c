/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2013  Tomas Mlcoch
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
#include <glib/gstdio.h>
#include <assert.h>
#include "xml_file.h"
#include <errno.h>
#include "error.h"
#include "xml_dump.h"
#include "compression_wrapper.h"
#include "xml_dump_internal.h"

#define ERR_DOMAIN              CREATEREPO_C_ERROR

#define XML_HEADER              "<?xml version=\""XML_DOC_VERSION \
                                "\" encoding=\""XML_ENCODING"\"?>\n"

#define XML_PRIMARY_HEADER      XML_HEADER"<metadata xmlns=\"" \
                                CR_XML_COMMON_NS"\" xmlns:rpm=\"" \
                                CR_XML_RPM_NS"\" packages=\"%d\">\n"
#define XML_FILELISTS_HEADER    XML_HEADER"<filelists xmlns=\"" \
                                CR_XML_FILELISTS_NS"\" packages=\"%d\">\n"
#define XML_OTHER_HEADER        XML_HEADER"<otherdata xmlns=\"" \
                                CR_XML_OTHER_NS"\" packages=\"%d\">\n"
#define XML_PRESTODELTA_HEADER  XML_HEADER"<prestodelta>\n"
#define XML_UPDATEINFO_HEADER   XML_HEADER"<updates>\n"

#define XML_MAX_HEADER_SIZE     300
#define XML_RECOMPRESS_BUFFER_SIZE   8192

#define XML_PRIMARY_FOOTER      "</metadata>"
#define XML_FILELISTS_FOOTER    "</filelists>"
#define XML_OTHER_FOOTER        "</otherdata>"
#define XML_PRESTODELTA_FOOTER  "</prestodelta>"
#define XML_UPDATEINFO_FOOTER   "</updates>"

cr_XmlFile *
cr_xmlfile_sopen(const char *filename,
                 cr_XmlFileType type,
                 cr_CompressionType comtype,
                 cr_ContentStat *stat,
                 GError **err)
{
    cr_XmlFile *f;
    GError *tmp_err = NULL;

    assert(filename);
    assert(type < CR_XMLFILE_SENTINEL);
    assert(comtype < CR_CW_COMPRESSION_SENTINEL);
    assert(!err || *err == NULL);

    if (g_file_test(filename, G_FILE_TEST_EXISTS)) {
        g_set_error(err, ERR_DOMAIN, CRE_EXISTS,
                    "File already exists");
        return NULL;
    }

    CR_FILE *cr_f = cr_sopen(filename,
                             CR_CW_MODE_WRITE,
                             comtype,
                             stat,
                             &tmp_err);
    if (!cr_f) {
        g_propagate_prefixed_error(err, tmp_err, "Cannot open %s: ", filename);
        return NULL;
    }

    f = g_new0(cr_XmlFile, 1);
    f->f      = cr_f;
    f->type   = type;
    f->header = 0;
    f->footer = 0;
    f->pkgs   = 0;

    return f;
}

int
cr_xmlfile_set_num_of_pkgs(cr_XmlFile *f, long num, GError **err)
{
    assert(f);
    assert(!err || *err == NULL);

    if (f->header != 0) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Header was already written");
        return CRE_BADARG;
    }

    if (num < 0) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "The number must be a positive integer number");
        return CRE_BADARG;
    }

    f->pkgs = num;
    return CRE_OK;
}

int
cr_xmlfile_write_xml_header(cr_XmlFile *f, GError **err)
{
    const char *xml_header;
    GError *tmp_err = NULL;

    assert(f);
    assert(!err || *err == NULL);
    assert(f->header == 0);

    switch (f->type) {
    case CR_XMLFILE_PRIMARY:
        xml_header = XML_PRIMARY_HEADER;
        break;
    case CR_XMLFILE_FILELISTS:
        xml_header = XML_FILELISTS_HEADER;
        break;
    case CR_XMLFILE_OTHER:
        xml_header = XML_OTHER_HEADER;
        break;
    case CR_XMLFILE_PRESTODELTA:
        xml_header = XML_PRESTODELTA_HEADER;
        break;
    case CR_XMLFILE_UPDATEINFO:
        xml_header = XML_UPDATEINFO_HEADER;
        break;
    default:
        g_critical("%s: Bad file type", __func__);
        assert(0);
        g_set_error(err, ERR_DOMAIN, CRE_ASSERT, "Bad file type");
        return CRE_ASSERT;
    }

    if (cr_printf(&tmp_err, f->f, xml_header, f->pkgs) == CR_CW_ERR) {
        int code = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Cannot write XML header: ");
        return code;
    }

    f->header = 1;

    return cr_end_chunk(f->f, err);
}

int
cr_xmlfile_write_xml_footer(cr_XmlFile *f, GError **err)
{
    const char *xml_footer;
    GError *tmp_err = NULL;

    assert(f);
    assert(!err || *err == NULL);
    assert(f->footer == 0);

    switch (f->type) {
    case CR_XMLFILE_PRIMARY:
        xml_footer = XML_PRIMARY_FOOTER;
        break;
    case CR_XMLFILE_FILELISTS:
        xml_footer = XML_FILELISTS_FOOTER;
        break;
    case CR_XMLFILE_OTHER:
        xml_footer = XML_OTHER_FOOTER;
        break;
    case CR_XMLFILE_PRESTODELTA:
        xml_footer = XML_PRESTODELTA_FOOTER;
        break;
    case CR_XMLFILE_UPDATEINFO:
        xml_footer = XML_UPDATEINFO_FOOTER;
        break;
    default:
        g_critical("%s: Bad file type", __func__);
        assert(0);
        g_set_error(err, ERR_DOMAIN, CRE_ASSERT, "Bad file type");
        return CRE_ASSERT;
    }

    cr_puts(f->f, xml_footer, &tmp_err);
    if (tmp_err) {
        int code = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Cannot write XML footer: ");
        return code;
    }

    f->footer = 1;

    return CRE_OK;
}

int
cr_xmlfile_add_pkg(cr_XmlFile *f, cr_Package *pkg, GError **err)
{
    char *xml;
    GError *tmp_err = NULL;

    assert(f);
    assert(pkg);
    assert(!err || *err == NULL);
    assert(f->footer == 0);

    switch (f->type) {
    case CR_XMLFILE_PRIMARY:
        xml = cr_xml_dump_primary(pkg, &tmp_err);
        break;
    case CR_XMLFILE_FILELISTS:
        xml = cr_xml_dump_filelists(pkg, &tmp_err);
        break;
    case CR_XMLFILE_OTHER:
        xml = cr_xml_dump_other(pkg, &tmp_err);
        break;
    default:
        g_critical("%s: Bad file type", __func__);
        assert(0);
        g_set_error(err, ERR_DOMAIN, CRE_ASSERT, "Bad file type");
        return CRE_ASSERT;
    }

    if (tmp_err) {
        int code = tmp_err->code;
        g_propagate_error(err, tmp_err);
        if (xml)
            g_free(xml);
        return code;
    }

    if (xml) {
        cr_xmlfile_add_chunk(f, xml, &tmp_err);
        g_free(xml);

        if (tmp_err) {
            int code = tmp_err->code;
            g_propagate_error(err, tmp_err);
            return code;
        }
    }

    return CRE_OK;
}

int
cr_xmlfile_add_chunk(cr_XmlFile *f, const char* chunk, GError **err)
{
    GError *tmp_err = NULL;

    assert(f);
    assert(!err || *err == NULL);
    assert(f->footer == 0);

    if (!chunk)
        return CRE_OK;

    if (f->header == 0) {
        cr_xmlfile_write_xml_header(f, &tmp_err);
        if (tmp_err) {
            int code = tmp_err->code;
            g_propagate_error(err, tmp_err);
            return code;
        }
    }

    cr_puts(f->f, chunk, &tmp_err);
    if (tmp_err) {
        int code = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Error while write: ");
        return code;
    }

    return CRE_OK;
}

int
cr_xmlfile_close(cr_XmlFile *f, GError **err)
{
    GError *tmp_err = NULL;

    assert(!err || *err == NULL);

    if (!f)
        return CRE_OK;

    if (f->header == 0) {
        cr_xmlfile_write_xml_header(f, &tmp_err);
        if (tmp_err) {
            int code = tmp_err->code;
            g_propagate_error(err, tmp_err);
            return code;
        }
    }

    if (f->footer == 0) {
        cr_xmlfile_write_xml_footer(f, &tmp_err);
        if (tmp_err) {
            int code = tmp_err->code;
            g_propagate_error(err, tmp_err);
            return code;
        }
    }

    cr_close(f->f, &tmp_err);
    if (tmp_err) {
        int code = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err,
                "Error while closing a file: ");
        return code;
    }

    g_free(f);

    return CRE_OK;
}

static int 
write_modified_header(int task_count,
                      int package_count,
                      cr_XmlFile *cr_file,
                      gchar *header_buf,
                      int header_len,
                      GError **err)
{
    GError *tmp_err = NULL;
    gchar *package_count_string;
    gchar *task_count_string;
    int bytes_written = 0;
    int package_count_string_len = rasprintf(&package_count_string, "packages=\"%i\"", package_count);
    int task_count_string_len = rasprintf(&task_count_string, "packages=\"%i\"", task_count);

    gchar *pointer_to_pkgs = g_strstr_len(header_buf, header_len, task_count_string);
    if (!pointer_to_pkgs){
        g_free(package_count_string);
        g_free(task_count_string);
        return 0;
    }
    gchar *pointer_to_pkgs_end = pointer_to_pkgs + task_count_string_len;

    bytes_written += cr_write(cr_file->f, header_buf, pointer_to_pkgs - header_buf, &tmp_err);
    if (!tmp_err)
        bytes_written += cr_write(cr_file->f, package_count_string, package_count_string_len, &tmp_err);
    if (!tmp_err)
        bytes_written += cr_write(cr_file->f, pointer_to_pkgs_end, header_len - (pointer_to_pkgs_end - header_buf), &tmp_err);
    if (tmp_err) {
        g_propagate_prefixed_error(err, tmp_err, "Error encountered while writing header part:");
        g_free(package_count_string);
        g_free(task_count_string);
        return 0;
    }
    g_free(package_count_string);
    g_free(task_count_string);
    return bytes_written;
}

void
cr_rewrite_header_package_count(gchar *original_filename,
                                cr_CompressionType xml_compression,
                                int package_count,
                                int task_count,
                                cr_ContentStat *file_stat,
                                gchar *zck_dict_file,
                                GError **err)
{
    GError *tmp_err = NULL;
    CR_FILE *original_file = cr_open(original_filename, CR_CW_MODE_READ, CR_CW_AUTO_DETECT_COMPRESSION, &tmp_err);
    if (tmp_err) {
        g_propagate_prefixed_error(err, tmp_err, "Error encountered while reopening for reading:");
        return;
    }

    gchar *tmp_xml_filename = g_strconcat(original_filename, ".tmp", NULL);
    cr_XmlFile *new_file = cr_xmlfile_sopen_primary(tmp_xml_filename,
                                                    xml_compression,
                                                    file_stat,
                                                    &tmp_err);
    if (tmp_err) {
        g_propagate_prefixed_error(err, tmp_err, "Error encountered while opening for writing:");
        cr_close(original_file, NULL); 
        g_free(tmp_xml_filename);
        cr_xmlfile_close(new_file, NULL);
        return;
    }

    // We want to keep identical zchunk chunk sizes, therefore we copy by chunk
    if (xml_compression == CR_CW_ZCK_COMPRESSION) {
        if (zck_dict_file){
            gchar *zck_dict = NULL;
            size_t zck_dict_size = 0;
            if (g_file_get_contents(zck_dict_file, &zck_dict, &zck_dict_size, &tmp_err)){
                cr_set_dict(new_file->f, zck_dict, zck_dict_size, &tmp_err);
            } else {
                g_propagate_prefixed_error(err, tmp_err, "Error encountered setting zck dict:");
                cr_xmlfile_close(new_file, NULL);
                cr_close(original_file, NULL);
                g_free(tmp_xml_filename);
                return;
            }
        }

        char *copy_buf = NULL;
        // Chunk with index 0 is dictionary, data (xml metadata and our header) starts at 1 
        ssize_t zchunk_index = 1;
        ssize_t len_read = cr_get_zchunk_with_index(original_file, zchunk_index, &copy_buf, &tmp_err);
        if (!tmp_err)
            write_modified_header(task_count, package_count, new_file, copy_buf, len_read, &tmp_err);
        if (tmp_err){
            g_propagate_prefixed_error(err, tmp_err, "Error encountered while recompressing:");
            cr_xmlfile_close(new_file, NULL);
            cr_close(original_file, NULL);
            g_free(tmp_xml_filename);
            return;
        }
        zchunk_index++;
        while(len_read){
            g_free(copy_buf);
            len_read = cr_get_zchunk_with_index(original_file, zchunk_index, &copy_buf, &tmp_err);
            if (!tmp_err)
                cr_write(new_file->f, copy_buf, len_read, &tmp_err);
            if (!tmp_err)
                cr_end_chunk(new_file->f, &tmp_err);
            if (tmp_err) {
                g_propagate_prefixed_error(err, tmp_err, "Error encountered while recompressing:");
                cr_xmlfile_close(new_file, NULL);
                cr_close(original_file, NULL); 
                g_free(tmp_xml_filename);
                return;
            }
            zchunk_index++;
        }
    } else {
        gchar header_buf[XML_MAX_HEADER_SIZE];
        int len_read = cr_read(original_file, header_buf, XML_MAX_HEADER_SIZE, &tmp_err);
        if (!tmp_err)
            write_modified_header(task_count, package_count, new_file, header_buf, len_read, &tmp_err);
        if (tmp_err) {
            g_propagate_prefixed_error(err, tmp_err, "Error encountered while recompressing:");
            cr_xmlfile_close(new_file, NULL);
            cr_close(original_file, NULL); 
            g_free(tmp_xml_filename);
            return;
        }
        //Copy the rest of the file
        gchar copy_buf[XML_RECOMPRESS_BUFFER_SIZE];
        while(len_read)
        {
            len_read = cr_read(original_file, copy_buf, XML_RECOMPRESS_BUFFER_SIZE, &tmp_err);
            if (!tmp_err)
                cr_write(new_file->f, copy_buf, len_read, &tmp_err);
            if (tmp_err) {
                g_propagate_prefixed_error(err, tmp_err, "Error encountered while recompressing:");
                cr_xmlfile_close(new_file, NULL);
                cr_close(original_file, NULL); 
                g_free(tmp_xml_filename);
                return;
            }
        }
    }

    new_file->header = 1;
    new_file->footer = 1;

    cr_xmlfile_close(new_file, &tmp_err);
    if (tmp_err) {
        g_propagate_prefixed_error(err, tmp_err, "Error encountered while writing:");
        cr_close(original_file, NULL); 
        g_free(tmp_xml_filename);
        return;
    }
    cr_close(original_file, &tmp_err); 
    if (tmp_err) {
        g_propagate_prefixed_error(err, tmp_err, "Error encountered while writing:");
        g_free(tmp_xml_filename);
        return;
    }

    if (g_rename(tmp_xml_filename, original_filename) == -1) {
        g_propagate_prefixed_error(err, tmp_err, "Error encountered while renaming:");
        g_free(tmp_xml_filename);
        return;
    }
    g_free(tmp_xml_filename);
}
