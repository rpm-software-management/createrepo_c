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
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <libxml/parser.h>
#include <string.h>
#include "logging.h"
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


void
cr_latin1_to_utf8(const unsigned char *in, unsigned char *out)
{
    // http://stackoverflow.com/questions/4059775/convert-iso-8859-1-strings-to-utf-8-in-c-c/4059934#4059934
    // This function converts latin1 to utf8 in effective and thread-safe way.
    while (*in) {
        if (*in<128) {
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


struct cr_XmlStruct
cr_xml_dump(cr_Package *pkg)
{
    struct cr_XmlStruct result;

    if (!pkg) {
        result.primary   = NULL;
        result.filelists = NULL;
        result.other     = NULL;
        return result;
    }

    result.primary   = cr_xml_dump_primary(pkg);
    result.filelists = cr_xml_dump_filelists(pkg);
    result.other     = cr_xml_dump_other(pkg);

    return result;
}
