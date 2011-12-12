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
dump_changelog(xmlTextWriterPtr writer, Package *package, xmlCharEncodingHandlerPtr handler)
{
#ifdef DEBUG
    printf("CALLED dump_changelog\n");
#endif

    xmlChar *tmp = NULL;
    gchar   *tmp2 = NULL;

    if (!package->changelogs) {
        return;
    }

    GSList *element = NULL;
    for(element = package->changelogs; element; element=element->next) {

        ChangelogEntry *entry = (ChangelogEntry*) element->data;
        // ***********************************
        // Element: file
        // ************************************

        int rc;
        rc = xmlTextWriterStartElement(writer, BAD_CAST "changelog");
        if (rc < 0) {
            printf("Error at xmlTextWriterWriteElement\n");
            return;
        }

        // Write param author
        tmp = ConvertInput(entry->author, handler);
        rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "author", tmp);
        if (rc < 0) {
             printf("Error at xmlTextWriterWriteAttribute\n");
             return;
        }
        if (handler && tmp != NULL) xmlFree(tmp);

        // Write param date
        tmp2 = g_strdup_printf("%d", entry->date);
        rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "date", tmp2);
        if (rc < 0) {
             printf("Error at xmlTextWriterWriteAttribute\n");
             return;
        }
        if (tmp2 != NULL) g_free(tmp2);

        // Write text (file path)
        tmp = ConvertInput(entry->changelog, handler);
        xmlTextWriterWriteString(writer, BAD_CAST tmp);
        if (handler && tmp != NULL) xmlFree(tmp);

        // Close file element
        rc = xmlTextWriterEndElement(writer);
        if (rc < 0) {
            printf("Error at xmlTextWriterEndElement\n");
            return;
        }
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
dump_other_items(xmlTextWriterPtr writer, Package *package, xmlCharEncodingHandlerPtr handler)
{
    int rc;
    xmlChar *tmp  = NULL;
    gchar   *tmp2 = NULL;

#ifdef DEBUG
    printf("CALLED dump_other_items\n");
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

    /* Add an attribute to package */
    tmp = ConvertInput(package->pkgId, handler);
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "pkgid", tmp);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (handler && tmp != NULL) xmlFree(tmp);

    tmp = ConvertInput(package->name, handler);
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "pkgid", tmp);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (handler && tmp != NULL) xmlFree(tmp);

    tmp = ConvertInput(package->arch, handler);
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "pkgid", tmp);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (handler && tmp != NULL) xmlFree(tmp);

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
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "epoch", tmp);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (handler && tmp != NULL) xmlFree(tmp);

    /* Write version attribute ver */
    tmp = ConvertInput(package->version, handler);
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "ver", tmp);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (handler && tmp != NULL) xmlFree(tmp);

    /* Write version attribute rel */
    tmp = ConvertInput(package->release, handler);
    rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "rel", tmp);
    if (rc < 0) {
         printf("Error at xmlTextWriterWriteAttribute\n");
         return;
    }
    if (handler && tmp != NULL) xmlFree(tmp);

    /* Close version element */
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        printf("Error at xmlTextWriterEndElement\n");
        return;
    }

    /* Files dump */
    dump_changelog(writer, package, handler);

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
xml_dump_other(Package *package, const char *encoding)
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

    dump_other_items(writer, package, handler);

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

