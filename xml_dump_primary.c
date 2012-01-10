#include <stdio.h>
#include <string.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "package.h"
#include "xml_dump.h"

#define MY_ENCODING "UTF-8"
//#define DEBUG
#undef DEBUG
//#define DEBUG_REQUIRES

#define PROVIDES    0
#define CONFLICTS   1
#define OBSOLETES   2
#define REQUIRES    3

void
dump_pco(xmlTextWriterPtr writer, Package *package, int pcotype, 
         xmlCharEncodingHandlerPtr handler)
{
#ifdef DEBUG
    printf("CALLED dump_pco\n");
#endif

    xmlChar *tmp = NULL;
    char provides[]  = "rpm:provides";
    char conflicts[] = "rpm:conflicts";
    char obsoletes[] = "rpm:obsoletes";
    char requires[]  = "rpm:requires";

    char *elem_name = NULL;
    GSList *files = NULL;

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
    PCP Element: provides, oboletes, conflicts, requires
    ************************************/
    int rc;
    tmp = ConvertInput(elem_name, handler);
    rc = xmlTextWriterStartElement(writer, BAD_CAST elem_name);
    if (rc < 0) {
        printf("Error at xmlTextWriterWriteElement\n");
        return;
    }
    if (handler && tmp != NULL) xmlFree(tmp);

    GSList *element = NULL;
    for(element = files; element; element=element->next) {

        Dependency *entry = (Dependency*) element->data;

        /***********************************
        Element: entry
        ************************************/
        int rc;
        rc = xmlTextWriterStartElement(writer, BAD_CAST "rpm:entry");
        if (rc < 0) {
            printf("Error at xmlTextWriterWriteElement\n");
            return;
        }

        tmp = ConvertInput(entry->name, handler);
        if (!tmp || ! strlen(tmp)) {
            if (handler && tmp != NULL) xmlFree(tmp);
            goto close;
        }
        rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "name", tmp);
        if (rc < 0) {
             printf("Error at xmlTextWriterWriteAttribute\n");
             return;
        }
        if (handler && tmp != NULL) xmlFree(tmp);

        tmp = ConvertInput(entry->flags, handler);
        if (!tmp || ! strlen(tmp)) {
            if (handler && tmp != NULL) xmlFree(tmp);
            goto close;
        }
        rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "flags", tmp);
        if (rc < 0) {
             printf("Error at xmlTextWriterWriteAttribute\n");
             return;
        }
        if (handler && tmp != NULL) xmlFree(tmp);

        tmp = ConvertInput(entry->epoch, handler);
        if (tmp && strlen(tmp)) {
            rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "epoch", tmp);
            if (rc < 0) {
                 printf("Error at xmlTextWriterWriteAttribute\n");
                 return;
            }
        }
        if (handler && tmp != NULL) xmlFree(tmp);

        tmp = ConvertInput(entry->version, handler);
        if (tmp && strlen(tmp)) {
            rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "ver", tmp);
            if (rc < 0) {
                 printf("Error at xmlTextWriterWriteAttribute\n");
                 return;
            }
        }
        if (handler && tmp != NULL) xmlFree(tmp);

        tmp = ConvertInput(entry->release, handler);
        if (tmp && strlen(tmp)) {
            rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "rel", tmp);
            if (rc < 0) {
                 printf("Error at xmlTextWriterWriteAttribute\n");
                 return;
            }
        }
        if (handler && tmp != NULL) xmlFree(tmp);

    close:

        if (pcotype == REQUIRES) {
            /* Add pre attribute */
            if (entry->pre) {
                rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "pre", "1");
            } else {
                ; //rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "pre", "0");
            }
            if (rc < 0) {
                 printf("Error at xmlTextWriterWriteAttribute\n");
                 return;
            }
        }

        /* Close entry element */
        rc = xmlTextWriterEndElement(writer);
        if (rc < 0) {
            printf("Error at xmlTextWriterEndElement\n");
            return;
        }
    }

    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        printf("Error at xmlTextWriterEndElement\n");
        return;
    }
}



/**
 * dump_base_items:
 * @writer:
 * @package:
 *
 * Converts @pkg_dict into xml string
 *
 * Returns the converted XML string, or NULL in case of error.
 */
void
dump_base_items(xmlTextWriterPtr writer, Package *package, xmlCharEncodingHandlerPtr handler)
{
    int rc;
    char n[] = "";
    xmlChar *tmp  = NULL;
    gchar   *tmp2 = NULL;

#ifdef DEBUG
    printf("CALLED dump_base_items\n");
#endif

    rc = xmlTextWriterStartDocument(writer, NULL, MY_ENCODING, NULL);
    if (rc < 0) {
        printf ("Error at xmlTextWriterStartDocument\n");
        return;
    }

    /***********************************
    Element: package
    ************************************/
    rc = xmlTextWriterStartElement(writer, BAD_CAST "package");
    if (rc < 0) {
        printf("Error at xmlTextWriterStartElement\n");
        return;
    }

    /* Add an attribute with name type to package */
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "type", BAD_CAST "rpm");
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }

    /***********************************
    Element: name
    ************************************/
    tmp = ConvertInput(package->name, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteElement(writer, BAD_CAST "name", tmp);
    if (rc < 0) {
        printf("Error at xmlTextWriterWriteElement\n");
        return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /***********************************
    Element: arch
    ************************************/
    tmp = ConvertInput(package->arch, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteElement(writer, BAD_CAST "arch", tmp);
    if (rc < 0) {
        printf("Error at xmlTextWriterWriteElement\n");
        return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /***********************************
    Element: version
    ************************************/
    rc = xmlTextWriterStartElement(writer, BAD_CAST "version");
    if (rc < 0) {
        printf("Error at xmlTextWriterStartElement\n");
        return;
    }

    /* Write version attribute epoch */
    tmp = ConvertInput(package->epoch, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "epoch", tmp);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /* Write version attribute ver */
    tmp = ConvertInput(package->version, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "ver", tmp);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /* Write version attribute rel */
    tmp = ConvertInput(package->release, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "rel", tmp);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /* Close version element */
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        printf("Error at xmlTextWriterEndElement\n");
        return;
    }

    /***********************************
    Element: checksum
    ************************************/
    rc = xmlTextWriterStartElement(writer, BAD_CAST "checksum");
    if (rc < 0) {
        printf("Error at xmlTextWriterStartElement\n");
        return;
    }

    /* Write checksum attribute pkgid */
    tmp = ConvertInput("YES", handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "pkgid", tmp);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /* Write checksum attribute checksum_type */
    tmp = ConvertInput(package->checksum_type, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "type", tmp);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /* Write element string */
    tmp = ConvertInput(package->pkgId, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteString(writer, BAD_CAST tmp);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteString\n");
         return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /* Close checksum element */
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        printf("Error at xmlTextWriterEndElement\n");
        return;
    }

    /***********************************
    Element: summary
    ************************************/
    tmp = ConvertInput(package->summary, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteElement(writer, BAD_CAST "summary", tmp);
    if (rc < 0) {
        printf("Error at xmlTextWriterWriteElement\n");
        return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /***********************************
    Element: description
    ************************************/
    tmp = ConvertInput(package->description, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteElement(writer, BAD_CAST "description", tmp);
    if (rc < 0) {
        printf("Error at xmlTextWriterWriteElement\n");
        return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /***********************************
    Element: packager
    ************************************/
    tmp = ConvertInput(package->rpm_packager, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteElement(writer, BAD_CAST "packager", tmp);
    if (rc < 0) {
        printf("Error at xmlTextWriterWriteElement\n");
        return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /***********************************
    Element: url
    ************************************/
    tmp = ConvertInput(package->url, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteElement(writer, BAD_CAST "url", tmp);
    if (rc < 0) {
        printf("Error at xmlTextWriterWriteElement\n");
        return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /***********************************
    Element: time
    ************************************/
    rc = xmlTextWriterStartElement(writer, BAD_CAST "time");
    if (rc < 0) {
        printf("Error at xmlTextWriterStartElement\n");
        return;
    }

    /* Write time attribute file */
    tmp2 = g_strdup_printf("%d", package->time_file);
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "file", tmp2);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (tmp2 != NULL) g_free(tmp2);

    /* Write time attribute build */
    tmp2 = g_strdup_printf("%d", package->time_build);
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "build", tmp2);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (tmp2 != NULL) g_free(tmp2);

    /* Close time element */
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        printf("Error at xmlTextWriterEndElement\n");
        return;
    }

    /***********************************
    Element: size
    ************************************/
    rc = xmlTextWriterStartElement(writer, BAD_CAST "size");
    if (rc < 0) {
        printf("Error at xmlTextWriterStartElement\n");
        return;
    }

    /* Write size attribute package */
    tmp2 = g_strdup_printf("%d", package->size_package);
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "package", tmp2);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (tmp2 != NULL) g_free(tmp2);

    /* Write size attribute installed */
    tmp2 = g_strdup_printf("%d", package->size_installed);
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "installed", tmp2);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (tmp2 != NULL) g_free(tmp2);

    /* Write size attribute archive */
    tmp2 = g_strdup_printf("%d", package->size_archive);
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "archive", tmp2);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (tmp2 != NULL) g_free(tmp2);

    /* Close size element */
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        printf("Error at xmlTextWriterEndElement\n");
        return;
    }

    /***********************************
    Element: location
    ************************************/
    rc = xmlTextWriterStartElement(writer, BAD_CAST "location");
    if (rc < 0) {
        printf("Error at xmlTextWriterStartElement\n");
        return;
    }

    /* Write location attribute href */
    tmp = ConvertInput(package->location_href, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "href", tmp);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /* Write location attribute base */
    tmp = ConvertInput(package->location_base, handler);
    if (!tmp) tmp = n;
    if (package->location_base && strlen(package->location_base)) {
        rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "base", tmp);
        if (rc < 0) {
             printf("Error at xmlTextWriterWriteAttribute\n");
             return;
        }
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /* Close location element */
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        printf("Error at xmlTextWriterEndElement\n");
        return;
    }

    /***********************************
    Element: format
    ************************************/
    rc = xmlTextWriterStartElement(writer, BAD_CAST "format");
    if (rc < 0) {
        printf("Error at xmlTextWriterStartElement\n");
        return;
    }

    /***********************************
    Element: license
    ************************************/
    tmp = ConvertInput(package->rpm_license, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteElement(writer, BAD_CAST "rpm:license", tmp);
    if (rc < 0) {
        printf("Error at xmlTextWriterWriteElement\n");
        return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /***********************************
    Element: vendor
    ************************************/
    tmp = ConvertInput(package->rpm_vendor, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteElement(writer, BAD_CAST "rpm:vendor", tmp);
    if (rc < 0) {
        printf("Error at xmlTextWriterWriteElement\n");
        return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /***********************************
    Element: group
    ************************************/
    tmp = ConvertInput(package->rpm_group, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteElement(writer, BAD_CAST "rpm:group", tmp);
    if (rc < 0) {
        printf("Error at xmlTextWriterWriteElement\n");
        return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /***********************************
    Element: buildhost
    ************************************/
    tmp = ConvertInput(package->rpm_buildhost, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteElement(writer, BAD_CAST "rpm:buildhost", tmp);
    if (rc < 0) {
        printf("Error at xmlTextWriterWriteElement\n");
        return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /***********************************
    Element: sourcerpm
    ************************************/
    tmp = ConvertInput(package->rpm_sourcerpm, handler);
    if (!tmp) tmp = n;
    rc = xmlTextWriterWriteElement(writer, BAD_CAST "rpm:sourcerpm", tmp);
    if (rc < 0) {
        printf("Error at xmlTextWriterWriteElement\n");
        return;
    }
    if (handler && tmp != NULL && tmp != n) xmlFree(tmp);

    /***********************************
    Element: header-range
    ************************************/
    rc = xmlTextWriterStartElement(writer, BAD_CAST "rpm:header-range");
    if (rc < 0) {
        printf("Error at xmlTextWriterWriteElement\n");
        return;
    }

    /* Write header-range attribute hdrstart */
    tmp2 = g_strdup_printf("%d", package->rpm_header_start);
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "start", tmp2);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (tmp2 != NULL) g_free(tmp2);

    /* Write header-range attribute hdrend */
    tmp2 = g_strdup_printf("%d", package->rpm_header_end);
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "end", tmp2);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (tmp2 != NULL) g_free(tmp2);

    /* Close header-range element */
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        printf("Error at xmlTextWriterEndElement\n");
        return;
    }

    /* Files dump */
    dump_pco(writer,   package, PROVIDES, handler);
    dump_pco(writer,   package, REQUIRES, handler);
    dump_pco(writer,   package, CONFLICTS, handler);
    dump_pco(writer,   package, OBSOLETES, handler);
    dump_files(writer, package, 1, handler);


    /* Close format element */
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        printf("Error at xmlTextWriterEndElement\n");
        return;
    }

    /* Close package element */
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        printf("Error at xmlTextWriterEndElement\n");
        return;
    }

    /* Close document (and every still opened tags) */
    rc = xmlTextWriterEndDocument(writer);
    if (rc < 0) {
        printf("Error at xmlTextWriterEndDocument\n");
        return;
    }
}


char *
xml_dump_primary(Package *package, const char *encoding)
{

    /*
    * Global variable initialization
    */

    // Encoging handler for selected encoding
    xmlCharEncodingHandlerPtr handler = NULL;
    if (encoding) {
        handler = xmlFindCharEncodingHandler(encoding);
        if (!handler) {
            printf("ConvertInput: no encoding handler found for 'utf-8'\n");
            return NULL;
        }
    }

    /*
     * XML Gen
     */

    xmlBufferPtr buf = xmlBufferCreate();
    if (buf == NULL) {
        printf("Error creating the xml buffer\n");
        return NULL;
    }

    xmlTextWriterPtr writer = xmlNewTextWriterMemory(buf, 0);
    if (writer == NULL) {
        printf("Error creating the xml writer\n");
        return NULL;
    }

#ifdef DEBUG
    printf("Xml buffer and writer created\n");
#endif

    dump_base_items(writer, package, handler);

    xmlFreeTextWriter(writer);

    // Get rid off <?xml ...?> header
    char *pkg_str = strstr((const char*) buf->content, "<package");
    if (!pkg_str) {
        pkg_str = (char*) buf->content;
    }

    char *result = malloc(sizeof(char) * strlen(pkg_str) + 1);
    strcpy(result, pkg_str);

    xmlBufferFree(buf);
    return result;
}

