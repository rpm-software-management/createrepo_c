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
#include "xml_parser_internal.h"
#include "xml_parser.h"
#include "error.h"
#include "package.h"
#include "misc.h"

#define ERR_DOMAIN      CREATEREPO_C_ERROR
#define ERR_CODE_XML    CRE_BADXMLREPOMD

typedef enum {
    STATE_START,
    STATE_REPOMD,
    STATE_REVISION,
    STATE_REPOID,
    STATE_CONTENTHASH,
    STATE_TAGS,
    STATE_REPO,
    STATE_CONTENT,
    STATE_DISTRO,
    STATE_DATA,
    STATE_LOCATION,
    STATE_CHECKSUM,
    STATE_OPENCHECKSUM,
    STATE_HEADERCHECKSUM,
    STATE_TIMESTAMP,
    STATE_SIZE,
    STATE_OPENSIZE,
    STATE_HEADERSIZE,
    STATE_DBVERSION,
    NUMSTATES
} cr_RepomdState;

/* NOTE: Same states in the first column must be together!!!
 * Performance tip: More frequent elements should be listed
 * first in its group (eg: element "package" (STATE_PACKAGE)
 * has a "file" element listed first, because it is more frequent
 * than a "version" element). */
static cr_StatesSwitch stateswitches[] = {
    { STATE_START,      "repomd",              STATE_REPOMD,         0 },
    { STATE_REPOMD,     "revision",            STATE_REVISION,       1 },
    { STATE_REPOMD,     "repoid",              STATE_REPOID,         1 },
    { STATE_REPOMD,     "contenthash",         STATE_CONTENTHASH,    1 },
    { STATE_REPOMD,     "tags",                STATE_TAGS,           0 },
    { STATE_REPOMD,     "data",                STATE_DATA,           0 },
    { STATE_TAGS,       "repo",                STATE_REPO,           1 },
    { STATE_TAGS,       "content",             STATE_CONTENT,        1 },
    { STATE_TAGS,       "distro",              STATE_DISTRO,         1 },
    { STATE_DATA,       "location",            STATE_LOCATION,       0 },
    { STATE_DATA,       "checksum",            STATE_CHECKSUM,       1 },
    { STATE_DATA,       "open-checksum",       STATE_OPENCHECKSUM,   1 },
    { STATE_DATA,       "header-checksum",     STATE_HEADERCHECKSUM, 1 },
    { STATE_DATA,       "timestamp",           STATE_TIMESTAMP,      1 },
    { STATE_DATA,       "size",                STATE_SIZE,           1 },
    { STATE_DATA,       "open-size",           STATE_OPENSIZE,       1 },
    { STATE_DATA,       "header-size",         STATE_HEADERSIZE,     1 },
    { STATE_DATA,       "database_version",    STATE_DBVERSION,      1 },
    { NUMSTATES,        NULL, NUMSTATES, 0 }
};

static void
cr_start_handler(void *pdata, const xmlChar *element, const xmlChar **attr)
{
    cr_ParserData *pd = pdata;
    cr_StatesSwitch *sw;

    if (pd->err)
        return;  // There was an error -> do nothing

    if (pd->depth != pd->statedepth) {
        // We are inside of unknown element
        pd->depth++;
        return;
    }
    pd->depth++;

    if (!pd->swtab[pd->state]) {
        // Current element should not have any sub elements
        return;
    }

    // Find current state by its name
    for (sw = pd->swtab[pd->state]; sw->from == pd->state; sw++)
        if (!strcmp((char *) element, sw->ename))
            break;
    if (sw->from != pd->state) {
        // No state for current element (unknown element)
        cr_xml_parser_warning(pd, CR_XML_WARNING_UNKNOWNTAG,
                              "Unknown element \"%s\"", element);
        return;
    }

    // Update parser data
    pd->state      = sw->to;
    pd->docontent  = sw->docontent;
    pd->statedepth = pd->depth;
    pd->lcontent   = 0;
    pd->content[0] = '\0';

    const char *val;

    switch(pd->state) {
    case STATE_START:
        break;

    case STATE_REPOMD:
        pd->main_tag_found = TRUE;
        break;

    case STATE_REVISION:
    case STATE_TAGS:
    case STATE_REPO:
    case STATE_CONTENT:
        break;

    case STATE_REPOID:
        assert(pd->repomd);
        assert(!pd->repomdrecord);

        val = cr_find_attr("type", attr);
        if (val)
            pd->repomd->repoid_type = g_string_chunk_insert(pd->repomd->chunk,
                                                            val);
        break;

    case STATE_CONTENTHASH:
        assert(pd->repomd);
        assert(!pd->repomdrecord);

        val = cr_find_attr("type", attr);
        if (val)
            pd->repomd->contenthash_type = g_string_chunk_insert(
                                                    pd->repomd->chunk, val);
        break;

    case STATE_DISTRO:
        assert(pd->repomd);
        assert(!pd->repomdrecord);

        val = cr_find_attr("cpeid", attr);
        if (val)
            pd->cpeid = g_strdup(val);
        break;

    case STATE_DATA:
        assert(pd->repomd);
        assert(!pd->repomdrecord);

        val = cr_find_attr("type", attr);
        if (!val) {
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                           "Missing attribute \"type\" of a data element");
            val = "unknown";
        }

        pd->repomdrecord = cr_repomd_record_new(val, NULL);
        cr_repomd_set_record(pd->repomd, pd->repomdrecord);
        break;

    case STATE_LOCATION:
        assert(pd->repomd);
        assert(pd->repomdrecord);

        val = cr_find_attr("href", attr);
        if (val)
            pd->repomdrecord->location_href = g_string_chunk_insert(
                                                    pd->repomdrecord->chunk,
                                                    val);
        else
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                    "Missing attribute \"href\" of a location element");

        val = cr_find_attr("xml:base", attr);
        if (val)
            pd->repomdrecord->location_base = g_string_chunk_insert(
                                                    pd->repomdrecord->chunk,
                                                    val);

        break;

    case STATE_CHECKSUM:
        assert(pd->repomd);
        assert(pd->repomdrecord);

        val = cr_find_attr("type", attr);
        if (!val) {
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                    "Missing attribute \"type\" of a checksum element");
            break;
        }

        pd->repomdrecord->checksum_type = g_string_chunk_insert(
                                                    pd->repomdrecord->chunk,
                                                    val);
        break;

    case STATE_OPENCHECKSUM:
        assert(pd->repomd);
        assert(pd->repomdrecord);

        val = cr_find_attr("type", attr);
        if (!val) {
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                    "Missing attribute \"type\" of an open checksum element");
            break;
        }

        pd->repomdrecord->checksum_open_type = g_string_chunk_insert(
                                                    pd->repomdrecord->chunk,
                                                    val);
        break;

    case STATE_HEADERCHECKSUM:
        assert(pd->repomd);
        assert(pd->repomdrecord);

        val = cr_find_attr("type", attr);
        if (!val) {
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                    "Missing attribute \"type\" of a header checksum element");
            break;
        }

        pd->repomdrecord->checksum_header_type = g_string_chunk_insert(
                                                    pd->repomdrecord->chunk,
                                                    val);
        break;

    case STATE_TIMESTAMP:
    case STATE_SIZE:
    case STATE_OPENSIZE:
    case STATE_HEADERSIZE:
    case STATE_DBVERSION:
    default:
        break;
    }
}

static void
cr_end_handler(void *pdata, G_GNUC_UNUSED const xmlChar *element)
{
    cr_ParserData *pd = pdata;
    unsigned int state = pd->state;

    if (pd->err)
        return; // There was an error -> do nothing

    if (pd->depth != pd->statedepth) {
        // Back from the unknown state
        pd->depth--;
        return;
    }

    pd->depth--;
    pd->statedepth--;
    pd->state = pd->sbtab[pd->state];
    pd->docontent = 0;

    switch (state) {
    case STATE_START:
    case STATE_REPOMD:
        break;

    case STATE_REVISION:
        assert(pd->repomd);
        assert(!pd->repomdrecord);

        if (pd->lcontent == 0) {
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGVAL,
                    "Missing value of a revision element");
            break;
        }

        cr_repomd_set_revision(pd->repomd, pd->content);
        break;

    case STATE_REPOID:
        assert(pd->repomd);
        assert(!pd->repomdrecord);

        pd->repomd->repoid = g_string_chunk_insert(pd->repomd->chunk,
                                                   pd->content);
        break;

    case STATE_CONTENTHASH:
        assert(pd->repomd);
        assert(!pd->repomdrecord);

        pd->repomd->contenthash = g_string_chunk_insert(pd->repomd->chunk,
                                                        pd->content);
        break;

    case STATE_TAGS:
        break;

    case STATE_REPO:
        assert(pd->repomd);
        assert(!pd->repomdrecord);

        cr_repomd_add_repo_tag(pd->repomd, pd->content);
        break;

    case STATE_CONTENT:
        assert(pd->repomd);
        assert(!pd->repomdrecord);

        cr_repomd_add_content_tag(pd->repomd, pd->content);
        break;

    case STATE_DISTRO:
        assert(pd->repomd);
        assert(!pd->repomdrecord);

        cr_repomd_add_distro_tag(pd->repomd, pd->cpeid, pd->content);
        if (pd->cpeid) {
            g_free(pd->cpeid);
            pd->cpeid = NULL;
        }
        break;

    case STATE_DATA:
        assert(pd->repomd);
        assert(pd->repomdrecord);

        pd->repomdrecord = NULL;
        break;

    case STATE_LOCATION:
        break;

    case STATE_CHECKSUM:
        assert(pd->repomd);
        assert(pd->repomdrecord);

        pd->repomdrecord->checksum = cr_safe_string_chunk_insert(
                                            pd->repomdrecord->chunk,
                                            pd->content);
        break;

    case STATE_OPENCHECKSUM:
        assert(pd->repomd);
        assert(pd->repomdrecord);

        pd->repomdrecord->checksum_open = cr_safe_string_chunk_insert(
                                            pd->repomdrecord->chunk,
                                            pd->content);
        break;

    case STATE_HEADERCHECKSUM:
        assert(pd->repomd);
        assert(pd->repomdrecord);

        pd->repomdrecord->checksum_header = cr_safe_string_chunk_insert(
                                            pd->repomdrecord->chunk,
                                            pd->content);
        break;

    case STATE_TIMESTAMP:
        assert(pd->repomd);
        assert(pd->repomdrecord);

        pd->repomdrecord->timestamp = cr_xml_parser_strtoll(pd, pd->content, 0);
        break;

    case STATE_SIZE:
        assert(pd->repomd);
        assert(pd->repomdrecord);

        pd->repomdrecord->size = cr_xml_parser_strtoll(pd, pd->content, 0);
        break;

    case STATE_OPENSIZE:
        assert(pd->repomd);
        assert(pd->repomdrecord);

        pd->repomdrecord->size_open = cr_xml_parser_strtoll(pd, pd->content, 0);
        break;

    case STATE_HEADERSIZE:
        assert(pd->repomd);
        assert(pd->repomdrecord);

        pd->repomdrecord->size_header = cr_xml_parser_strtoll(pd, pd->content, 0);
        break;

    case STATE_DBVERSION:
        assert(pd->repomd);
        assert(pd->repomdrecord);

        pd->repomdrecord->db_ver = (int) cr_xml_parser_strtoll(pd,
                                                               pd->content,
                                                               0);
        break;

    default:
        break;
    }
}

int
cr_xml_parse_repomd(const char *path,
                    cr_Repomd *repomd,
                    cr_XmlParserWarningCb warningcb,
                    void *warningcb_data,
                    GError **err)
{
    int ret = CRE_OK;
    cr_ParserData *pd;
    GError *tmp_err = NULL;

    assert(path);
    assert(repomd);
    assert(!err || *err == NULL);

    // Init
    xmlSAXHandler sax;
    memset(&sax, 0, sizeof(sax));
    sax.startElement = cr_start_handler;
    sax.endElement = cr_end_handler;
    sax.characters = cr_char_handler;

    pd = cr_xml_parser_data(NUMSTATES);

    xmlParserCtxtPtr parser;
    parser = xmlCreatePushParserCtxt(&sax, pd, NULL, 0, NULL);

    pd->parser = parser;
    pd->state = STATE_START;
    pd->repomd = repomd;
    pd->warningcb = warningcb;
    pd->warningcb_data = warningcb_data;
    for (cr_StatesSwitch *sw = stateswitches; sw->from != NUMSTATES; sw++) {
        if (!pd->swtab[sw->from])
            pd->swtab[sw->from] = sw;
        pd->sbtab[sw->to] = sw->from;
    }

    // Parsing

    ret = cr_xml_parser_generic(parser, pd, path, &tmp_err);
    if (tmp_err)
        g_propagate_error(err, tmp_err);

    // Warning if file was probably a different type than expected

    if (!pd->main_tag_found && ret == CRE_OK)
        cr_xml_parser_warning(pd, CR_XML_WARNING_BADMDTYPE,
                          "The file don't contain the expected element "
                          "\"<repomd>\" - The file probably isn't "
                          "a valid repomd.xml");

    // Clean up

    cr_xml_parser_data_free(pd);

    return ret;
}
