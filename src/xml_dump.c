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
#include <assert.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <libxml/parser.h>
#include <string.h>
#include "error.h"
#include "misc.h"
#include "xml_dump.h"
#include "xml_dump_internal.h"


static int _xml_dump_parameters[CR_XML_DUMP_OPTION_COUNT];

void
cr_xml_dump_init()
{
    xmlInitParser();

    /* Default Settings for parameters */
    _xml_dump_parameters[CR_XML_DUMP_DO_PRETTY_PRINT] = TRUE;
}

void cr_xml_dump_set_parameter(cr_dump_parameter param, int value)
{
    switch(param) {
        case CR_XML_DUMP_DO_PRETTY_PRINT:
            _xml_dump_parameters[param] = value;
        break;
        default:
            /* undefined parameter, ignored. */
        break;
    }
}

/** Get the value of one xml dump parameter.
*/
int cr_xml_dump_get_parameter(cr_dump_parameter param) {
    switch (param) {
        case CR_XML_DUMP_DO_PRETTY_PRINT:
            return _xml_dump_parameters[param];
        default:
            break;
    }

    return 0;
}

void
cr_xml_dump_cleanup()
{
    xmlCleanupParser();
}

gboolean cr_hascontrollchars(const unsigned char *str)
{
    while (*str) {
        if (*str < 32 && (*str != 9 && *str != 10 && *str != 13))
            return TRUE;
        ++str;
    }

    return FALSE;
}

gchar *
cr_prepend_protocol(const gchar *url)
{
    if (url && *url == '/')
        return g_strconcat("file://", url, NULL);
    return g_strdup(url);
}

void
cr_latin1_to_utf8(const unsigned char *in, unsigned char *out)
{
    // http://stackoverflow.com/questions/4059775/convert-iso-8859-1-strings-to-utf-8-in-c-c/4059934#4059934
    // This function converts latin1 to utf8 in effective and thread-safe way.
    while (*in) {
        if (*in<128) {
            if (*in < 32 && (*in != 9 && *in != 10 && *in != 13)) {
                ++in;
                continue;
            }
            *out++=*in++;
        } else if (*in<192) {
            // Found latin1 (iso-8859-1) control code.
            // The string is probably misencoded cp-1252 and not a real latin1.
            // Just skip this character.
            in++;
            continue;
        } else {
            *out++=0xc2+(*in>0xbf);
            *out++=(*in++&0x3f)+0x80;
        }
    }
    *out = '\0';
}

xmlNodePtr
cr_xmlNewTextChild(xmlNodePtr parent,
                   xmlNsPtr ns,
                   const xmlChar *name,
                   const xmlChar *orig_content)
{
    int free_content = 0;
    xmlChar *content;
    xmlNodePtr child;

    if (!orig_content) {
        content = BAD_CAST "";
    } else if (xmlCheckUTF8(orig_content)) {
        content = (xmlChar *) orig_content;
    } else {
        size_t len = strlen((const char *) orig_content);
        content = malloc(sizeof(xmlChar)*len*2 + 1);
        cr_latin1_to_utf8(orig_content, content);
        free_content = 1;
    }

    child = xmlNewTextChild(parent, ns, name, content);

    if (free_content)
        free(content);

    return child;
}

xmlAttrPtr
cr_xmlNewProp(xmlNodePtr node, const xmlChar *name, const xmlChar *orig_content)
{
    int free_content = 0;
    xmlChar *content;
    xmlAttrPtr attr;

    if (!orig_content) {
        content = BAD_CAST "";
    } else if (xmlCheckUTF8(orig_content)) {
        content = (xmlChar *) orig_content;
    } else {
        size_t len = strlen((const char *) orig_content);
        content = malloc(sizeof(xmlChar)*len*2 + 1);
        cr_latin1_to_utf8(orig_content, content);
        free_content = 1;
    }

    attr = xmlNewProp(node, name, content);

    if (free_content)
        free(content);

    return attr;
}

void
cr_xml_dump_files(xmlNodePtr node, cr_Package *package, int primary, gboolean filelists_ext)
{
    if (!node || !package->files) {
        return;
    }


    GSList *element = NULL;
    for(element = package->files; element; element=element->next) {
        cr_PackageFile *entry = (cr_PackageFile*) element->data;


        // File without name or path is suspicious => Skip it

        if (!(entry->path) || !(entry->name)) {
            continue;
        }


        // String concatenation (path + basename)

        gchar *fullname;
        fullname = g_strconcat(entry->path, entry->name, NULL);

        if (!fullname) {
            continue;
        }


        // Skip a file if we want primary files and the file is not one

        if (primary && !cr_is_primary(fullname)) {
            g_free(fullname);
            continue;
        }


        // ***********************************
        // Element: file
        // ************************************

        xmlNodePtr file_node;
        file_node = cr_xmlNewTextChild(node,
                                       NULL,
                                       BAD_CAST "file",
                                       BAD_CAST fullname);
        g_free(fullname);

        // Write type (skip type if type value is empty of "file")
        if (entry->type && entry->type[0] != '\0' && strcmp(entry->type, "file")) {
            cr_xmlNewProp(file_node, BAD_CAST "type", BAD_CAST entry->type);
        }

        if (filelists_ext && entry->digest && entry->digest[0] != '\0') {
            cr_xmlNewProp(file_node, BAD_CAST "hash", BAD_CAST entry->digest);
        }
    }
}

gboolean
cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(GSList *dep)
{
    gboolean ret = FALSE;
    GSList *element;
    for (element = dep; element; element=g_slist_next(element)) {
        cr_Dependency *d = element->data;
        if (d->name    && cr_hascontrollchars((unsigned char *) d->name)) {
            g_printerr("name %s have forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", d->name);
            ret = TRUE;
        }
        if (d->epoch   && cr_hascontrollchars((unsigned char *) d->epoch)) {
            g_printerr("epoch %s have forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", d->epoch);
            ret = TRUE;
        }
        if (d->version && cr_hascontrollchars((unsigned char *) d->version)) {
            g_printerr("version %s have forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", d->version);
            ret = TRUE;
        }
        if (d->release && cr_hascontrollchars((unsigned char *) d->release)) {
            g_printerr("release %s have forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", d->release);
            ret = TRUE;
        }
    }
    return ret;
}

gboolean
cr_Package_contains_forbidden_control_chars(cr_Package *pkg)
{
    gboolean ret = FALSE;

    if (pkg->name && cr_hascontrollchars((unsigned char *) pkg->name)) {
        g_printerr("Package name %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->name);
        ret = TRUE;
    }
    if (pkg->arch && cr_hascontrollchars((unsigned char *) pkg->arch)) {
        g_printerr("Package arch %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->arch);
        ret = TRUE;
    }
    if (pkg->version && cr_hascontrollchars((unsigned char *) pkg->version)) {
        g_printerr("Package version %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->version);
        ret = TRUE;
    }
    if (pkg->epoch && cr_hascontrollchars((unsigned char *) pkg->epoch)) {
        g_printerr("Package epoch %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->epoch);
        ret = TRUE;
    }
    if (pkg->release && cr_hascontrollchars((unsigned char *) pkg->release)) {
        g_printerr("Package release %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->release);
        ret = TRUE;
    }
    if (pkg->summary && cr_hascontrollchars((unsigned char *) pkg->summary)) {
        g_printerr("Package summary %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->summary);
        ret = TRUE;
    }
    if (pkg->description && cr_hascontrollchars((unsigned char *) pkg->description)) {
        g_printerr("Package description %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->description);
        ret = TRUE;
    }
    if (pkg->url && cr_hascontrollchars((unsigned char *) pkg->url)) {
        g_printerr("Package URL %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->url);
        ret = TRUE;
    }
    if (pkg->rpm_license && cr_hascontrollchars((unsigned char *) pkg->rpm_license)) {
        g_printerr("Package RPM license %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->rpm_license);
        ret = TRUE;
    }
    if (pkg->rpm_vendor && cr_hascontrollchars((unsigned char *) pkg->rpm_vendor)) {
        g_printerr("Package RPM vendor %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->rpm_vendor);
        ret = TRUE;
    }
    if (pkg->rpm_group && cr_hascontrollchars((unsigned char *) pkg->rpm_group)) {
        g_printerr("Package RPM group %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->rpm_group);
        ret = TRUE;
    }
    if (pkg->rpm_buildhost && cr_hascontrollchars((unsigned char *) pkg->rpm_buildhost)) {
        g_printerr("Package RPM buildhost %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->rpm_buildhost);
        ret = TRUE;
    }
    if (pkg->rpm_sourcerpm && cr_hascontrollchars((unsigned char *) pkg->rpm_sourcerpm)) {
        g_printerr("Package RPM sourcerpm %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->rpm_sourcerpm);
        ret = TRUE;
    }
    if (pkg->rpm_packager && cr_hascontrollchars((unsigned char *) pkg->rpm_packager)) {
        g_printerr("Package RPM packager %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->rpm_packager);
        ret = TRUE;
    }
    if (pkg->location_href && cr_hascontrollchars((unsigned char *) pkg->location_href)) {
        g_printerr("Package location href %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->location_href);
        ret = TRUE;
    }
    if (pkg->location_base && cr_hascontrollchars((unsigned char *) pkg->location_base)) {
        g_printerr("Package location base %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", pkg->location_base);
        ret = TRUE;
    }


    if (cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->requires)) {
        g_printerr("One or more dependencies in 'requires' contain forbidden control chars (ASCII values <32 except 9, 10 and 13).\n");
        ret = TRUE;
    }

    if (cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->provides)) {
        g_printerr("One or more dependencies in 'provides' contain forbidden control chars (ASCII values <32 except 9, 10 and 13).\n");
        ret = TRUE;
    }

    if (cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->conflicts)) {
        g_printerr("One or more dependencies in 'conflicts' contain forbidden control chars (ASCII values <32 except 9, 10 and 13).\n");
        ret = TRUE;
    }

    if (cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->obsoletes)) {
        g_printerr("One or more dependencies in 'obsoletes' contain forbidden control chars (ASCII values <32 except 9, 10 and 13).\n");
        ret = TRUE;
    }

    if (cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->suggests)) {
        g_printerr("One or more dependencies in 'suggests' contain forbidden control chars (ASCII values <32 except 9, 10 and 13).\n");
        ret = TRUE;
    }

    if (cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->enhances)) {
        g_printerr("One or more dependencies in 'enhances' contain forbidden control chars (ASCII values <32 except 9, 10 and 13).\n");
        ret = TRUE;
    }

    if (cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->recommends)) {
        g_printerr("One or more dependencies in 'recommends' contain forbidden control chars (ASCII values <32 except 9, 10 and 13).\n");
        ret = TRUE;
    }

    if (cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->supplements)) {
        g_printerr("One or more dependencies in 'supplements' contain forbidden control chars (ASCII values <32 except 9, 10 and 13).\n");
        ret = TRUE;
    }


    GSList *element;

    for (element = pkg->files; element; element=g_slist_next(element)) {
        cr_PackageFile *f = element->data;
        if (f->name && cr_hascontrollchars((unsigned char *) f->name)) {
            g_printerr("File name %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", f->name);
            ret = TRUE;
        }
        if (f->path && cr_hascontrollchars((unsigned char *) f->path)) {
            g_printerr("File path %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", f->path);
            ret = TRUE;
        }
    }

    for (element = pkg->changelogs; element; element=g_slist_next(element)) {
        cr_ChangelogEntry *ch = element->data;
        if (ch->author && cr_hascontrollchars((unsigned char *) ch->author)) {
            g_printerr("Changelog author %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", ch->author);
            ret = TRUE;
        }
        if (ch->changelog && cr_hascontrollchars((unsigned char *) ch->changelog)) {
            g_printerr("Changelog entry %s contains forbidden control chars (ASCII values <32 except 9, 10 and 13).\n", ch->changelog);
            ret = TRUE;
        }
    }

    return ret;
}

struct cr_XmlStruct
cr_xml_dump_int(cr_Package *pkg, gboolean filelists_ext, GError **err)
{
    struct cr_XmlStruct result;
    GError *tmp_err = NULL;

    assert(!err || *err == NULL);

    result.primary       = NULL;
    result.filelists     = NULL;
    result.filelists_ext = NULL;
    result.other         = NULL;

    if (!pkg)
        return result;

    if (cr_Package_contains_forbidden_control_chars(pkg)) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_XMLDATA,
                    "Forbidden control chars found (ASCII values <32 except 9, 10 and 13).");
        return result;
    }

    result.primary = cr_xml_dump_primary(pkg, &tmp_err);
    if (tmp_err) {
        g_propagate_error(err, tmp_err);
        return result;
    }

    result.filelists = cr_xml_dump_filelists(pkg, &tmp_err);
    if (tmp_err) {
        g_propagate_error(err, tmp_err);
        g_free(result.primary);
        result.primary = NULL;
        return result;
    }

    if (filelists_ext) {
        result.filelists_ext = cr_xml_dump_filelists_ext(pkg, &tmp_err);
        if (tmp_err) {
            g_propagate_error(err, tmp_err);
            g_free(result.primary);
            result.primary = NULL;
            g_free(result.filelists);
            result.filelists = NULL;
            return result;
        }
    }

    result.other = cr_xml_dump_other(pkg, &tmp_err);
    if (tmp_err) {
        g_propagate_error(err, tmp_err);
        g_free(result.primary);
        result.primary = NULL;
        g_free(result.filelists);
        result.filelists = NULL;
	if (filelists_ext) {
            g_free(result.filelists_ext);
            result.filelists_ext = NULL;
	}
        return result;
    }

    return result;
}

struct cr_XmlStruct
cr_xml_dump(cr_Package *pkg, GError **err)
{
    return cr_xml_dump_int(pkg, FALSE, err);
}

struct cr_XmlStruct
cr_xml_dump_ext(cr_Package *pkg, GError **err)
{
    return cr_xml_dump_int(pkg, TRUE, err);
}
