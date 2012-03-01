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


#define PROVIDES    0
#define CONFLICTS   1
#define OBSOLETES   2
#define REQUIRES    3


void dump_changelog(xmlTextWriterPtr writer, Package *package)
{
    int rc;

    if (!package->changelogs) {
        return;
    }

    GSList *element = NULL;
    for(element = package->changelogs; element; element=element->next) {

        ChangelogEntry *entry = (ChangelogEntry*) element->data;

        assert(entry);


        // ***********************************
        // Element: Changelog
        // ************************************

        // Open changelog element
        rc = xmlTextWriterStartElement(writer, BAD_CAST "changelog");
        if (rc < 0) {
            g_critical(MODULE"dump_changelog: Error at xmlTextWriterWriteElement");
            return;
        }

        // Write param author
        rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "author", "%s", entry->author);
        if (rc < 0) {
             g_critical(MODULE"dump_changelog: Error at xmlTextWriterWriteAttribute");
             return;
        }

        // Write param date
        rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "date", "%lld", (long long int) entry->date);
        if (rc < 0) {
             g_critical(MODULE"dump_changelog: Error at xmlTextWriterWriteFormatAttribute");
             return;
        }

        // Write changelog text
        rc = xmlTextWriterWriteFormatString(writer, "%s", entry->changelog);
        if (rc < 0) {
             g_critical(MODULE"dump_changelog: Error at xmlTextWriterWriteFormatString");
             return;
        }

        // Close changelog element
        rc = xmlTextWriterEndElement(writer);
        if (rc < 0) {
            g_critical(MODULE"dump_changelog: Error at xmlTextWriterEndElement");
            return;
        }
    }
}


void dump_other_items(xmlTextWriterPtr writer, Package *package)
{
    int rc;

    rc = xmlTextWriterStartDocument(writer, NULL, NULL, NULL);
    if (rc < 0) {
        g_critical(MODULE"dump_other_items: Error at xmlTextWriterStartDocument");
        return;
    }


    /***********************************
    Element: package
    ************************************/

    // Open package element
    rc = xmlTextWriterStartElement(writer, BAD_CAST "package");
    if (rc < 0) {
        g_critical(MODULE"dump_other_items: Error at xmlTextWriterStartElement");
        return;
    }

    // Add pkgid attribute
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "pkgid", "%s", package->pkgId);
    if (rc < 0) {
         g_critical(MODULE"dump_other_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Add name attribute
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "name", "%s", package->name);
    if (rc < 0) {
         g_critical(MODULE"dump_other_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }

    // Add arch attribute
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "arch", "%s", package->arch);
    if (rc < 0) {
         g_critical(MODULE"dump_other_items: Error at xmlTextWriterWriteFormatAttribute");
         return;
    }


    /***********************************
    Element: version
    ************************************/

    // Open version element
    rc = xmlTextWriterStartElement(writer, BAD_CAST "version");
    if (rc < 0) {
        g_critical(MODULE"dump_other_items: Error at xmlTextWriterStartElement");
        return;
    }

    // Write version attribute epoch
    if (!package->epoch) {
        rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "arch", BAD_CAST "0");
    } else {
        rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "arch", "%s", package->epoch);
    }
    if (rc < 0) {
        g_critical(MODULE"dump_other_items: Error at xmlTextWriterWrite(Format)Attribute");
        return;
    }

    // Write version attribute ver
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "ver", "%s", package->version);
    if (rc < 0) {
        g_critical(MODULE"dump_other_items: Error at xmlTextWriterWriteFormatAttribute");
        return;
    }

    // Write version attribute rel
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "rel", "%s", package->release);
    if (rc < 0) {
        g_critical(MODULE"dump_other_items: Error at xmlTextWriterWriteFormatAttribute");
        return;
    }

    // Close version element
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        g_critical(MODULE"dump_other_items: Error at xmlTextWriterEndElement");
        return;
    }


    // Files dump

    dump_changelog(writer, package);


    // Close package element
    rc = xmlTextWriterEndElement(writer);
    if (rc < 0) {
        g_critical(MODULE"dump_other_items: Error at xmlTextWriterEndElement");
        return;
    }

    // Close document (and every still opened tags)
    rc = xmlTextWriterEndDocument(writer);
    if (rc < 0) {
        g_critical(MODULE"dump_other_items: Error at xmlTextWriterEndDocument");
        return;
    }
}


char *xml_dump_other(Package *package)
{
    xmlBufferPtr buf = xmlBufferCreate();
    if (buf == NULL) {
        g_critical(MODULE"xml_dump_other: Error creating the xml buffer");
        return NULL;
    }

    xmlTextWriterPtr writer = xmlNewTextWriterMemory(buf, 0);
    if (writer == NULL) {
        g_critical(MODULE"xml_dump_other: Error creating the xml writer");
        return NULL;
    }


    // Dump IT!

    dump_other_items(writer, package);


    xmlFreeTextWriter(writer);


    // Get XML from xmlBuffer without <?xml ...?> header

    assert(buf->content);

    char *pkg_str = strstr((const char*) buf->content, "<package");
    if (!pkg_str) {
        pkg_str = (char*) buf->content;
    }

    char *result = g_strdup(pkg_str);

    xmlBufferFree(buf);

    return result;
}
