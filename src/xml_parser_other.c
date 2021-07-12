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
#define ERR_CODE_XML    CRE_BADXMLOTHER

typedef enum {
    STATE_START,
    STATE_OTHERDATA,
    STATE_PACKAGE,
    STATE_VERSION,
    STATE_CHANGELOG,
    NUMSTATES,
} cr_OthState;

/* NOTE: Same states in the first column must be together!!!
 * Performance tip: More frequent elements should be listed
 * first in its group (eg: element "package" (STATE_PACKAGE)
 * has a "file" element listed first, because it is more frequent
 * than a "version" element). */
static cr_StatesSwitch stateswitches[] = {
    { STATE_START,      "otherdata",    STATE_OTHERDATA,    0 },
    { STATE_OTHERDATA,  "package",      STATE_PACKAGE,      0 },
    { STATE_PACKAGE,    "changelog",    STATE_CHANGELOG,    1 },
    { STATE_PACKAGE,    "version",      STATE_VERSION,      0 },
    { NUMSTATES,        NULL,           NUMSTATES,          0 },
};

static void XMLCALL
cr_start_handler(void *pdata, const xmlChar *element, const xmlChar **attr)
{
    GError *tmp_err = NULL;
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

    if (!pd->pkg && pd->state != STATE_OTHERDATA && pd->state != STATE_START)
        return;  // Do not parse current package tag and its content

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

    case STATE_OTHERDATA:
        pd->main_tag_found = TRUE;
        break;

    case STATE_PACKAGE: {
        const char *pkgId = cr_find_attr("pkgid", attr);
        const char *name  = cr_find_attr("name", attr);
        const char *arch  = cr_find_attr("arch", attr);

        if (!pkgId) {
            // Package without a pkgid attr is error
            g_set_error(&pd->err, ERR_DOMAIN, ERR_CODE_XML,
                        "Package pkgid attributte is missing!");
            break;
        }

        if (!name)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                           "Missing attribute \"name\" of a package element");

        if (!arch)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                           "Missing attribute \"arch\" of a package element");

        // Get package object to store current package or NULL if
        // current XML package element should be skipped/ignored.
        if (pd->newpkgcb(&pd->pkg,
                         pkgId,
                         name,
                         arch,
                         pd->newpkgcb_data,
                         &tmp_err))
        {
            if (tmp_err)
                g_propagate_prefixed_error(&pd->err,
                                           tmp_err,
                                           "Parsing interrupted: ");
            else
                g_set_error(&pd->err, ERR_DOMAIN, CRE_CBINTERRUPTED,
                            "Parsing interrupted");
            break;
        } else {
            // If callback return CRE_OK but it simultaneously set
            // the tmp_err then it's a programming error.
            assert(tmp_err == NULL);
        }

        if (pd->pkg) {
            if (!pd->pkg->pkgId)
                pd->pkg->pkgId = g_string_chunk_insert(pd->pkg->chunk, pkgId);
            if (!pd->pkg->name && name)
                pd->pkg->name = g_string_chunk_insert(pd->pkg->chunk, name);
            if (!pd->pkg->arch && arch)
                pd->pkg->arch = g_string_chunk_insert(pd->pkg->chunk, arch);
        }
        break;
    }

    case STATE_VERSION:
        assert(pd->pkg);

        // Version string insert only if them don't already exists

        if (!pd->pkg->epoch)
            pd->pkg->epoch = cr_safe_string_chunk_insert(pd->pkg->chunk,
                                            cr_find_attr("epoch", attr));
        if (!pd->pkg->version)
            pd->pkg->version = cr_safe_string_chunk_insert(pd->pkg->chunk,
                                            cr_find_attr("ver", attr));
        if (!pd->pkg->release)
            pd->pkg->release = cr_safe_string_chunk_insert(pd->pkg->chunk,
                                            cr_find_attr("rel", attr));
        break;

    case STATE_CHANGELOG: {
        assert(pd->pkg);
        assert(!pd->changelog);

        cr_ChangelogEntry *changelog = cr_changelog_entry_new();

        val = cr_find_attr("author", attr);
        if (!val)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                        "Missing attribute \"author\" of a package element");
        else
            changelog->author = g_string_chunk_insert(pd->pkg->chunk, val);

        val = cr_find_attr("date", attr);
        if (!val)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                        "Missing attribute \"date\" of a package element");
        else
            changelog->date = cr_xml_parser_strtoll(pd, val, 10);

        pd->pkg->changelogs = g_slist_prepend(pd->pkg->changelogs, changelog);
        pd->changelog = changelog;

        break;
    }

    default:
        break;
    }
}

static void XMLCALL
cr_end_handler(void *pdata, G_GNUC_UNUSED const xmlChar *element)
{
    cr_ParserData *pd = pdata;
    GError *tmp_err = NULL;
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
    case STATE_OTHERDATA:
    case STATE_VERSION:
        break;

    case STATE_PACKAGE:
        if (!pd->pkg)
            return;

        // Reverse list of changelogs
        pd->pkg->changelogs = g_slist_reverse(pd->pkg->changelogs);

        if (pd->pkgcb && pd->pkgcb(pd->pkg, pd->pkgcb_data, &tmp_err)) {
            if (tmp_err)
                g_propagate_prefixed_error(&pd->err,
                                           tmp_err,
                                           "Parsing interrupted: ");
            else
                g_set_error(&pd->err, ERR_DOMAIN, CRE_CBINTERRUPTED,
                            "Parsing interrupted");
        } else {
            // If callback return CRE_OK but it simultaneously set
            // the tmp_err then it's a programming error.
            assert(tmp_err == NULL);
        }

        pd->pkg = NULL;
        break;

    case STATE_CHANGELOG: {
        assert(pd->pkg);
        assert(pd->changelog);

        if (!pd->content)
            break;

        pd->changelog->changelog = g_string_chunk_insert(pd->pkg->chunk,
                                                         pd->content);
        pd->changelog = NULL;
        break;
    }

    default:
        break;
    }
}

cr_ParserData *
other_parser_data_new(cr_XmlParserNewPkgCb newpkgcb,
                      void *newpkgcb_data,
                      cr_XmlParserPkgCb pkgcb,
                      void *pkgcb_data,
                      cr_XmlParserWarningCb warningcb,
                      void *warningcb_data)
{
    cr_ParserData *pd;

    assert(newpkgcb || pkgcb);

    if (!newpkgcb)  // Use default newpkgcb
        newpkgcb = cr_newpkgcb;

    // Init
    xmlSAXHandler sax;
    memset(&sax, 0, sizeof(sax));
    sax.startElement = cr_start_handler;
    sax.endElement = cr_end_handler;
    sax.characters = cr_char_handler;

    pd = cr_xml_parser_data(NUMSTATES);

    pd->parser = xmlCreatePushParserCtxt(&sax, pd, NULL, 0, NULL);
    pd->state = STATE_START;
    pd->newpkgcb_data = newpkgcb_data;
    pd->newpkgcb = newpkgcb;
    pd->pkgcb_data = pkgcb_data;
    pd->pkgcb = pkgcb;
    pd->warningcb = warningcb;
    pd->warningcb_data = warningcb_data;
    for (cr_StatesSwitch *sw = stateswitches; sw->from != NUMSTATES; sw++) {
        if (!pd->swtab[sw->from])
            pd->swtab[sw->from] = sw;
        pd->sbtab[sw->to] = sw->from;
    }

    return pd;
}

int
cr_xml_parse_other_internal(const char *target,
                            cr_XmlParserNewPkgCb newpkgcb,
                            void *newpkgcb_data,
                            cr_XmlParserPkgCb pkgcb,
                            void *pkgcb_data,
                            cr_XmlParserWarningCb warningcb,
                            void *warningcb_data,
                            int (*parser_func)(xmlParserCtxtPtr, cr_ParserData *, const char *, GError**),
                            GError **err)
{
    int ret = CRE_OK;
    GError *tmp_err = NULL;

    assert(target);
    assert(!err || *err == NULL);

    cr_ParserData *pd;
    pd = other_parser_data_new(newpkgcb, newpkgcb_data, pkgcb, pkgcb_data, warningcb, warningcb_data);

    // Parsing

    ret = parser_func(pd->parser, pd, target, &tmp_err);

    if (tmp_err)
        g_propagate_error(err, tmp_err);

    // Warning if file was probably a different type than expected

    if (!pd->main_tag_found && ret == CRE_OK)
        cr_xml_parser_warning(pd, CR_XML_WARNING_BADMDTYPE,
                          "The target doesn't contain the expected element "
                          "\"<otherdata>\" - The target probably isn't "
                          "a valid other xml");

    // Clean up

    if (ret != CRE_OK && newpkgcb == cr_newpkgcb) {
        // Prevent memory leak when the parsing is interrupted by an error.
        // If a new package object was created by the cr_newpkgcb then
        // is obvious that there is no other reference to the package
        // except of the parser reference in pd->pkg.
        // If a caller supplied its own newpkgcb, then the freeing
        // of the currently parsed package is the caller responsibility.
        cr_package_free(pd->pkg);
    }

    cr_xml_parser_data_free(pd);

    return ret;
}

int
cr_xml_parse_other(const char *path,
                   cr_XmlParserNewPkgCb newpkgcb,
                   void *newpkgcb_data,
                   cr_XmlParserPkgCb pkgcb,
                   void *pkgcb_data,
                   cr_XmlParserWarningCb warningcb,
                   void *warningcb_data,
                   GError **err)
{
    return cr_xml_parse_other_internal(path, newpkgcb, newpkgcb_data, pkgcb, pkgcb_data,
                                       warningcb, warningcb_data, &cr_xml_parser_generic, err);
}

int
cr_xml_parse_other_snippet(const char *xml_string,
                           cr_XmlParserNewPkgCb newpkgcb,
                           void *newpkgcb_data,
                           cr_XmlParserPkgCb pkgcb,
                           void *pkgcb_data,
                           cr_XmlParserWarningCb warningcb,
                           void *warningcb_data,
                           GError **err)
{
    char* wrapped_xml_string = g_strconcat("<otherdata>", xml_string, "</otherdata>", NULL);
    int ret = cr_xml_parse_other_internal(wrapped_xml_string, newpkgcb, newpkgcb_data, pkgcb, pkgcb_data,
                                          warningcb, warningcb_data, &cr_xml_parser_generic_from_string, err);
    free(wrapped_xml_string);
    return ret;
}
