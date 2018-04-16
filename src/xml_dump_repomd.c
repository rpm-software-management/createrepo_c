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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlsave.h>
#include "error.h"
#include "repomd.h"
#include "xml_dump.h"
#include "xml_dump_internal.h"


void
cr_xml_dump_repomd_record(xmlNodePtr root, cr_RepomdRecord *rec)
{
    xmlNodePtr data, node;
    gchar str_buffer[DATESIZE_STR_MAX_LEN];

    if (!rec)
        return;

    // Data element
    data = xmlNewChild(root, NULL, BAD_CAST "data", NULL);
    xmlNewProp(data, BAD_CAST "type", BAD_CAST rec->type);

    // Checksum element
    node = cr_xmlNewTextChild(data,
                              NULL,
                              BAD_CAST "checksum",
                              BAD_CAST rec->checksum);
    cr_xmlNewProp(node,
                  BAD_CAST "type",
                  BAD_CAST rec->checksum_type);

    // Checksum_open element
    if (rec->checksum_open) {
        node = cr_xmlNewTextChild(data,
                                  NULL,
                                  BAD_CAST "open-checksum",
                                  BAD_CAST rec->checksum_open);
        cr_xmlNewProp(node,
                      BAD_CAST "type",
                      BAD_CAST rec->checksum_open_type);
    }

    // Checksum_header element
    if (rec->checksum_header) {
        node = cr_xmlNewTextChild(data,
                                  NULL,
                                  BAD_CAST "header-checksum",
                                  BAD_CAST rec->checksum_header);
        cr_xmlNewProp(node,
                      BAD_CAST "type",
                      BAD_CAST rec->checksum_header_type);
    }

    // Location element
    node = xmlNewChild(data,
                       NULL,
                       BAD_CAST "location",
                       NULL);
    cr_xmlNewProp(node,
                  BAD_CAST "href",
                  BAD_CAST rec->location_href);
    if (rec->location_base)
        cr_xmlNewProp(node,
                      BAD_CAST "xml:base",
                      BAD_CAST rec->location_base);


    // Timestamp element
    g_snprintf(str_buffer, DATESIZE_STR_MAX_LEN,
               "%"G_GINT64_FORMAT, rec->timestamp);
    xmlNewChild(data, NULL, BAD_CAST "timestamp", BAD_CAST str_buffer);

    // Size element
    g_snprintf(str_buffer, DATESIZE_STR_MAX_LEN,
               "%"G_GINT64_FORMAT, rec->size);
    xmlNewChild(data, NULL, BAD_CAST "size", BAD_CAST str_buffer);

    // Open-size element
    if (rec->size_open != -1) {
        g_snprintf(str_buffer, DATESIZE_STR_MAX_LEN,
                   "%"G_GINT64_FORMAT, rec->size_open);
        xmlNewChild(data, NULL, BAD_CAST "open-size", BAD_CAST str_buffer);
    }

    // Header-size element
    if (rec->checksum_header && rec->size_header != -1) {
        g_snprintf(str_buffer, DATESIZE_STR_MAX_LEN,
                   "%"G_GINT64_FORMAT, rec->size_header);
        xmlNewChild(data, NULL, BAD_CAST "header-size", BAD_CAST str_buffer);
    }

    // Database_version element
    if (g_str_has_suffix((char *) rec->type, "_db")) {
        g_snprintf(str_buffer, DATESIZE_STR_MAX_LEN, "%d", rec->db_ver);
        xmlNewChild(data,
                    NULL,
                    BAD_CAST "database_version",
                    BAD_CAST str_buffer);
    }
}


void
cr_xml_dump_repomd_body(xmlNodePtr root, cr_Repomd *repomd)
{
    GSList *element;

    // Add namespaces to the root element
    xmlNewNs(root, BAD_CAST CR_XML_REPOMD_NS, BAD_CAST NULL);
    xmlNewNs(root, BAD_CAST CR_XML_RPM_NS, BAD_CAST "rpm");

    // **********************************
    // Element: Revision
    // **********************************

    if (repomd->revision) {
        cr_xmlNewTextChild(root,
                           NULL,
                           BAD_CAST "revision",
                           BAD_CAST repomd->revision);
    } else {
        // Use the current time if no revision was explicitly specified
        gchar *rev = g_strdup_printf("%ld", time(NULL));
        xmlNewChild(root, NULL, BAD_CAST "revision", BAD_CAST rev);
        g_free(rev);
    }

    // **********************************
    // Element: Repoid
    // **********************************

    if (repomd->repoid) {
        xmlNodePtr repoid_elem = cr_xmlNewTextChild(root,
                                                    NULL,
                                                    BAD_CAST "repoid",
                                                    BAD_CAST repomd->repoid);
        if (repomd->repoid_type)
            cr_xmlNewProp(repoid_elem,
                          BAD_CAST "type",
                          BAD_CAST repomd->repoid_type);
    }

    // **********************************
    // Element: Contenthash
    // **********************************

    if (repomd->contenthash) {
        xmlNodePtr contenthash_elem = cr_xmlNewTextChild(root,
                                                NULL,
                                                BAD_CAST "contenthash",
                                                BAD_CAST repomd->contenthash);
        if (repomd->contenthash_type)
            cr_xmlNewProp(contenthash_elem,
                          BAD_CAST "type",
                          BAD_CAST repomd->contenthash_type);
    }

    // **********************************
    // Element: Tags
    // **********************************

    if (repomd->repo_tags || repomd->distro_tags || repomd->content_tags) {
        GSList *element;
        xmlNodePtr tags = xmlNewChild(root, NULL, BAD_CAST "tags", NULL);

        // Content tags
        element = repomd->content_tags;
        for (; element; element = g_slist_next(element))
            cr_xmlNewTextChild(tags, NULL,
                               BAD_CAST "content",
                               BAD_CAST element->data);

        // Repo tags
        element = repomd->repo_tags;
        for (; element; element = g_slist_next(element))
            cr_xmlNewTextChild(tags, NULL,
                               BAD_CAST "repo",
                               BAD_CAST element->data);

        // Distro tags
        element = repomd->distro_tags;
        for (; element; element = g_slist_next(element)) {
            cr_DistroTag *distro = (cr_DistroTag *) element->data;
            xmlNodePtr distro_elem = cr_xmlNewTextChild(tags,
                                                        NULL,
                                                        BAD_CAST "distro",
                                                        BAD_CAST distro->val);
            // Cpeid attribute of distro tag
            if (distro->cpeid)
                cr_xmlNewProp(distro_elem,
                              BAD_CAST "cpeid",
                              BAD_CAST distro->cpeid);
        }
    }

    // Dump records

    for (element = repomd->records; element; element = g_slist_next(element)) {
        cr_RepomdRecord *rec = element->data;
        cr_xml_dump_repomd_record(root, rec);
    }
}


char *
cr_xml_dump_repomd(cr_Repomd *repomd, GError **err)
{
    xmlDocPtr doc;
    xmlNodePtr root;
    char *result;

    assert(!err || *err == NULL);

    if (!repomd) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_BADARG,
                    "No repomd object to dump specified");
        return NULL;
    }


    // Dump IT!

    doc = xmlNewDoc(BAD_CAST XML_DOC_VERSION);
    root = xmlNewNode(NULL, BAD_CAST "repomd");
    cr_xml_dump_repomd_body(root, repomd);
    xmlDocSetRootElement(doc, root);
    xmlDocDumpFormatMemoryEnc(doc,
                              (xmlChar **) &result,
                              NULL,
                              XML_ENCODING,
                              FORMAT_XML);

    // Clean up

    xmlFreeDoc(doc);

    return result;
}
