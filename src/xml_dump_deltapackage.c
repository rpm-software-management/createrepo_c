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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlsave.h>
#include "deltarpms.h"
#include "error.h"
#include "misc.h"
#include "package.h"
#include "xml_dump.h"
#include "xml_dump_internal.h"

#define ERR_DOMAIN      CREATEREPO_C_ERROR
#define INDENT          4

void
cr_xml_dump_delta(xmlNodePtr root, cr_DeltaPackage *package)
{
    /***********************************
     Element: delta
    ************************************/

    cr_NEVR * nevr = cr_str_to_nevr(package->nevr);

    // Add oldepoch attribute
    cr_xmlNewProp(root, BAD_CAST "oldepoch",
                  BAD_CAST ((nevr->epoch && *(nevr->epoch)) ? nevr->epoch : "0"));

    // Add oldversion attribute
    cr_xmlNewProp(root, BAD_CAST "oldversion", BAD_CAST nevr->version);

    // Add oldrelease attribute
    cr_xmlNewProp(root, BAD_CAST "oldrelease", BAD_CAST nevr->release);

    cr_nevr_free(nevr);

    /***********************************
     Element: filename
    ************************************/

    cr_xmlNewTextChild(root, NULL,
                       BAD_CAST "filename",
                       BAD_CAST package->package->location_href);

    /***********************************
    Element: sequence
    ************************************/

    char *sequence = g_strconcat(package->nevr, "-", package->sequence, NULL);
    cr_xmlNewTextChild(root, NULL,
                       BAD_CAST "sequence",
                       BAD_CAST sequence);
    g_free(sequence);

    /***********************************
     Element: size
    ************************************/

    char size_str[SIZE_STR_MAX_LEN];

    g_snprintf(size_str, SIZE_STR_MAX_LEN, "%"G_GINT64_FORMAT,
               package->package->size_package);

    cr_xmlNewTextChild(root, NULL, BAD_CAST "size", BAD_CAST size_str);

    /***********************************
     Element: checksum
    ************************************/

    xmlNodePtr checksum;

    checksum = cr_xmlNewTextChild(root,
                                  NULL,
                                  BAD_CAST "checksum",
                                  BAD_CAST package->package->pkgId);

    cr_xmlNewProp(checksum,
                  BAD_CAST "type",
                  BAD_CAST package->package->checksum_type);
}


char *
cr_xml_dump_deltapackage(cr_DeltaPackage *package, GError **err)
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

    root = xmlNewNode(NULL, BAD_CAST "delta");
    cr_xml_dump_delta(root, package);
    // xmlNodeDump seems to be a little bit faster than xmlDocDumpFormatMemory
    xmlNodeDump(buf, NULL, root, 2, FORMAT_XML);
    assert(buf->content);
    // First line in the buf is not indented, we must indent it by ourself
    result = g_malloc(sizeof(char *) * buf->use + INDENT + 1);
    for (int x = 0; x < INDENT; x++) result[x] = ' ';
    memcpy((void *) result+INDENT, buf->content, buf->use);
    result[buf->use + INDENT]   = '\n';
    result[buf->use + INDENT + 1]   = '\0';


    // Cleanup

    xmlBufferFree(buf);
    xmlFreeNode(root);

    return result;
}
