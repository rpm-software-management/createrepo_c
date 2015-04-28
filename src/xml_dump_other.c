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
#include <stdio.h>
#include <string.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlsave.h>
#include "error.h"
#include "package.h"
#include "xml_dump.h"
#include "xml_dump_internal.h"

#define ERR_DOMAIN      CREATEREPO_C_ERROR


void
cr_xml_dump_other_changelog(xmlNodePtr root, cr_Package *package)
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
        // ***********************************

        xmlNodePtr changelog;

        // Add changelog element
        changelog = cr_xmlNewTextChild(root,
                                       NULL,
                                       BAD_CAST "changelog",
                                       BAD_CAST entry->changelog);

        // Write param author
        cr_xmlNewProp(changelog, BAD_CAST "author", BAD_CAST entry->author);

        // Write param date
        char date_str[DATE_STR_MAX_LEN];
        g_snprintf(date_str, DATE_STR_MAX_LEN, "%"G_GINT64_FORMAT, entry->date);
        xmlNewProp(changelog, BAD_CAST "date", BAD_CAST date_str);
    }
}


void
cr_xml_dump_other_items(xmlNodePtr root, cr_Package *package)
{
    /***********************************
     Element: package
    ************************************/

    // Add pkgid attribute
    cr_xmlNewProp(root, BAD_CAST "pkgid", BAD_CAST package->pkgId);

    // Add name attribute
    cr_xmlNewProp(root, BAD_CAST "name", BAD_CAST package->name);

    // Add arch attribute
    cr_xmlNewProp(root, BAD_CAST "arch", BAD_CAST package->arch);


    /***********************************
     Element: version
    ************************************/

    xmlNodePtr version;

    // Add version element
    version = xmlNewChild(root, NULL, BAD_CAST "version", NULL);

    // Write version attribute epoch
    xmlNewProp(version, BAD_CAST "epoch", BAD_CAST package->epoch);

    // Write version attribute ver
    xmlNewProp(version, BAD_CAST "ver", BAD_CAST package->version);

    // Write version attribute rel
    xmlNewProp(version, BAD_CAST "rel", BAD_CAST package->release);


    // Changelog dump

    cr_xml_dump_other_changelog(root, package);
}


char *
cr_xml_dump_other(cr_Package *package, GError **err)
{
    xmlNodePtr root;
    char *result;

    assert(!err || *err == NULL);

    if (!package) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_BADARG,
                    "No package object to dump specified");
        return NULL;
    }


    // Dump IT!

    xmlBufferPtr buf = xmlBufferCreate();
    if (buf == NULL) {
        g_critical("%s: Error creating the xml buffer", __func__);
        g_set_error(err, ERR_DOMAIN, CRE_MEMORY,
                    "Cannot create an xml buffer");
        return NULL;
    }

    root = xmlNewNode(NULL, BAD_CAST "package");
    cr_xml_dump_other_items(root, package);
    // xmlNodeDump seems to be a little bit faster than xmlDocDumpFormatMemory
    xmlNodeDump(buf, NULL, root, FORMAT_LEVEL, FORMAT_XML);
    assert(buf->content);
    result = g_strndup((char *) buf->content, (buf->use+1));
    result[buf->use]     = '\n';
    result[buf->use+1]   = '\0';


    // Cleanup

    xmlBufferFree(buf);
    xmlFreeNode(root);

    return result;
}
