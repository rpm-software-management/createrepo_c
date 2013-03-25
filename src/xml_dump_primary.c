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
#include "logging.h"
#include "package.h"
#include "xml_dump.h"
#include "xml_dump_internal.h"

#define PROVIDES    0
#define CONFLICTS   1
#define OBSOLETES   2
#define REQUIRES    3

#define FORMAT_XML      1
#define FORMAT_LEVEL    0

#define DATE_STR_MAX_LEN    32
#define SIZE_STR_MAX_LEN    32


void
cr_xml_dump_primary_dump_pco(xmlNodePtr root, cr_Package *package, int pcotype)
{
    const char *elem_name;
    GSList *files = NULL;

    const char *provides  = "rpm:provides";
    const char *conflicts = "rpm:conflicts";
    const char *obsoletes = "rpm:obsoletes";
    const char *requires  = "rpm:requires";

    if (pcotype == PROVIDES) {
        elem_name = provides;
        files = package->provides;
    } else if (pcotype == CONFLICTS) {
        elem_name = conflicts;
        files = package->conflicts;
    } else if (pcotype == OBSOLETES) {
        elem_name = obsoletes;
        files = package->obsoletes;
    } else if (pcotype == REQUIRES) {
        elem_name = requires;
        files = package->requires;
    } else {
        return;
    }

    if (!files) {
        return;
    }


    /***********************************
     PCOR Element: provides, oboletes, conflicts, requires
    ************************************/

    xmlNodePtr pcor_node;

    pcor_node = xmlNewChild(root, NULL, BAD_CAST elem_name, NULL);

    GSList *element = NULL;
    for(element = files; element; element=element->next) {

        cr_Dependency *entry = (cr_Dependency*) element->data;

        assert(entry);

        if (!entry->name || entry->name[0] == '\0') {
            continue;
        }


        /***********************************
         Element: entry
        ************************************/

        xmlNodePtr entry_node;

        entry_node = xmlNewChild(pcor_node, NULL, BAD_CAST "rpm:entry", NULL);
        cr_xmlNewProp(entry_node, BAD_CAST "name", BAD_CAST entry->name);

        if (entry->flags && entry->flags[0] != '\0') {
            cr_xmlNewProp(entry_node, BAD_CAST "flags", BAD_CAST entry->flags);

            if (entry->epoch && entry->epoch[0] != '\0') {
                cr_xmlNewProp(entry_node, BAD_CAST "epoch", BAD_CAST entry->epoch);
            }

            if (entry->version && entry->version[0] != '\0') {
                cr_xmlNewProp(entry_node, BAD_CAST "ver", BAD_CAST entry->version);
            }

            if (entry->release && entry->release[0] != '\0') {
                cr_xmlNewProp(entry_node, BAD_CAST "rel", BAD_CAST entry->release);
            }
        }

        if (pcotype == REQUIRES && entry->pre) {
            // Add pre attribute
            xmlNewProp(entry_node, BAD_CAST "pre", BAD_CAST "1");
        }
    }
}



void
cr_xml_dump_primary_base_items(xmlNodePtr root, cr_Package *package)
{
    /***********************************
     Element: package
    ************************************/

    // Add an attribute with type to package
    xmlNewProp(root, BAD_CAST "type", BAD_CAST "rpm");


    /***********************************
     Element: name
    ************************************/

    cr_xmlNewTextChild(root, NULL,  BAD_CAST "name", BAD_CAST package->name);


    /***********************************
     Element: arch
    ************************************/

    cr_xmlNewTextChild(root, NULL,  BAD_CAST "arch", BAD_CAST package->arch);


    /***********************************
     Element: version
    ************************************/

    xmlNodePtr version;

    // Add version element
    version = xmlNewChild(root, NULL, BAD_CAST "version", NULL);

    // Write version attribute epoch
    cr_xmlNewProp(version, BAD_CAST "epoch", BAD_CAST package->epoch);

    // Write version attribute ver
    cr_xmlNewProp(version, BAD_CAST "ver", BAD_CAST package->version);

    // Write version attribute rel
    cr_xmlNewProp(version, BAD_CAST "rel", BAD_CAST package->release);


    /***********************************
     Element: checksum
    ************************************/

    xmlNodePtr checksum;

    checksum = cr_xmlNewTextChild(root,
                                  NULL,
                                  BAD_CAST "checksum",
                                  BAD_CAST package->pkgId);

    // Write checksum attribute checksum_type
    cr_xmlNewProp(checksum, BAD_CAST "type", BAD_CAST package->checksum_type);

    // Write checksum attribute pkgid
    xmlNewProp(checksum, BAD_CAST "pkgid", BAD_CAST "YES");


    /***********************************
     Element: summary
    ************************************/

    cr_xmlNewTextChild(root, NULL,  BAD_CAST "summary",
                       BAD_CAST package->summary);


    /***********************************
    Element: description
    ************************************/

    cr_xmlNewTextChild(root, NULL,  BAD_CAST "description",
                       BAD_CAST package->description);


    /***********************************
     Element: packager
    ************************************/

    cr_xmlNewTextChild(root, NULL,  BAD_CAST "packager",
                       BAD_CAST package->rpm_packager);


    /***********************************
     Element: url
    ************************************/

    cr_xmlNewTextChild(root, NULL,  BAD_CAST "url", BAD_CAST package->url);


    /***********************************
     Element: time
    ************************************/

    xmlNodePtr time;
    char date_str[DATE_STR_MAX_LEN];

    time = xmlNewChild(root, NULL, BAD_CAST "time", NULL);

    // Write time attribute file
    g_snprintf(date_str, DATE_STR_MAX_LEN, "%lld",
               (long long int) package->time_file);
    xmlNewProp(time, BAD_CAST "file", BAD_CAST date_str);

    // Write time attribute build
    g_snprintf(date_str, DATE_STR_MAX_LEN, "%lld",
               (long long int) package->time_build);
    xmlNewProp(time, BAD_CAST "build", BAD_CAST date_str);


    /***********************************
     Element: size
    ************************************/

    xmlNodePtr size;
    char size_str[SIZE_STR_MAX_LEN];

    size = xmlNewChild(root, NULL, BAD_CAST "size", NULL);

    // Write size attribute package
    g_snprintf(size_str, SIZE_STR_MAX_LEN, "%lld",
               (long long int) package->size_package);
    xmlNewProp(size, BAD_CAST "package", BAD_CAST size_str);

    // Write size attribute installed
    g_snprintf(size_str, SIZE_STR_MAX_LEN, "%lld",
               (long long int) package->size_installed);
    xmlNewProp(size, BAD_CAST "installed", BAD_CAST size_str);

    // Write size attribute archive
    g_snprintf(size_str, SIZE_STR_MAX_LEN, "%lld",
               (long long int) package->size_archive);
    xmlNewProp(size, BAD_CAST "archive", BAD_CAST size_str);


    /***********************************
     Element: location
    ************************************/

    xmlNodePtr location;

    location = xmlNewChild(root, NULL, BAD_CAST "location", NULL);

    // Write location attribute base
    if (package->location_base && package->location_base[0] != '\0') {
        cr_xmlNewProp(location,
                      BAD_CAST "xml:base",
                      BAD_CAST package->location_base);
    }

    // Write location attribute href
    cr_xmlNewProp(location, BAD_CAST "href", BAD_CAST package->location_href);


    /***********************************
     Element: format
    ************************************/

    xmlNodePtr format;

    format = xmlNewChild(root, NULL, BAD_CAST "format", NULL);


    /***********************************
     Element: license
    ************************************/

    cr_xmlNewTextChild(format, NULL,  BAD_CAST "rpm:license",
                       BAD_CAST package->rpm_license);


    /***********************************
     Element: vendor
    ************************************/

    cr_xmlNewTextChild(format, NULL,  BAD_CAST "rpm:vendor",
                       BAD_CAST package->rpm_vendor);


    /***********************************
     Element: group
    ************************************/

    cr_xmlNewTextChild(format, NULL,  BAD_CAST "rpm:group",
                       BAD_CAST package->rpm_group);


    /***********************************
     Element: buildhost
    ************************************/

    cr_xmlNewTextChild(format, NULL,  BAD_CAST "rpm:buildhost",
                       BAD_CAST package->rpm_buildhost);


    /***********************************
     Element: sourcerpm
    ************************************/

    cr_xmlNewTextChild(format, NULL,  BAD_CAST "rpm:sourcerpm",
                       BAD_CAST package->rpm_sourcerpm);


    /***********************************
     Element: header-range
    ************************************/

    xmlNodePtr header_range;

    header_range = xmlNewChild(format, NULL, BAD_CAST "rpm:header-range", NULL);

    // Write header-range attribute hdrstart
    g_snprintf(size_str,
               SIZE_STR_MAX_LEN,
               "%lld",
               (long long int) package->rpm_header_start);
    xmlNewProp(header_range, BAD_CAST "start", BAD_CAST size_str);

    // Write header-range attribute hdrend
    g_snprintf(size_str,
               SIZE_STR_MAX_LEN,
               "%lld",
               (long long int) package->rpm_header_end);
    xmlNewProp(header_range, BAD_CAST "end", BAD_CAST size_str);


    // Files dump

    cr_xml_dump_primary_dump_pco(format,   package, PROVIDES);
    cr_xml_dump_primary_dump_pco(format,   package, REQUIRES);
    cr_xml_dump_primary_dump_pco(format,   package, CONFLICTS);
    cr_xml_dump_primary_dump_pco(format,   package, OBSOLETES);
    cr_xml_dump_files(format, package, 1);
}



char *
cr_xml_dump_primary(cr_Package *package)
{
    if (!package)
        return NULL;

    xmlNodePtr root = NULL;
    root = xmlNewNode(NULL, BAD_CAST "package");


    // Dump IT!

    cr_xml_dump_primary_base_items(root, package);

    char *result;
    xmlBufferPtr buf = xmlBufferCreate();
    if (buf == NULL) {
        g_critical("%s: Error creating the xml buffer", __func__);
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
