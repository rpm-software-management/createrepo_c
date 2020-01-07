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


void
cr_xml_dump_init()
{
    xmlInitParser();
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
cr_xml_dump_files(xmlNodePtr node, cr_Package *package, int primary)
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
    }
}

gboolean
cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(GSList *dep)
{
    GSList *element;
    for (element = dep; element; element=g_slist_next(element)) {
        cr_Dependency *d = element->data;
        if ((d->name    && cr_hascontrollchars((unsigned char *) d->name))    ||
            (d->epoch   && cr_hascontrollchars((unsigned char *) d->epoch))   ||
            (d->version && cr_hascontrollchars((unsigned char *) d->version)) ||
            (d->release && cr_hascontrollchars((unsigned char *) d->release)))
        {
            return 1;
        }
    }
    return 0;
}

gboolean
cr_Package_contains_forbidden_control_chars(cr_Package *pkg)
{
    if ((pkg->name          && cr_hascontrollchars((unsigned char *) pkg->name))          ||
        (pkg->arch          && cr_hascontrollchars((unsigned char *) pkg->arch))          ||
        (pkg->version       && cr_hascontrollchars((unsigned char *) pkg->version))       ||
        (pkg->epoch         && cr_hascontrollchars((unsigned char *) pkg->epoch))         ||
        (pkg->release       && cr_hascontrollchars((unsigned char *) pkg->release))       ||
        (pkg->summary       && cr_hascontrollchars((unsigned char *) pkg->summary))       ||
        (pkg->description   && cr_hascontrollchars((unsigned char *) pkg->description))   ||
        (pkg->url           && cr_hascontrollchars((unsigned char *) pkg->url))           ||
        (pkg->rpm_license   && cr_hascontrollchars((unsigned char *) pkg->rpm_license))   ||
        (pkg->rpm_vendor    && cr_hascontrollchars((unsigned char *) pkg->rpm_vendor))    ||
        (pkg->rpm_group     && cr_hascontrollchars((unsigned char *) pkg->rpm_group))     ||
        (pkg->rpm_buildhost && cr_hascontrollchars((unsigned char *) pkg->rpm_buildhost)) ||
        (pkg->rpm_sourcerpm && cr_hascontrollchars((unsigned char *) pkg->rpm_sourcerpm)) ||
        (pkg->rpm_packager  && cr_hascontrollchars((unsigned char *) pkg->rpm_packager))  ||
        (pkg->location_href && cr_hascontrollchars((unsigned char *) pkg->location_href)) ||
        (pkg->location_base && cr_hascontrollchars((unsigned char *) pkg->location_base)))
    {
        return 1;
    }

    if (cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->requires)    ||
        cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->provides)    ||
        cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->conflicts)   ||
        cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->obsoletes)   ||
        cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->suggests)    ||
        cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->enhances)    ||
        cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->recommends)  ||
        cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(pkg->supplements))
    {
        return 1;
    }

    GSList *element;

    for (element = pkg->files; element; element=g_slist_next(element)) {
        cr_PackageFile *f = element->data;
        if ((f->name && cr_hascontrollchars((unsigned char *) f->name)) ||
            (f->path && cr_hascontrollchars((unsigned char *) f->path)))
        {
            return 1;
        }
    }

    for (element = pkg->changelogs; element; element=g_slist_next(element)) {
        cr_ChangelogEntry *ch = element->data;
        if ((ch->author    && cr_hascontrollchars((unsigned char *) ch->author)) ||
            (ch->changelog && cr_hascontrollchars((unsigned char *) ch->changelog)))
        {
            return 1;
        }
    }

    return 0;
}

struct cr_XmlStruct
cr_xml_dump(cr_Package *pkg, GError **err)
{
    struct cr_XmlStruct result;
    GError *tmp_err = NULL;

    assert(!err || *err == NULL);

    result.primary   = NULL;
    result.filelists = NULL;
    result.other     = NULL;

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

    result.other = cr_xml_dump_other(pkg, &tmp_err);
    if (tmp_err) {
        g_propagate_error(err, tmp_err);
        g_free(result.primary);
        result.primary = NULL;
        g_free(result.filelists);
        result.filelists = NULL;
        return result;
    }

    return result;
}
