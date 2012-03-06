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


void dump_pco(xmlTextWriterPtr writer, Package *package, int pcotype)
{
    int rc;
    const char *elem_name = NULL;
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

    rc = xmlTextWriterStartElement(writer, BAD_CAST elem_name);
    if (rc < 0) {
        g_critical(MODULE"dump_pco: Error at xmlTextWriterWriteElement");
        return;
    }

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

        rc = xmlTextWriterStartElement(writer, BAD_CAST "rpm:entry");
        if (rc < 0) {
            g_critical(MODULE"dump_pco: Error at xmlTextWriterWriteElement");
            return;
        }

        rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "name", "%s", entry->name);
        if (rc < 0) {
             g_critical(MODULE"dump_pco: Error at xmlTextWriterWriteFormatAttribute");
             return;
        }

        if (entry->flags && entry->flags[0] != '\0') {
            rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "flags", "%s", entry->flags);
            if (rc < 0) {
                 g_critical(MODULE"dump_pco: Error at xmlTextWriterWriteFormatAttribute");
                 return;
            }

            if (entry->epoch && entry->epoch[0] != '\0') {
                rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "epoch", "%s", entry->epoch);
                if (rc < 0) {
                     g_critical(MODULE"dump_pco: Error at xmlTextWriterWriteFormatAttribute");
                     return;
                }
            }

            if (entry->version && entry->version[0] != '\0') {
                rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "ver", "%s", entry->version);
                if (rc < 0) {
                     g_critical(MODULE"dump_pco: Error at xmlTextWriterWriteFormatAttribute");
                     return;
                }
            }

            if (entry->release && entry->release[0] != '\0') {
                rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "rel", "%s", entry->release);
                if (rc < 0) {
                     g_critical(MODULE"dump_pco: Error at xmlTextWriterWriteFormatAttribute");
                     return;
                }
            }
        }

        if (pcotype == REQUIRES) {
            // Add pre attribute
            rc = 0;
            if (entry->pre) {
                rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "pre", BAD_CAST "1");
            } else {
                rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "pre", BAD_CAST "0");
            }
            if (rc < 0) {
                 g_critical(MODULE"dump_pco: Error at xmlTextWriterWriteAttribute");
                 return;
            }
        }

        // Close entry element
        rc = xmlTextWriterEndElement(writer);
        if (rc < 0) {
            g_critical(MODULE"dump_pco: Error at xmlTextWriterEndElement");
            return;
        }
    }

    // Close PCOR element
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        g_critical(MODULE"dump_pco: Error at xmlTextWriterEndElement");
        return;
    }
}



void dump_base_items(xmlTextWriterPtr writer, Package *package)
{
    int rc;

    rc = xmlTextWriterStartDocument(writer, NULL, NULL, NULL);
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterStartDocument");
        return;
    }


    /***********************************
    Element: package
    ************************************/

    // Start element package
    rc = xmlTextWriterStartElement(writer, BAD_CAST "package");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterStartElement");
        return;
    }

    // Add an attribute with type to package
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "type", BAD_CAST "rpm");
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteAttribute");
         return;
    }


    /***********************************
    Element: name
    ************************************/

    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "name", "%s", (package->name) ? package->name : "");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatElement");
        return;
    }


    /***********************************
    Element: arch
    ************************************/

    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "arch", "%s", (package->arch) ? package->arch : "");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatElement");
        return;
    }


    /***********************************
    Element: version
    ************************************/

    rc = xmlTextWriterStartElement(writer, BAD_CAST "version");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterStartElement");
        return;
    }

    // Write version attribute epoch
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "epoch", "%s", (package->epoch) ? package->epoch : "0");
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Write version attribute ver
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "ver", "%s", (package->version) ? package->version : "");
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Write version attribute rel
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "rel", "%s", (package->release) ? package->release : "");
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Close version element
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterEndElement");
        return;
    }


    /***********************************
    Element: checksum
    ************************************/

    rc = xmlTextWriterStartElement(writer, BAD_CAST "checksum");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterStartElement");
        return;
    }

    // Write checksum attribute checksum_type
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "type", "%s", (package->checksum_type) ? package->checksum_type : "");
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Write checksum attribute pkgid
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "pkgid", BAD_CAST "YES");
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteAttribute");
         return;
    }

    // Write element string
    rc = xmlTextWriterWriteFormatString(writer, "%s", (package->pkgId) ? package->pkgId : "");
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatString");
         return;
    }

    // Close checksum element
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterEndElement");
        return;
    }


    /***********************************
    Element: summary
    ************************************/

    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "summary", "%s", (package->summary) ? package->summary : "");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatElement");
        return;
    }


    /***********************************
    Element: description
    ************************************/

    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "description", "%s", (package->description) ? package->description : "");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatElement");
        return;
    }


    /***********************************
    Element: packager
    ************************************/

    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "packager", "%s", (package->rpm_packager) ? package->rpm_packager : "");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatElement");
        return;
    }


    /***********************************
    Element: url
    ************************************/

    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "url", "%s", (package->url) ? package->url : "");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatElement");
        return;
    }


    /***********************************
    Element: time
    ************************************/

    rc = xmlTextWriterStartElement(writer, BAD_CAST "time");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterStartElement");
        return;
    }

    // Write time attribute file
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "file", "%lld", (long long int) package->time_file);
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Write time attribute build
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "build", "%lld", (long long int) package->time_build);
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Close time element
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterEndElement");
        return;
    }


    /***********************************
    Element: size
    ************************************/

    rc = xmlTextWriterStartElement(writer, BAD_CAST "size");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterStartElement");
        return;
    }

    // Write size attribute package
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "package", "%lld", (long long int)  package->size_package);
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Write size attribute installed
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "installed", "%lld", (long long int) package->size_installed);
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Write size attribute archive
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "archive", "%lld", (long long int) package->size_archive);
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Close size element
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterEndElement");
        return;
    }


    /***********************************
    Element: location
    ************************************/

    rc = xmlTextWriterStartElement(writer, BAD_CAST "location");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterStartElement");
        return;
    }

    // Write location attribute href
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "href", "%s", (package->location_href) ? package->location_href : "");
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Write location attribute base
    if (package->location_base && package->location_base[0] != '\0') {
        rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "xml:base", "%s", package->location_base);
        if (rc < 0) {
             g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatAttribute");
             return;
        }
    }

    /* Close location element */
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterEndElement");
        return;
    }


    /***********************************
    Element: format
    ************************************/

    rc = xmlTextWriterStartElement(writer, BAD_CAST "format");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterStartElement");
        return;
    }


    /***********************************
    Element: license
    ************************************/

    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "rpm:license", "%s", (package->rpm_license) ? package->rpm_license : "");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatElement");
        return;
    }


    /***********************************
    Element: vendor
    ************************************/

    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "rpm:vendor", "%s", (package->rpm_vendor) ? package->rpm_vendor : "");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatElement");
        return;
    }


    /***********************************
    Element: group
    ************************************/

    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "rpm:group", "%s", (package->rpm_group) ? package->rpm_group : "");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatElement");
        return;
    }


    /***********************************
    Element: buildhost
    ************************************/

    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "rpm:buildhost", "%s", (package->rpm_buildhost) ? package->rpm_buildhost : "");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatElement");
        return;
    }


    /***********************************
    Element: sourcerpm
    ************************************/

    rc = xmlTextWriterWriteFormatElement(writer, BAD_CAST "rpm:sourcerpm", "%s", (package->rpm_sourcerpm) ? package->rpm_sourcerpm : "");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatElement");
        return;
    }


    /***********************************
    Element: header-range
    ************************************/

    rc = xmlTextWriterStartElement(writer, BAD_CAST "rpm:header-range");
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterStartElement");
        return;
    }

    // Write header-range attribute hdrstart
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "start", "%lld", (long long int) package->rpm_header_start);
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Write header-range attribute hdrend
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "end", "%lld", (long long int)  package->rpm_header_end);
    if (rc < 0) {
         g_critical(MODULE"dump_base_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Close header-range element
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterEndElement");
        return;
    }


    // Files dump

    dump_pco(writer,   package, PROVIDES);
    dump_pco(writer,   package, REQUIRES);
    dump_pco(writer,   package, CONFLICTS);
    dump_pco(writer,   package, OBSOLETES);
    dump_files(writer, package, 1);


    // Close format element
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterEndElement");
        return;
    }

    // Close package element
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterEndElement");
        return;
    }

    // Close document (and every still opened tags)
    rc = xmlTextWriterEndDocument(writer);
    if (rc < 0) {
        g_critical(MODULE"dump_base_items: Error at xmlTextWriterEndDocument");
        return;
    }
}



char *xml_dump_primary(Package *package)
{
    xmlBufferPtr buf = xmlBufferCreate();
    if (buf == NULL) {
        g_critical(MODULE"xml_dump_primary: Error creating the xml buffer");
        return NULL;
    }

    xmlTextWriterPtr writer = xmlNewTextWriterMemory(buf, 0);
    if (writer == NULL) {
        g_critical(MODULE"xml_dump_primary: Error creating the xml writer");
        return NULL;
    }

    dump_base_items(writer, package);

    xmlFreeTextWriter(writer);


    // Get XML from xmlBuffer without <?xml ...?> header

    char *pkg_str = strstr((const char*) buf->content, "<package");
    if (!pkg_str) {
        pkg_str = (char*) buf->content;
    }

    char *result = g_strdup(pkg_str);

    xmlBufferFree(buf);

    return result;
}
