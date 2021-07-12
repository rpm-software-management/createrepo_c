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
#define ERR_CODE_XML    CRE_BADXMLPRIMARY

typedef enum {
    STATE_START,
      STATE_METADATA,
        STATE_PACKAGE,
          STATE_NAME,
          STATE_ARCH,
          STATE_VERSION,
          STATE_CHECKSUM,
          STATE_SUMMARY,
          STATE_DESCRIPTION,
          STATE_PACKAGER,
          STATE_URL,
          STATE_TIME,
          STATE_SIZE,
          STATE_LOCATION,
          STATE_FORMAT,
            STATE_RPM_LICENSE,
            STATE_RPM_VENDOR,
            STATE_RPM_GROUP,
            STATE_RPM_BUILDHOST,
            STATE_RPM_SOURCERPM,
            STATE_RPM_HEADER_RANGE,
            STATE_RPM_PROVIDES,
              STATE_RPM_ENTRY_PROVIDES,
            STATE_RPM_REQUIRES,
              STATE_RPM_ENTRY_REQUIRES,
            STATE_RPM_CONFLICTS,
              STATE_RPM_ENTRY_CONFLICTS,
            STATE_RPM_OBSOLETES,
              STATE_RPM_ENTRY_OBSOLETES,
            STATE_RPM_SUGGESTS,
              STATE_RPM_ENTRY_SUGGESTS,
            STATE_RPM_ENHANCES,
              STATE_RPM_ENTRY_ENHANCES,
            STATE_RPM_RECOMMENDS,
              STATE_RPM_ENTRY_RECOMMENDS,
            STATE_RPM_SUPPLEMENTS,
              STATE_RPM_ENTRY_SUPPLEMENTS,
            STATE_FILE,
    NUMSTATES,
} cr_PriState;

/* NOTE: Same states in the first column must be together!!!
 * Performance tip: More frequent elements should be listed
 * first in its group (eg: element "package" (STATE_PACKAGE)
 * has a "file" element listed first, because it is more frequent
 * than other elements). */
static cr_StatesSwitch stateswitches[] = {
    { STATE_START,          "metadata",         STATE_METADATA,         0 },
    { STATE_METADATA,       "package",          STATE_PACKAGE,          0 },
    { STATE_PACKAGE,        "name",             STATE_NAME,             1 },
    { STATE_PACKAGE,        "arch",             STATE_ARCH,             1 },
    { STATE_PACKAGE,        "version",          STATE_VERSION,          0 },
    { STATE_PACKAGE,        "checksum",         STATE_CHECKSUM,         1 },
    { STATE_PACKAGE,        "summary",          STATE_SUMMARY,          1 },
    { STATE_PACKAGE,        "description",      STATE_DESCRIPTION,      1 },
    { STATE_PACKAGE,        "packager",         STATE_PACKAGER,         1 },
    { STATE_PACKAGE,        "url",              STATE_URL,              1 },
    { STATE_PACKAGE,        "time",             STATE_TIME,             0 },
    { STATE_PACKAGE,        "size",             STATE_SIZE,             0 },
    { STATE_PACKAGE,        "location",         STATE_LOCATION,         0 },
    { STATE_PACKAGE,        "format",           STATE_FORMAT,           0 },
    { STATE_FORMAT,         "file",             STATE_FILE,             1 },
    { STATE_FORMAT,         "rpm:license",      STATE_RPM_LICENSE,      1 },
    { STATE_FORMAT,         "rpm:vendor",       STATE_RPM_VENDOR,       1 },
    { STATE_FORMAT,         "rpm:group",        STATE_RPM_GROUP,        1 },
    { STATE_FORMAT,         "rpm:buildhost",    STATE_RPM_BUILDHOST,    1 },
    { STATE_FORMAT,         "rpm:sourcerpm",    STATE_RPM_SOURCERPM,    1 },
    { STATE_FORMAT,         "rpm:header-range", STATE_RPM_HEADER_RANGE, 0 },
    { STATE_FORMAT,         "rpm:provides",     STATE_RPM_PROVIDES,     0 },
    { STATE_FORMAT,         "rpm:requires",     STATE_RPM_REQUIRES,     0 },
    { STATE_FORMAT,         "rpm:conflicts",    STATE_RPM_CONFLICTS,    0 },
    { STATE_FORMAT,         "rpm:obsoletes",    STATE_RPM_OBSOLETES,    0 },
    { STATE_FORMAT,         "rpm:suggests",     STATE_RPM_SUGGESTS,     0 },
    { STATE_FORMAT,         "rpm:enhances",     STATE_RPM_ENHANCES,     0 },
    { STATE_FORMAT,         "rpm:recommends",   STATE_RPM_RECOMMENDS,   0 },
    { STATE_FORMAT,         "rpm:supplements",  STATE_RPM_SUPPLEMENTS,  0 },
    { STATE_RPM_PROVIDES,   "rpm:entry",        STATE_RPM_ENTRY_PROVIDES,    0 },
    { STATE_RPM_REQUIRES,   "rpm:entry",        STATE_RPM_ENTRY_REQUIRES,    0 },
    { STATE_RPM_CONFLICTS,  "rpm:entry",        STATE_RPM_ENTRY_CONFLICTS,   0 },
    { STATE_RPM_OBSOLETES,  "rpm:entry",        STATE_RPM_ENTRY_OBSOLETES,   0 },
    { STATE_RPM_SUGGESTS,   "rpm:entry",        STATE_RPM_ENTRY_SUGGESTS,    0 },
    { STATE_RPM_ENHANCES,   "rpm:entry",        STATE_RPM_ENTRY_ENHANCES,    0 },
    { STATE_RPM_RECOMMENDS, "rpm:entry",        STATE_RPM_ENTRY_RECOMMENDS,  0 },
    { STATE_RPM_SUPPLEMENTS,"rpm:entry",        STATE_RPM_ENTRY_SUPPLEMENTS, 0 },
    { NUMSTATES,            NULL,               NUMSTATES,              0 },
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

    if (!pd->pkg && pd->state != STATE_METADATA && pd->state != STATE_START)
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

    case STATE_METADATA:
        pd->main_tag_found = TRUE;
        break;

    case STATE_PACKAGE:
        assert(!pd->pkg);

        val = cr_find_attr("type", attr);

        if (!val)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                           "Missing attribute \"type\" of a package element");

        // Get package object to store current package or NULL if
        // current XML package element should be skipped/ignored.
        if (pd->newpkgcb(&pd->pkg,
                         val,
                         NULL,
                         NULL,
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

        break;

    case STATE_NAME:
    case STATE_ARCH:
        break;

    case STATE_VERSION:
        assert(pd->pkg);

        // Version strings insert only if them don't already exists
        // They could be already filled by filelists or other parser.

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

    case STATE_CHECKSUM:
        assert(pd->pkg);
        val = cr_find_attr("type", attr);
        if (!val)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                        "Missing attribute \"type\" of a checksum element");
        else
            pd->pkg->checksum_type = g_string_chunk_insert(pd->pkg->chunk, val);
        break;

    case STATE_SUMMARY:
    case STATE_DESCRIPTION:
    case STATE_PACKAGER:
    case STATE_URL:
        break;

    case STATE_TIME:
        assert(pd->pkg);

        val = cr_find_attr("file", attr);
        if (!val)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                        "Missing attribute \"file\" of a time element");
        else
            pd->pkg->time_file = cr_xml_parser_strtoll(pd, val, 10);

        val = cr_find_attr("build", attr);
        if (!val)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                        "Missing attribute \"build\" of a time element");
        else
            pd->pkg->time_build = cr_xml_parser_strtoll(pd, val, 10);

        break;

    case STATE_SIZE:
        assert(pd->pkg);

        val = cr_find_attr("package", attr);
        if (!val)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                        "Missing attribute \"package\" of a size element");
        else
            pd->pkg->size_package = cr_xml_parser_strtoll(pd, val, 10);

        val = cr_find_attr("installed", attr);
        if (!val)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                        "Missing attribute \"installed\" of a size element");
        else
            pd->pkg->size_installed = cr_xml_parser_strtoll(pd, val, 10);

        val = cr_find_attr("archive", attr);
        if (!val)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                        "Missing attribute \"archive\" of a size element");
        else
            pd->pkg->size_archive = cr_xml_parser_strtoll(pd, val, 10);

        break;

    case STATE_LOCATION:
        assert(pd->pkg);

        val = cr_find_attr("href", attr);
        if (!val)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                        "Missing attribute \"href\" of a location element");
        else
            pd->pkg->location_href = g_string_chunk_insert(pd->pkg->chunk, val);

        val = cr_find_attr("xml:base", attr);
        if (val)
            pd->pkg->location_base = g_string_chunk_insert(pd->pkg->chunk, val);

        break;

    case STATE_FORMAT:
    case STATE_RPM_LICENSE:
    case STATE_RPM_VENDOR:
    case STATE_RPM_GROUP:
    case STATE_RPM_BUILDHOST:
    case STATE_RPM_SOURCERPM:
        break;

    case STATE_RPM_HEADER_RANGE:
        assert(pd->pkg);

        val = cr_find_attr("start", attr);
        if (!val)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                    "Missing attribute \"start\" of a header-range element");
        else
            pd->pkg->rpm_header_start = cr_xml_parser_strtoll(pd, val, 10);

        val = cr_find_attr("end", attr);
        if (!val)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                        "Missing attribute \"end\" of a time element");
        else
            pd->pkg->rpm_header_end = cr_xml_parser_strtoll(pd, val, 10);

        break;

    case STATE_RPM_PROVIDES:
    case STATE_RPM_REQUIRES:
    case STATE_RPM_CONFLICTS:
    case STATE_RPM_OBSOLETES:
    case STATE_RPM_SUGGESTS:
    case STATE_RPM_ENHANCES:
    case STATE_RPM_RECOMMENDS:
    case STATE_RPM_SUPPLEMENTS:
        break;

    case STATE_RPM_ENTRY_PROVIDES:
    case STATE_RPM_ENTRY_REQUIRES:
    case STATE_RPM_ENTRY_CONFLICTS:
    case STATE_RPM_ENTRY_OBSOLETES:
    case STATE_RPM_ENTRY_SUGGESTS:
    case STATE_RPM_ENTRY_ENHANCES:
    case STATE_RPM_ENTRY_RECOMMENDS:
    case STATE_RPM_ENTRY_SUPPLEMENTS:
    {
        assert(pd->pkg);

        cr_Dependency *dep = cr_dependency_new();

        val = cr_find_attr("name", attr);
        if (!val)
            cr_xml_parser_warning(pd, CR_XML_WARNING_MISSINGATTR,
                        "Missing attribute \"name\" of an entry element");
        else
            dep->name = g_string_chunk_insert(pd->pkg->chunk, val);

        // Rest of attrs is optional

        val = cr_find_attr("flags", attr);
        if (val)
            dep->flags = g_string_chunk_insert(pd->pkg->chunk, val);

        val = cr_find_attr("epoch", attr);
        if (val)
            dep->epoch = g_string_chunk_insert(pd->pkg->chunk, val);

        val = cr_find_attr("ver", attr);
        if (val)
            dep->version = g_string_chunk_insert(pd->pkg->chunk, val);

        val = cr_find_attr("rel", attr);
        if (val)
            dep->release = g_string_chunk_insert(pd->pkg->chunk, val);

        val = cr_find_attr("pre", attr);
        if (val) {
            if (!strcmp(val, "0") ||
                !strcmp(val, "FALSE") ||
                !strcmp(val, "false") ||
                !strcmp(val, "False"))
                dep->pre = FALSE;
            else
                dep->pre = TRUE;
        }

        switch (pd->state) {
            case STATE_RPM_ENTRY_PROVIDES:
                pd->pkg->provides = g_slist_prepend(pd->pkg->provides, dep);
                break;
            case STATE_RPM_ENTRY_REQUIRES:
                pd->pkg->requires = g_slist_prepend(pd->pkg->requires, dep);
                break;
            case STATE_RPM_ENTRY_CONFLICTS:
                pd->pkg->conflicts = g_slist_prepend(pd->pkg->conflicts, dep);
                break;
            case STATE_RPM_ENTRY_OBSOLETES:
                pd->pkg->obsoletes = g_slist_prepend(pd->pkg->obsoletes, dep);
                break;
            case STATE_RPM_ENTRY_SUGGESTS:
                pd->pkg->suggests = g_slist_prepend(pd->pkg->suggests, dep);
                break;
            case STATE_RPM_ENTRY_ENHANCES:
                pd->pkg->enhances = g_slist_prepend(pd->pkg->enhances, dep);
                break;
            case STATE_RPM_ENTRY_RECOMMENDS:
                pd->pkg->recommends = g_slist_prepend(pd->pkg->recommends, dep);
                break;
            case STATE_RPM_ENTRY_SUPPLEMENTS:
                pd->pkg->supplements = g_slist_prepend(pd->pkg->supplements, dep);
                break;
            default: assert(0);
        }

        break;
    }

    case STATE_FILE:
        assert(pd->pkg);

        if (!pd->do_files)
            break;

        val = cr_find_attr("type", attr);
        pd->last_file_type = FILE_FILE;
        if (val) {
            if (!strcmp(val, "dir"))
                pd->last_file_type = FILE_DIR;
            else if (!strcmp(val, "ghost"))
                pd->last_file_type = FILE_GHOST;
            else
                cr_xml_parser_warning(pd, CR_XML_WARNING_UNKNOWNVAL,
                                      "Unknown file type \"%s\"", val);
        }
        break;

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
    case STATE_METADATA:
        break;

    case STATE_PACKAGE:
        if (!pd->pkg)
            return;

        if (!pd->pkg->pkgId) {
            // Package without a pkgid attr is error
            g_set_error(&pd->err, ERR_DOMAIN, ERR_CODE_XML,
                        "Package without pkgid (checksum)!");
            break;
        }

        if (pd->pkg->pkgId[0] == '\0') {
            // Package without a pkgid attr is error
            g_set_error(&pd->err, ERR_DOMAIN, ERR_CODE_XML,
                        "Package with empty pkgid (checksum)!");
            break;
        }

        if (pd->do_files)
            // Reverse order of files
            pd->pkg->files = g_slist_reverse(pd->pkg->files);

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

    case STATE_NAME:
        assert(pd->pkg);
        if (!pd->pkg->name)
            // name could be already filled by filelists or other xml parser
            pd->pkg->name = cr_safe_string_chunk_insert_null(pd->pkg->chunk,
                                                             pd->content);
        break;

    case STATE_ARCH:
        assert(pd->pkg);
        if (!pd->pkg->arch)
            // arch could be already filled by filelists or other xml parser
            pd->pkg->arch = cr_safe_string_chunk_insert_null(pd->pkg->chunk,
                                                             pd->content);
        break;

    case STATE_CHECKSUM:
        assert(pd->pkg);
        if (!pd->pkg->pkgId)
            // pkgId could be already filled by filelists or other xml parser
            pd->pkg->pkgId = cr_safe_string_chunk_insert_null(pd->pkg->chunk,
                                                              pd->content);
        break;

    case STATE_SUMMARY:
        assert(pd->pkg);
        pd->pkg->summary = cr_safe_string_chunk_insert_null(pd->pkg->chunk,
                                                            pd->content);
        break;

    case STATE_DESCRIPTION:
        assert(pd->pkg);
        pd->pkg->description = cr_safe_string_chunk_insert_null(pd->pkg->chunk,
                                                                pd->content);
        break;

    case STATE_PACKAGER:
        assert(pd->pkg);
        pd->pkg->rpm_packager = cr_safe_string_chunk_insert_null(pd->pkg->chunk,
                                                                 pd->content);
        break;

    case STATE_URL:
        assert(pd->pkg);
        pd->pkg->url = cr_safe_string_chunk_insert_null(pd->pkg->chunk,
                                                        pd->content);
        break;

    case STATE_RPM_LICENSE:
        assert(pd->pkg);
        pd->pkg->rpm_license = cr_safe_string_chunk_insert_null(pd->pkg->chunk,
                                                                pd->content);
        break;

    case STATE_RPM_VENDOR:
        assert(pd->pkg);
        pd->pkg->rpm_vendor = cr_safe_string_chunk_insert_null(pd->pkg->chunk,
                                                               pd->content);
        break;

    case STATE_RPM_GROUP:
        assert(pd->pkg);
        pd->pkg->rpm_group = cr_safe_string_chunk_insert_null(pd->pkg->chunk,
                                                              pd->content);
        break;

    case STATE_RPM_BUILDHOST:
        assert(pd->pkg);
        pd->pkg->rpm_buildhost = cr_safe_string_chunk_insert_null(pd->pkg->chunk,
                                                                  pd->content);
        break;

    case STATE_RPM_SOURCERPM:
        assert(pd->pkg);
        pd->pkg->rpm_sourcerpm = cr_safe_string_chunk_insert_null(pd->pkg->chunk,
                                                                  pd->content);
        break;

    case STATE_RPM_PROVIDES:
        pd->pkg->provides = g_slist_reverse(pd->pkg->provides);
        break;

    case STATE_RPM_REQUIRES:
        pd->pkg->requires = g_slist_reverse(pd->pkg->requires);
        break;

    case STATE_RPM_CONFLICTS:
        pd->pkg->conflicts = g_slist_reverse(pd->pkg->conflicts);
        break;

    case STATE_RPM_OBSOLETES:
        pd->pkg->obsoletes = g_slist_reverse(pd->pkg->obsoletes);
        break;

    case STATE_RPM_SUGGESTS:
        pd->pkg->suggests = g_slist_reverse(pd->pkg->suggests);
        break;

    case STATE_RPM_ENHANCES:
        pd->pkg->enhances = g_slist_reverse(pd->pkg->enhances);
        break;

    case STATE_RPM_RECOMMENDS:
        pd->pkg->recommends = g_slist_reverse(pd->pkg->recommends);
        break;

    case STATE_RPM_SUPPLEMENTS:
        pd->pkg->supplements = g_slist_reverse(pd->pkg->supplements);
        break;

    case STATE_FILE: {
        assert(pd->pkg);

        if (!pd->do_files)
            break;

        if (!pd->content)
            break;

        cr_PackageFile *pkg_file = cr_package_file_new();
        pkg_file->name = cr_safe_string_chunk_insert(pd->pkg->chunk,
                                                cr_get_filename(pd->content));
        if (!pkg_file->name) {
            g_set_error(&pd->err, ERR_DOMAIN, ERR_CODE_XML,
                        "Invalid <file> element: %s", pd->content);
            g_free(pkg_file);
            break;
        }
        pd->content[pd->lcontent - strlen(pkg_file->name)] = '\0';
        pkg_file->path = cr_safe_string_chunk_insert_const(pd->pkg->chunk,
                                                           pd->content);
        switch (pd->last_file_type) {
            case FILE_FILE:  pkg_file->type = NULL;    break; // NULL => "file"
            case FILE_DIR:   pkg_file->type = "dir";   break;
            case FILE_GHOST: pkg_file->type = "ghost"; break;
            default: assert(0);  // Should not happend
        }

        pd->pkg->files = g_slist_prepend(pd->pkg->files, pkg_file);
        break;
    }

    default:
        break;
    }
}

cr_ParserData *
primary_parser_data_new(cr_XmlParserNewPkgCb newpkgcb,
                        void *newpkgcb_data,
                        cr_XmlParserPkgCb pkgcb,
                        void *pkgcb_data,
                        cr_XmlParserWarningCb warningcb,
                        void *warningcb_data,
                        int do_files)
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
    pd->do_files = do_files;
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
cr_xml_parse_primary_internal(const char *target,
                              cr_XmlParserNewPkgCb newpkgcb,
                              void *newpkgcb_data,
                              cr_XmlParserPkgCb pkgcb,
                              void *pkgcb_data,
                              cr_XmlParserWarningCb warningcb,
                              void *warningcb_data,
                              int do_files,
                              int (*parser_func)(xmlParserCtxtPtr, cr_ParserData *, const char *, GError**),
                              GError **err)
{
    int ret = CRE_OK;
    GError *tmp_err = NULL;

    assert(target);
    assert(!err || *err == NULL);

    cr_ParserData *pd;
    pd = primary_parser_data_new(newpkgcb, newpkgcb_data, pkgcb, pkgcb_data, warningcb, warningcb_data, do_files);

    // Parsing
    ret = parser_func(pd->parser, pd, target, &tmp_err);

    if (tmp_err)
        g_propagate_error(err, tmp_err);

    // Warning if file was probably a different type than expected

    if (!pd->main_tag_found && ret == CRE_OK)
        cr_xml_parser_warning(pd, CR_XML_WARNING_BADMDTYPE,
                          "The target doesn't contain the expected element "
                          "\"<metadata>\" - The target probably isn't "
                          "a valid primary xml");

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
cr_xml_parse_primary(const char *path,
                     cr_XmlParserNewPkgCb newpkgcb,
                     void *newpkgcb_data,
                     cr_XmlParserPkgCb pkgcb,
                     void *pkgcb_data,
                     cr_XmlParserWarningCb warningcb,
                     void *warningcb_data,
                     int do_files,
                     GError **err)
{

    return cr_xml_parse_primary_internal(path, newpkgcb, newpkgcb_data, pkgcb, pkgcb_data,
                                         warningcb, warningcb_data, do_files, &cr_xml_parser_generic, err);
}

int
cr_xml_parse_primary_snippet(const char *xml_string,
                             cr_XmlParserNewPkgCb newpkgcb,
                             void *newpkgcb_data,
                             cr_XmlParserPkgCb pkgcb,
                             void *pkgcb_data,
                             cr_XmlParserWarningCb warningcb,
                             void *warningcb_data,
                             int do_files,
                             GError **err)
{
    char* wrapped_xml_string = g_strconcat("<metadata>", xml_string, "</metadata>", NULL);
    int ret =  cr_xml_parse_primary_internal(wrapped_xml_string, newpkgcb, newpkgcb_data, pkgcb, pkgcb_data,
                                             warningcb, warningcb_data, do_files, &cr_xml_parser_generic_from_string, err);
    free(wrapped_xml_string);
    return ret;
}
