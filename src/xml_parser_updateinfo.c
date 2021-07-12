/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2014  Tomas Mlcoch
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
#include "updateinfo.h"
#include "error.h"
#include "misc.h"

#define ERR_DOMAIN      CREATEREPO_C_ERROR
#define ERR_CODE_XML    CRE_BADXMLUPDATEINFO

typedef enum {
    STATE_START,
    STATE_UPDATES,
    STATE_UPDATE,           // <update> -----------------------------
    STATE_ID,
    STATE_TITLE,
    STATE_ISSUED,
    STATE_UPDATED,
    STATE_RIGHTS,
    STATE_RELEASE,
    STATE_PUSHCOUNT,
    STATE_SEVERITY,
    STATE_SUMMARY,
    STATE_DESCRIPTION,
    STATE_SOLUTION,
    STATE_MESSAGE,          // Not implemented
    STATE_REFERENCES,       // <references> -------------------------
    STATE_REFERENCE,
    STATE_PKGLIST,          // <pkglist> ----------------------------
    STATE_COLLECTION,
    STATE_NAME,
    STATE_MODULE,
    STATE_PACKAGE,
    STATE_FILENAME,
    STATE_SUM,
    STATE_UPDATERECORD_REBOOTSUGGESTED,
    STATE_REBOOTSUGGESTED,
    STATE_RESTARTSUGGESTED,
    STATE_RELOGINSUGGESTED,
    NUMSTATES,
} cr_UpdateinfoState;

/* NOTE: Same states in the first column must be together!!!
 * Performance tip: More frequent elements should be listed
 * first in its group (eg: element "package" (STATE_PACKAGE)
 * has a "file" element listed first, because it is more frequent
 * than a "version" element). */
static cr_StatesSwitch stateswitches[] = {
    { STATE_START,      "updates",           STATE_UPDATES,           0 },
    { STATE_UPDATES,    "update",            STATE_UPDATE,            0 },
    { STATE_UPDATE,     "id",                STATE_ID,                1 },
    { STATE_UPDATE,     "title",             STATE_TITLE,             1 },
    { STATE_UPDATE,     "issued",            STATE_ISSUED,            0 },
    { STATE_UPDATE,     "updated",           STATE_UPDATED,           0 },
    { STATE_UPDATE,     "rights",            STATE_RIGHTS,            1 },
    { STATE_UPDATE,     "release",           STATE_RELEASE,           1 },
    { STATE_UPDATE,     "pushcount",         STATE_PUSHCOUNT,         1 },
    { STATE_UPDATE,     "severity",          STATE_SEVERITY,          1 },
    { STATE_UPDATE,     "summary",           STATE_SUMMARY,           1 },
    { STATE_UPDATE,     "description",       STATE_DESCRIPTION,       1 },
    { STATE_UPDATE,     "solution",          STATE_SOLUTION,          1 },
    { STATE_UPDATE,     "message",           STATE_MESSAGE,           1 }, // NI
    { STATE_UPDATE,     "references",        STATE_REFERENCES,        0 },
    { STATE_UPDATE,     "pkglist",           STATE_PKGLIST,           0 },
    { STATE_UPDATE,     "reboot_suggested",  STATE_UPDATERECORD_REBOOTSUGGESTED,0 },
    { STATE_REFERENCES, "reference",         STATE_REFERENCE,         0 },
    { STATE_PKGLIST,    "collection",        STATE_COLLECTION,        0 },
    { STATE_COLLECTION, "package",           STATE_PACKAGE,           0 },
    { STATE_COLLECTION, "name",              STATE_NAME,              1 },
    { STATE_COLLECTION, "module",            STATE_MODULE,            0 },
    { STATE_PACKAGE,    "filename",          STATE_FILENAME,          1 },
    { STATE_PACKAGE,    "sum",               STATE_SUM,               1 },
    { STATE_PACKAGE,    "reboot_suggested",  STATE_REBOOTSUGGESTED,   0 },
    { STATE_PACKAGE,    "restart_suggested", STATE_RESTARTSUGGESTED,  0 },
    { STATE_PACKAGE,    "relogin_suggested", STATE_RELOGINSUGGESTED,  0 },
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

    // Shortcuts
    cr_UpdateRecord *rec = pd->updaterecord;
    cr_UpdateCollection *collection = pd->updatecollection;
    cr_UpdateCollectionPackage *package = pd->updatecollectionpackage;

    switch(pd->state) {
    case STATE_START:
        break;

    case STATE_UPDATES:
        pd->main_tag_found = TRUE;
        break;

    case STATE_ID:
    case STATE_TITLE:
    case STATE_RIGHTS:
    case STATE_RELEASE:
    case STATE_PUSHCOUNT:
    case STATE_SEVERITY:
    case STATE_SUMMARY:
    case STATE_DESCRIPTION:
    case STATE_SOLUTION:
    case STATE_NAME:
    case STATE_FILENAME:
    case STATE_REFERENCES:
    case STATE_PKGLIST:
    default:
        // All states which don't have attributes and no action is
        // required for them should be skipped
        break;

    case STATE_UPDATE:
        assert(pd->updateinfo);
        assert(!pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionmodule);
        assert(!pd->updatecollectionpackage);

        rec = cr_updaterecord_new();
        cr_updateinfo_apped_record(pd->updateinfo, rec);
        pd->updaterecord = rec;

        val = cr_find_attr("from", attr);
        if (val)
            rec->from = g_string_chunk_insert(rec->chunk, val);

        val = cr_find_attr("status", attr);
        if (val)
            rec->status = g_string_chunk_insert(rec->chunk, val);

        val = cr_find_attr("type", attr);
        if (val)
            rec->type = g_string_chunk_insert(rec->chunk, val);

        val = cr_find_attr("version", attr);
        if (val)
            rec->version = g_string_chunk_insert(rec->chunk, val);

        break;

    case STATE_ISSUED:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionmodule);
        assert(!pd->updatecollectionpackage);
        val = cr_find_attr("date", attr);
        if (val)
            rec->issued_date = g_string_chunk_insert(rec->chunk, val);
        break;

    case STATE_UPDATED:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionmodule);
        assert(!pd->updatecollectionpackage);
        val = cr_find_attr("date", attr);
        if (val)
            rec->updated_date = g_string_chunk_insert(rec->chunk, val);
        break;

    case STATE_REFERENCE: {
        cr_UpdateReference *ref;

        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionmodule);
        assert(!pd->updatecollectionpackage);

        ref = cr_updatereference_new();
        cr_updaterecord_append_reference(rec, ref);

        val = cr_find_attr("id", attr);
        if (val)
            ref->id = g_string_chunk_insert(ref->chunk, val);

        val = cr_find_attr("href", attr);
        if (val)
            ref->href = g_string_chunk_insert(ref->chunk, val);

        val = cr_find_attr("type", attr);
        if (val)
            ref->type = g_string_chunk_insert(ref->chunk, val);

        val = cr_find_attr("title", attr);
        if (val)
            ref->title = g_string_chunk_insert(ref->chunk, val);

        break;
    }

    case STATE_COLLECTION:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionmodule);
        assert(!pd->updatecollectionpackage);

        collection = cr_updatecollection_new();
        cr_updaterecord_append_collection(rec, collection);
        pd->updatecollection = collection;

        val = cr_find_attr("short", attr);
        if (val)
            collection->shortname = g_string_chunk_insert(collection->chunk, val);

        break;

    case STATE_MODULE:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(pd->updatecollection);
        assert(!pd->updatecollectionmodule);
        assert(!pd->updatecollectionpackage);

        cr_UpdateCollectionModule *module = cr_updatecollectionmodule_new();
        assert(module);

        if (module)
            collection->module = module;

        val = cr_find_attr("name", attr);
        if (val)
            module->name = g_string_chunk_insert(module->chunk, val);

        val = cr_find_attr("stream", attr);
        if (val)
            module->stream = g_string_chunk_insert(module->chunk, val);

        val = cr_find_attr("version", attr);
        if (val){
            gchar *endptr;
            errno = 0;
            module->version = strtoull(val, &endptr, 10);
            if ((errno == ERANGE && (module->version == ULLONG_MAX))
                 || (errno != 0 && module->version == 0)) {
                perror("strtoull error when parsing module version");
                module->version = 0;
            }
            if (endptr == val)
                module->version = 0;
        }

        val = cr_find_attr("context", attr);
        if (val)
            module->context = g_string_chunk_insert(module->chunk, val);

        val = cr_find_attr("arch", attr);
        if (val)
            module->arch = g_string_chunk_insert(module->chunk, val);

        break;

    case STATE_PACKAGE:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(pd->updatecollection);
        assert(!pd->updatecollectionpackage);

        package = cr_updatecollectionpackage_new();
        assert(package);

        cr_updatecollection_append_package(collection, package);
        pd->updatecollectionpackage = package;

        val = cr_find_attr("name", attr);
        if (val)
            package->name = g_string_chunk_insert(package->chunk, val);

        val = cr_find_attr("version", attr);
        if (val)
            package->version = g_string_chunk_insert(package->chunk, val);

        val = cr_find_attr("release", attr);
        if (val)
            package->release = g_string_chunk_insert(package->chunk, val);

        val = cr_find_attr("epoch", attr);
        if (val)
            package->epoch = g_string_chunk_insert(package->chunk, val);

        val = cr_find_attr("arch", attr);
        if (val)
            package->arch = g_string_chunk_insert(package->chunk, val);

        val = cr_find_attr("src", attr);
        if (val)
            package->src = g_string_chunk_insert(package->chunk, val);

        break;

    case STATE_SUM:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(pd->updatecollection);
        assert(pd->updatecollectionpackage);
        val = cr_find_attr("type", attr);
        if (val)
            package->sum_type = cr_checksum_type(val);
        break;

    case STATE_UPDATERECORD_REBOOTSUGGESTED:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        rec->reboot_suggested = TRUE;
        break;

    case STATE_REBOOTSUGGESTED:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(pd->updatecollection);
        assert(pd->updatecollectionpackage);
        package->reboot_suggested = TRUE;
        break;

    case STATE_RESTARTSUGGESTED:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(pd->updatecollection);
        assert(pd->updatecollectionpackage);
        package->restart_suggested = TRUE;
        break;

    case STATE_RELOGINSUGGESTED:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(pd->updatecollection);
        assert(pd->updatecollectionpackage);
        package->relogin_suggested = TRUE;
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

    // Shortcuts
    char *content = pd->content;
    cr_UpdateRecord *rec = pd->updaterecord;
    cr_UpdateCollection *col = pd->updatecollection;
    cr_UpdateCollectionPackage *package = pd->updatecollectionpackage;

    switch (state) {
    case STATE_START:
    case STATE_UPDATES:
    case STATE_ISSUED:
    case STATE_UPDATED:
    case STATE_REFERENCES:
    case STATE_REFERENCE:
    case STATE_MODULE:
    case STATE_PKGLIST:
    case STATE_REBOOTSUGGESTED:
    case STATE_RESTARTSUGGESTED:
    case STATE_RELOGINSUGGESTED:
    case STATE_UPDATERECORD_REBOOTSUGGESTED:
        // All elements with no text data and without need of any
        // post processing should go here
        break;

    case STATE_ID:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionpackage);
        rec->id = cr_safe_string_chunk_insert_null(rec->chunk, content);
        break;

    case STATE_TITLE:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionpackage);
        rec->title = cr_safe_string_chunk_insert_null(rec->chunk, content);
        break;

    case STATE_RIGHTS:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionpackage);
        rec->rights = cr_safe_string_chunk_insert_null(rec->chunk, content);
        break;

    case STATE_RELEASE:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionpackage);
        rec->release = cr_safe_string_chunk_insert_null(rec->chunk, content);
        break;

    case STATE_PUSHCOUNT:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionpackage);
        rec->pushcount = cr_safe_string_chunk_insert_null(rec->chunk, content);
        break;

    case STATE_SEVERITY:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionpackage);
        rec->severity = cr_safe_string_chunk_insert_null(rec->chunk, content);
        break;

    case STATE_SUMMARY:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionpackage);
        rec->summary = cr_safe_string_chunk_insert_null(rec->chunk, content);
        break;

    case STATE_DESCRIPTION:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionpackage);
        rec->description = cr_safe_string_chunk_insert_null(rec->chunk, content);
        break;

    case STATE_SOLUTION:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionpackage);
        rec->solution = cr_safe_string_chunk_insert_null(rec->chunk, content);
        break;

    case STATE_NAME:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(pd->updatecollection);
        assert(!pd->updatecollectionpackage);
        col->name = cr_safe_string_chunk_insert_null(col->chunk, content);
        break;

    case STATE_FILENAME:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(pd->updatecollection);
        assert(pd->updatecollectionpackage);
        package->filename = cr_safe_string_chunk_insert_null(package->chunk,
                                                             content);
        break;

    case STATE_SUM:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(pd->updatecollection);
        assert(pd->updatecollectionpackage);
        package->sum = cr_safe_string_chunk_insert_null(package->chunk, content);
        break;

    case STATE_PACKAGE:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(pd->updatecollection);
        assert(pd->updatecollectionpackage);
        pd->updatecollectionpackage = NULL;
        break;

    case STATE_COLLECTION:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(pd->updatecollection);
        assert(!pd->updatecollectionpackage);
        pd->updatecollection = NULL;
        break;

    case STATE_UPDATE:
        assert(pd->updateinfo);
        assert(pd->updaterecord);
        assert(!pd->updatecollection);
        assert(!pd->updatecollectionpackage);
        pd->updaterecord = NULL;
        break;

    default:
        break;
    }
}

int
cr_xml_parse_updateinfo(const char *path,
                        cr_UpdateInfo *updateinfo,
                        cr_XmlParserWarningCb warningcb,
                        void *warningcb_data,
                        GError **err)
{
    int ret = CRE_OK;
    cr_ParserData *pd;
    GError *tmp_err = NULL;

    assert(path);
    assert(updateinfo);
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
    pd->updateinfo = updateinfo;
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
                          "\"<updates>\" - The file probably isn't "
                          "a valid updates.xml");

    // Clean up

    cr_xml_parser_data_free(pd);

    return ret;
}
