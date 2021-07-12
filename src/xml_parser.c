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
#include <glib/gprintf.h>
#include <assert.h>
#include <errno.h>
#include "error.h"
#include "xml_parser.h"
#include "xml_parser_internal.h"
#include "misc.h"

#define ERR_DOMAIN      CREATEREPO_C_ERROR


cr_ParserData *
cr_xml_parser_data(unsigned int numstates)
{
    cr_ParserData *pd = g_new0(cr_ParserData, 1);
    pd->content = g_malloc(CONTENT_REALLOC_STEP);
    pd->acontent = CONTENT_REALLOC_STEP;
    pd->swtab = g_malloc0(sizeof(cr_StatesSwitch *) * numstates);
    pd->sbtab = g_malloc(sizeof(unsigned int) * numstates);

    return pd;
}

void
cr_xml_parser_data_free(cr_ParserData *pd)
{
    if (!pd) {
        return;
    }
    if (pd->parser) {
        xmlFreeParserCtxt(pd->parser);
    }
    g_free(pd->content);
    g_free(pd->swtab);
    g_free(pd->sbtab);
    g_free(pd);
}

void
cr_char_handler(void *pdata, const xmlChar *s, int len)
{
    int l;
    char *c;
    cr_ParserData *pd = pdata;

    if (pd->err)
        return; /* There was an error -> do nothing */

    if (!pd->docontent)
        return; /* Do not store the content */

    l = pd->lcontent + len + 1;
    if (l > pd->acontent) {
        pd->acontent = l + CONTENT_REALLOC_STEP;
        pd->content = realloc(pd->content, pd->acontent);
    }

    c = pd->content + pd->lcontent;
    pd->lcontent += len;
    while (len-- > 0)
        *c++ = *s++;
    *c = '\0';
}

int
cr_xml_parser_warning(cr_ParserData *pd,
                      cr_XmlParserWarningType type,
                      const char *msg,
                      ...)
{
    int ret;
    va_list args;
    char *warn;
    GError *tmp_err;

    assert(pd);
    assert(msg);

    if (!pd->warningcb)
        return CR_CB_RET_OK;

    va_start(args, msg);
    g_vasprintf(&warn, msg, args);
    va_end(args);

    tmp_err = NULL;
    ret = pd->warningcb(type, warn, pd->warningcb_data, &tmp_err);
    g_free(warn);
    if (ret != CR_CB_RET_OK) {
        if (tmp_err)
            g_propagate_prefixed_error(&pd->err, tmp_err,
                                       "Parsing interrupted: ");
        else
            g_set_error(&pd->err, ERR_DOMAIN, CRE_CBINTERRUPTED,
                        "Parsing interrupted");
    }


    assert(pd->err || ret == CR_CB_RET_OK);

    return ret;
}

gint64
cr_xml_parser_strtoll(cr_ParserData *pd,
                      const char *nptr,
                      unsigned int base)
{
    gint64 val;
    char *endptr = NULL;

    assert(pd);
    assert(base <= 36 && base != 1);

    if (!nptr)
        return 0;

    val = g_ascii_strtoll(nptr, &endptr, base);

    if ((val == G_MAXINT64 || val == G_MININT64) && errno == ERANGE)
        cr_xml_parser_warning(pd, CR_XML_WARNING_BADATTRVAL,
                "Correct integer value \"%s\" caused overflow", nptr);
    else if (val == 0 && *endptr != '\0')
        cr_xml_parser_warning(pd, CR_XML_WARNING_BADATTRVAL,
                "Conversion of \"%s\" to integer failed", nptr);

    return val;
}

int
cr_newpkgcb(cr_Package **pkg,
            G_GNUC_UNUSED const char *pkgId,
            G_GNUC_UNUSED const char *name,
            G_GNUC_UNUSED const char *arch,
            G_GNUC_UNUSED void *cbdata,
            GError **err)
{
    assert(pkg && *pkg == NULL);
    assert(!err || *err == NULL);

    *pkg = cr_package_new();

    return CR_CB_RET_OK;
}

int
cr_xml_parser_generic(xmlParserCtxtPtr parser,
                      cr_ParserData *pd,
                      const char *path,
                      GError **err)
{
    /* Note: This function uses .err members of cr_ParserData! */

    int ret = CRE_OK;
    CR_FILE *f;
    GError *tmp_err = NULL;
    char buf[XML_BUFFER_SIZE];

    assert(parser);
    assert(pd);
    assert(path);
    assert(!err || *err == NULL);

    f = cr_open(path, CR_CW_MODE_READ, CR_CW_AUTO_DETECT_COMPRESSION, &tmp_err);
    if (tmp_err) {
        int code = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Cannot open %s: ", path);
        return code;
    }

    while (1) {
        int len;
        len = cr_read(f, buf, XML_BUFFER_SIZE, &tmp_err);
        if (tmp_err) {
            ret = tmp_err->code;
            g_critical("%s: Error while reading xml '%s': %s",
                       __func__, path, tmp_err->message);
            g_propagate_prefixed_error(err, tmp_err, "Read error: ");
            break;
        }

        if (xmlParseChunk(parser, buf, len, len == 0)) {
            ret = CRE_XMLPARSER;
            xmlErrorPtr xml_err = xmlCtxtGetLastError(parser);
            g_critical("%s: parsing error '%s': %s",
                       __func__,
                       path,
                       xml_err->message);
            g_set_error(err, ERR_DOMAIN, CRE_XMLPARSER,
                        "Parse error '%s' at line: %d (%s)",
                        path,
                        (int) xml_err->line,
                        (char *) xml_err->message);
            break;
        }

        if (pd->err) {
            ret = pd->err->code;
            g_propagate_error(err, pd->err);
            break;
        }

        if (len == 0)
            break;
    }

    if (ret != CRE_OK) {
        // An error already encoutentered
        // just close the file without error checking
        cr_close(f, NULL);
    } else {
        // No error encountered yet
        cr_close(f, &tmp_err);
        if (tmp_err) {
            ret = tmp_err->code;
            g_propagate_prefixed_error(err, tmp_err, "Error while closing: ");
        }
    }

    return ret;
}

int
cr_xml_parser_generic_from_string(xmlParserCtxtPtr parser,
                                  cr_ParserData *pd,
                                  const char *xml_string,
                                  GError **err)
{
    /* Note: This function uses .err members of cr_ParserData! */

    int ret = CRE_OK;
    int block_size = XML_BUFFER_SIZE;
    const char *next_data = xml_string;
    const char *end_of_string = xml_string + strlen(xml_string);
    int finished = 0;

    assert(parser);
    assert(pd);
    assert(xml_string);
    assert(!err || *err == NULL);

    const char *data;
    while (!finished) {
        data = next_data;

        // Check if we are in the last loop
        next_data = data + block_size;
        if (next_data > end_of_string) {
            block_size = strlen(data);
            finished = 1;
        }

        if (xmlParseChunk(parser, data, block_size, finished)) {
            ret = CRE_XMLPARSER;
            xmlErrorPtr xml_err = xmlCtxtGetLastError(parser);
            g_critical("%s: parsing error '%s': %s",
                       __func__,
                       data,
                       xml_err->message);
            g_set_error(err, ERR_DOMAIN, CRE_XMLPARSER,
                        "Parse error '%s' at line: %d (%s)",
                        data,
                        (int) xml_err->line,
                        (char *) xml_err->message);
        }

        if (pd->err) {
            ret = pd->err->code;
            g_propagate_error(err, pd->err);
        }
    }

    return ret;
}
