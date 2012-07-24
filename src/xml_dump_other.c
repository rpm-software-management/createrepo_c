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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <glib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "logging.h"
#include "package.h"
#include "xml_dump.h"

#undef MODULE
#define MODULE "xml_dump_other: "

#define FORMAT_XML      1
#define FORMAT_LEVEL    0

#define DATE_MAX_LEN    32


void
dump_changelog(xmlNodePtr root, cr_Package *package)
{
    if (!package->changelogs) {
        return;
    }

    GSList *element = NULL;
    for(element = package->changelogs; element; element=element->next) {

        cr_ChangelogEntry *entry = (cr_ChangelogEntry*) element->data;

        assert(entry);


        // ***********************************
        // Element: Changelog
        // ************************************

        xmlNodePtr changelog;

        // Add changelog element
        changelog = xmlNewTextChild(root,
                                    NULL,
                                    BAD_CAST "changelog",
                                    BAD_CAST ((entry->changelog) ? entry->changelog : ""));

        // Write param author
        xmlNewProp(changelog,
                   BAD_CAST "author",
                   BAD_CAST ((entry->author) ? entry->author : ""));

        // Write param date
        char date_str[DATE_MAX_LEN];
        g_snprintf(date_str, DATE_MAX_LEN, "%lld", (long long int) entry->date);
        xmlNewProp(changelog, BAD_CAST "date", BAD_CAST date_str);
    }
}


void
dump_other_items(xmlNodePtr root, cr_Package *package)
{
    /***********************************
     Element: package
    ************************************/

    // Add pkgid attribute
    xmlNewProp(root, BAD_CAST "pkgid",
               BAD_CAST ((package->pkgId) ? package->pkgId : ""));

    // Add name attribute
    xmlNewProp(root, BAD_CAST "name",
               BAD_CAST ((package->name) ? package->name : ""));

    // Add arch attribute
    xmlNewProp(root, BAD_CAST "arch",
               BAD_CAST ((package->arch) ? package->arch : ""));


    /***********************************
     Element: version
    ************************************/

    xmlNodePtr version;

    // Add version element
    version = xmlNewChild(root, NULL, BAD_CAST "version", NULL);

    // Write version attribute epoch
    xmlNewProp(version, BAD_CAST "epoch",
               BAD_CAST ((package->epoch) ? package->epoch : ""));

    // Write version attribute ver
    xmlNewProp(version, BAD_CAST "ver",
               BAD_CAST ((package->version) ? package->version : ""));

    // Write version attribute rel
    xmlNewProp(version, BAD_CAST "rel",
               BAD_CAST ((package->release) ? package->release : ""));


    // Changelog dump

    dump_changelog(root, package);
}


char *
cr_xml_dump_other(cr_Package *package)
{
    if (!package)
        return NULL;

    xmlNodePtr root = NULL;
    root = xmlNewNode(NULL, BAD_CAST "package");


    // Dump IT!

    dump_other_items(root, package);

    char *result;
    xmlBufferPtr buf = xmlBufferCreate();
    if (buf == NULL) {
        g_critical(MODULE"%s: Error creating the xml buffer", __func__);
        return NULL;
    }
    // Seems to be little bit faster than xmlDocDumpFormatMemory
    xmlNodeDump(buf, NULL, root, FORMAT_LEVEL, FORMAT_XML);
    assert(buf->content);
    result = g_strndup((char *) buf->content, (buf->use+1));
    result[buf->use]     = '\n';
    result[buf->use+1]   = '\0';
    xmlBufferFree(buf);


    // Cleanup

    xmlFreeNode(root);

    return result;
}
