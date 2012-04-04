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
#define MODULE "xml_dump_primary: "

#define PROVIDES    0
#define CONFLICTS   1
#define OBSOLETES   2
#define REQUIRES    3

#define FORMAT_XML  1

#define DATE_STR_MAX_LEN    32
#define SIZE_STR_MAX_LEN    32


void dump_pco(xmlNodePtr root, Package *package, int pcotype)
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

        Dependency *entry = (Dependency*) element->data;

        assert(entry);

        if (!entry->name || entry->name[0] == '\0') {
            continue;
        }


        /***********************************
        Element: entry
        ************************************/

        xmlNodePtr entry_node;

        entry_node = xmlNewChild(pcor_node, NULL, BAD_CAST "rpm:entry", NULL);
        xmlNewProp(entry_node, BAD_CAST "name", BAD_CAST entry->name);

        if (entry->flags && entry->flags[0] != '\0') {
            xmlNewProp(entry_node, BAD_CAST "flags", BAD_CAST entry->flags);

            if (entry->epoch && entry->epoch[0] != '\0') {
                xmlNewProp(entry_node, BAD_CAST "epoch", BAD_CAST entry->epoch);
            }

            if (entry->version && entry->version[0] != '\0') {
                xmlNewProp(entry_node, BAD_CAST "ver", BAD_CAST entry->version);
            }

            if (entry->release && entry->release[0] != '\0') {
                xmlNewProp(entry_node, BAD_CAST "rel", BAD_CAST entry->release);
            }
        }

        if (pcotype == REQUIRES && entry->pre) {
            // Add pre attribute
            xmlNewProp(entry_node, BAD_CAST "pre", BAD_CAST "1");
        }
    }
}



void dump_base_items(xmlNodePtr root, Package *package)
{
    /***********************************
     Element: package
    ************************************/

    // Add an attribute with type to package
    xmlNewProp(root, BAD_CAST "type", BAD_CAST "rpm");


    /***********************************
     Element: name
    ************************************/

    xmlNewTextChild(root, NULL,  BAD_CAST "name", BAD_CAST ((package->name) ? package->name : ""));


    /***********************************
     Element: arch
    ************************************/

    xmlNewTextChild(root, NULL,  BAD_CAST "arch", BAD_CAST ((package->arch) ? package->arch : ""));


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


    /***********************************
     Element: checksum
    ************************************/

    xmlNodePtr checksum;

    checksum = xmlNewTextChild(root, NULL,  BAD_CAST "checksum", BAD_CAST ((package->pkgId) ? package->pkgId : ""));

    // Write checksum attribute checksum_type
    xmlNewProp(checksum, BAD_CAST "type", BAD_CAST ((package->checksum_type) ? package->checksum_type : ""));

    // Write checksum attribute pkgid
    xmlNewProp(checksum, BAD_CAST "pkgid", BAD_CAST "YES");


    /***********************************
     Element: summary
    ************************************/

    xmlNewTextChild(root, NULL,  BAD_CAST "summary", BAD_CAST ((package->summary) ? package->summary : ""));


    /***********************************
    Element: description
    ************************************/

    xmlNewTextChild(root, NULL,  BAD_CAST "description", BAD_CAST ((package->description) ? package->description : ""));


    /***********************************
     Element: packager
    ************************************/

    xmlNewTextChild(root, NULL,  BAD_CAST "packager", BAD_CAST ((package->rpm_packager) ? package->rpm_packager : ""));


    /***********************************
     Element: url
    ************************************/

    xmlNewTextChild(root, NULL,  BAD_CAST "url", BAD_CAST ((package->url) ? package->url : ""));


    /***********************************
     Element: time
    ************************************/

    xmlNodePtr time;
    char date_str[DATE_STR_MAX_LEN];

    time = xmlNewChild(root, NULL, BAD_CAST "time", NULL);

    // Write time attribute file
    g_snprintf(date_str, DATE_STR_MAX_LEN, "%lld", (long long int) package->time_file);
    xmlNewProp(time, BAD_CAST "file", BAD_CAST date_str);

    // Write time attribute build
    g_snprintf(date_str, DATE_STR_MAX_LEN, "%lld", (long long int) package->time_build);
    xmlNewProp(time, BAD_CAST "build", BAD_CAST date_str);


    /***********************************
     Element: size
    ************************************/

    xmlNodePtr size;
    char size_str[SIZE_STR_MAX_LEN];

    size = xmlNewChild(root, NULL, BAD_CAST "size", NULL);

    // Write size attribute package
    g_snprintf(size_str, SIZE_STR_MAX_LEN, "%lld", (long long int) package->size_package);
    xmlNewProp(size, BAD_CAST "package", BAD_CAST size_str);

    // Write size attribute installed
    g_snprintf(size_str, SIZE_STR_MAX_LEN, "%lld", (long long int) package->size_installed);
    xmlNewProp(size, BAD_CAST "installed", BAD_CAST size_str);

    // Write size attribute archive
    g_snprintf(size_str, SIZE_STR_MAX_LEN, "%lld", (long long int) package->size_archive);
    xmlNewProp(size, BAD_CAST "archive", BAD_CAST size_str);


    /***********************************
     Element: location
    ************************************/

    xmlNodePtr location;

    location = xmlNewChild(root, NULL, BAD_CAST "location", NULL);

    // Write location attribute href
    xmlNewProp(location, BAD_CAST "href", BAD_CAST ((package->location_href) ? package->location_href : ""));

    // Write location attribute base
    if (package->location_base && package->location_base[0] != '\0') {
        xmlNewProp(location, BAD_CAST "xml:base", BAD_CAST package->location_base);
    }


    /***********************************
     Element: format
    ************************************/

    xmlNodePtr format;

    format = xmlNewChild(root, NULL, BAD_CAST "format", NULL);


    /***********************************
     Element: license
    ************************************/

    xmlNewTextChild(format, NULL,  BAD_CAST "rpm:license", BAD_CAST ((package->rpm_license) ? package->rpm_license : ""));


    /***********************************
     Element: vendor
    ************************************/

    xmlNewTextChild(format, NULL,  BAD_CAST "rpm:vendor", BAD_CAST ((package->rpm_vendor) ? package->rpm_vendor : ""));


    /***********************************
     Element: group
    ************************************/

    xmlNewTextChild(format, NULL,  BAD_CAST "rpm:group", BAD_CAST ((package->rpm_group) ? package->rpm_group : ""));


    /***********************************
     Element: buildhost
    ************************************/

    xmlNewTextChild(format, NULL,  BAD_CAST "rpm:buildhost", BAD_CAST ((package->rpm_buildhost) ? package->rpm_buildhost : ""));


    /***********************************
     Element: sourcerpm
    ************************************/

    xmlNewTextChild(format, NULL,  BAD_CAST "rpm:sourcerpm", BAD_CAST ((package->rpm_sourcerpm) ? package->rpm_sourcerpm : ""));


    /***********************************
     Element: header-range
    ************************************/

    xmlNodePtr header_range;

    header_range = xmlNewChild(format, NULL, BAD_CAST "rpm:header-range", NULL);

    // Write header-range attribute hdrstart
    g_snprintf(size_str, SIZE_STR_MAX_LEN, "%lld", (long long int) package->rpm_header_start);
    xmlNewProp(header_range, BAD_CAST "start", BAD_CAST size_str);

    // Write header-range attribute hdrend
    g_snprintf(size_str, SIZE_STR_MAX_LEN, "%lld", (long long int) package->rpm_header_end);
    xmlNewProp(header_range, BAD_CAST "end", BAD_CAST size_str);


    // Files dump

    dump_pco(format,   package, PROVIDES);
    dump_pco(format,   package, REQUIRES);
    dump_pco(format,   package, CONFLICTS);
    dump_pco(format,   package, OBSOLETES);
    dump_files(format, package, 1);
}



char *xml_dump_primary(Package *package)
{
    xmlNodePtr root = NULL;
    root = xmlNewNode(NULL, BAD_CAST "package");


    // Dump IT!

    dump_base_items(root, package);

    char *result;
    xmlBufferPtr buf = xmlBufferCreate();
    if (buf == NULL) {
        g_critical(MODULE"%s: Error creating the xml buffer", __func__);
        return NULL;
    }
    // Seems to be little bit faster than xmlDocDumpFormatMemory
    xmlNodeDump(buf, NULL, root, 1, FORMAT_XML);
    assert(buf->content);
    result = g_strdup((char *) buf->content);
    xmlBufferFree(buf);


    // Cleanup

    xmlFreeNode(root);

    return result;

}
