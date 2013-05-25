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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <expat.h>
#include "xml_parser_internal.h"
#include "xml_parser.h"
#include "error.h"
#include "package.h"
#include "logging.h"
#include "misc.h"

typedef enum {
    STATE_START,
    STATE_FILELISTS,
    STATE_PACKAGE,
    STATE_VERSION,
    STATE_FILE,
    NUMSTATES,
} cr_FilState;

/* NOTE: Same states in the first column must be together!!!
 * Performance tip: More frequent elements shoud be listed
 * first in its group (eg: element "package" (STATE_PACKAGE)
 * has a "file" element listed first, because it is more frequent
 * than a "version" element). */
static cr_StatesSwitch stateswitches[] = {
    { STATE_START,      "filelists",    STATE_FILELISTS,    0 },
    { STATE_FILELISTS,  "package",      STATE_PACKAGE,      0 },
    { STATE_PACKAGE,    "file",         STATE_FILE,         1 },
    { STATE_PACKAGE,    "version",      STATE_VERSION,      0 },
    { NUMSTATES,        NULL,           NUMSTATES,          0 },
};

static void XMLCALL
cr_start_handler(void *pdata, const char *element, const char **attr)
{
    GError *tmp_err = NULL;
    cr_ParserData *pd = pdata;
    cr_StatesSwitch *sw;

    if (pd->ret != CRE_OK)
        return;  // There was an error -> do nothing

    if (pd->depth != pd->statedepth) {
        // There probably was an unknown element
        pd->depth++;
        return;
    }
    pd->depth++;

    if (!pd->swtab[pd->state])
         return;  // Current element should not have any sub elements

    /* TODO TEST THIS */
    if (!pd->pkg && pd->state != STATE_FILELISTS && pd->state != STATE_START)
        return;  // Do not parse current package tag and its content

    // Find current state by its name
    for (sw = pd->swtab[pd->state]; sw->from == pd->state; sw++)
        if (!strcmp(element, sw->ename))
            break;
    if (sw->from != pd->state)
      return; // There is no state for the name -> skip

    // Update parser data
    pd->state      = sw->to;
    pd->docontent  = sw->docontent;
    pd->statedepth = pd->depth;
    pd->lcontent   = 0;
    pd->content[0] = '\0';

    switch(pd->state) {
    case STATE_START:
    case STATE_FILELISTS:
        break;

    case STATE_PACKAGE: {
        /* TODO: Parse all attrs in single loop instead of use cr_find_attr */
        const char *pkgId = cr_find_attr("pkgid", attr);
        const char *name  = cr_find_attr("name", attr);
        const char *arch  = cr_find_attr("arch", attr);

        if (!pkgId) {
            pd->ret = CRE_BADXMLFILELISTS;
            g_set_error(pd->err, CR_XML_PARSER_FIL_ERROR, CRE_BADXMLFILELISTS,
                        "Package pkgid attributte is missing!");
            break;
        }

        if (pd->newpkgcb(&pd->pkg,
                         pkgId,
                         name,
                         arch,
                         pd->newpkgcb_data,
                         &tmp_err))
        {
            pd->ret = CRE_CBINTERRUPTED;
            if (tmp_err)
                g_propagate_prefixed_error(pd->err,
                                           tmp_err,
                                           "Parsing interrupted:");
            else
                g_set_error(pd->err, CR_XML_PARSER_FIL_ERROR, CRE_CBINTERRUPTED,
                            "Parsing interrupted");
        }

        /* TODO: Insert name and pkg id to the package */
        break;
    }

    case STATE_VERSION:
        /* TODO: Parse version */
        break;

    case STATE_FILE: {
        const char *type = cr_find_attr("type", attr);
        pd->last_file_type = FILE_FILE;
        if (type) {
            if (!strcmp(type, "dir"))
                pd->last_file_type = FILE_DIR;
            else if (!strcmp(type, "ghost"))
                pd->last_file_type = FILE_GHOST;
            else
                g_string_append_printf(pd->msgs,
                                       "Unknown file type \"%s\";",
                                       type);
        }
        break;
    }

    default:
        break;
    }
}

static void XMLCALL
cr_end_handler(void *pdata, const char *element)
{
    cr_ParserData *pd = pdata;
    GError *tmp_err = NULL;
    unsigned int state = pd->state;

    CR_UNUSED(element);

    if (pd->ret != CRE_OK)
        return; /* There was an error -> do nothing */

    if (pd->depth != pd->statedepth) {
        /* Back from the unknown state */
        pd->depth--;
        return;
    }

    pd->depth--;
    pd->statedepth--;
    pd->state = pd->sbtab[pd->state];
    pd->docontent = 0;

    switch (state) {
    case STATE_START:
    case STATE_FILELISTS:
    case STATE_VERSION:
        break;

    case STATE_PACKAGE:
        if (!pd->pkg)
            return;

        if (pd->pkgcb(pd->pkg, pd->pkgcb_data, &tmp_err)) {
            pd->ret = CRE_CBINTERRUPTED;
            if (tmp_err)
                g_propagate_prefixed_error(pd->err,
                                           tmp_err,
                                           "Parsing interrupted:");
            else
                g_set_error(pd->err, CR_XML_PARSER_FIL_ERROR, CRE_CBINTERRUPTED,
                            "Parsing interrupted");
        }
        pd->pkg = NULL;
        break;

    case STATE_FILE: {
        if (!pd->pkg || !pd->content)
            break;

        cr_PackageFile *pkg_file = cr_package_file_new();
        pkg_file->name = cr_safe_string_chunk_insert(pd->pkg->chunk,
                                                cr_get_filename(pd->content));
        pkg_file->path = cr_safe_string_chunk_insert(pd->pkg->chunk,
                                                     pd->content);
        switch (pd->last_file_type) {
            case FILE_FILE:  pkg_file->type = NULL;    break; // NULL => "file"
            case FILE_DIR:   pkg_file->type = "dir";   break;
            case FILE_GHOST: pkg_file->type = "ghost"; break;
        }

        pd->pkg->files = g_slist_prepend(pd->pkg->files, pkg_file);
        break;
    }

    default:
        break;
    }
}

int
cr_xml_parse_filelists(const char *path,
                       cr_XmlParserNewPkgCb newpkgcb,
                       void *newpkgcb_data,
                       cr_XmlParserPkgCb pkgcb,
                       void *pkgcb_data,
                       GError **err)
{
    int ret = CRE_OK;
    CR_FILE *f;
    cr_ParserData *pd;
    XML_Parser parser;

    assert(path);
    assert(pkgcb);
    assert(!err || *err == NULL);

    if (!newpkgcb)
        newpkgcb = cr_newpkgcb;

    f = cr_open(path, CR_CW_MODE_READ, CR_CW_AUTO_DETECT_COMPRESSION);
    if (!f) {
        g_set_error(err, CR_XML_PARSER_FIL_ERROR, CRE_IO, "Cannot open %s", path);
        return CRE_IO;
    }

    parser = XML_ParserCreate(NULL);
    XML_SetElementHandler(parser, cr_start_handler, cr_end_handler);
    XML_SetCharacterDataHandler(parser, cr_char_handler);

    pd = cr_xml_parser_data();
    pd->parser = &parser;
    pd->state = STATE_START;
    pd->newpkgcb_data = newpkgcb_data;
    pd->newpkgcb = newpkgcb;
    pd->pkgcb_data = pkgcb_data;
    pd->pkgcb = pkgcb;
    pd->swtab = g_malloc0(sizeof(cr_StatesSwitch *) * NUMSTATES);
    pd->sbtab = g_malloc(sizeof(cr_FilState) * NUMSTATES);
    for (cr_StatesSwitch *sw = stateswitches; sw->from != NUMSTATES; sw++) {
        if (!pd->swtab[sw->from])
            pd->swtab[sw->from] = sw;
        pd->sbtab[sw->to] = sw->from;
    }

    XML_SetUserData(parser, pd);

    while (1) {
        int len;
        void *buf = XML_GetBuffer(parser, XML_BUFFER_SIZE);
        if (!buf) {
            ret = CRE_MEMORY;
            g_set_error(err, CR_XML_PARSER_FIL_ERROR, CRE_MEMORY,
                        "Out of memory: Cannot allocate buffer for xml parser");
            break;
        }

        len = cr_read(f, buf, XML_BUFFER_SIZE);
        if (len < 0) {
            ret = CRE_IO;
            g_critical("%s: Cannot read for parsing : %s\n",
                       __func__, strerror(errno));
            g_set_error(err, CR_XML_PARSER_FIL_ERROR, CRE_IO,
                        "Error while reading xml");
            break;
        }

        if (!XML_ParseBuffer(parser, len, len == 0)) {
            ret = CRE_XMLPARSER;
            g_critical("%s: parsing error: %s\n",
                       __func__, XML_ErrorString(XML_GetErrorCode(parser)));
            g_set_error(err, CR_XML_PARSER_FIL_ERROR, CRE_XMLPARSER,
                        "Parse error at line: %d (%s)",
                        (int) XML_GetCurrentLineNumber(parser),
                        (char *) XML_ErrorString(XML_GetErrorCode(parser)));
            break;
        }

        if (len == 0)
            break;

        if (pd->ret != CRE_OK) {
            ret = pd->ret;
            break;
        }
    }

    cr_xml_parser_data_free(pd);
    XML_ParserFree(parser);

    return ret;
}
