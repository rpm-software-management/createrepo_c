/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <glib.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <string.h>
#include "logging.h"
#include "misc.h"
#include "xml_dump.h"

#undef MODULE
#define MODULE "xml_dump: "


void dump_files(xmlNodePtr node, Package *package, int primary)
{
    if (!node || !package->files) {
        return;
    }


    GSList *element = NULL;
    for(element = package->files; element; element=element->next) {
        PackageFile *entry = (PackageFile*) element->data;


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

        if (primary && !is_primary(fullname)) {
            g_free(fullname);
            continue;
        }


        // ***********************************
        // Element: file
        // ************************************

        xmlNodePtr file_node;
        file_node = xmlNewTextChild(node, NULL, BAD_CAST "file", BAD_CAST fullname);
        g_free(fullname);

        // Write type (skip type if type value is empty of "file")
        if (entry->type && entry->type[0] != '\0' && strcmp(entry->type, "file")) {
            xmlNewProp(file_node, BAD_CAST "type", BAD_CAST entry->type);
        }
    }
}



struct XmlStruct xml_dump(Package *pkg)
{
    struct XmlStruct result;

    if (!pkg) {
        result.primary   = NULL;
        result.filelists = NULL;
        result.other     = NULL;
        return result;
    }

    result.primary   = xml_dump_primary(pkg);
    result.filelists = xml_dump_filelists(pkg);
    result.other     = xml_dump_other(pkg);

    return result;
}
