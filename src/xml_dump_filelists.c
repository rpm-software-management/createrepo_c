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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "logging.h"
#include "package.h"
#include "xml_dump.h"

#undef MODULE
#define MODULE "xml_dump_filelists: "

#define FORMAT_XML      1
#define FORMAT_LEVEL    0


void dump_filelists_items(xmlNodePtr root, Package *package)
{
    /***********************************
     Element: package
    ************************************/

    // Add pkgid attribute
    xmlNewProp(root, BAD_CAST "pkgid", BAD_CAST ((package->pkgId) ? package->pkgId : ""));

    // Add name attribute
    xmlNewProp(root, BAD_CAST "name", BAD_CAST ((package->name) ? package->name : ""));

    // Add arch attribute
    xmlNewProp(root, BAD_CAST "arch", BAD_CAST ((package->arch) ? package->arch : ""));


    /***********************************
     Element: version
    ************************************/

    xmlNodePtr version;

    // Add version element
    version = xmlNewChild(root, NULL, BAD_CAST "version", NULL);

    // Write version attribute epoch
    xmlNewProp(version, BAD_CAST "epoch", BAD_CAST ((package->epoch) ? package->epoch : ""));

    // Write version attribute ver
    xmlNewProp(version, BAD_CAST "ver", BAD_CAST ((package->version) ? package->version : ""));

    // Write version attribute rel
    xmlNewProp(version, BAD_CAST "rel", BAD_CAST ((package->release) ? package->release : ""));


    // Files dump

    dump_files(root, package, 0);
}


char *xml_dump_filelists(Package *package)
{
    xmlNodePtr root = NULL;
    root = xmlNewNode(NULL, BAD_CAST "package");


    // Dump IT!

    dump_filelists_items(root, package);

    char *result;
    xmlBufferPtr buf = xmlBufferCreate();
    if (buf == NULL) {
        g_critical(MODULE"%s: Error creating the xml buffer", __func__);
        return NULL;
    }
    // Seems to be little bit faster than xmlDocDumpFormatMemory
    xmlNodeDump(buf, NULL, root, FORMAT_LEVEL, FORMAT_XML);
    assert(buf->content);
    result = g_strndup((char *) buf->content, (buf->use+1)); // g_strndup allocate (buf->use+1
    result[buf->use]     = '\n';
    result[buf->use+1]   = '\0';
    xmlBufferFree(buf);


    // Cleanup

    xmlFreeNode(root);

    return result;
}
