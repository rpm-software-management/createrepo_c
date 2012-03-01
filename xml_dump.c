#include <glib.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <string.h>
#include "logging.h"
#include "misc.h"
#include "xml_dump.h"

#undef MODULE
#define MODULE "xml_dump: "


void dump_files(xmlTextWriterPtr writer, Package *package, int primary)
{
    if (!package->files) {
        return;
    }


    GSList *element = NULL;
    for(element = package->files; element; element=element->next) {
        PackageFile *entry = (PackageFile*) element->data;


        // File without name or path is suspicious => Skip it

        if (!(entry->path)) {
            continue;
        }

        if (!(entry->name)) {
            continue;
        }


        // String concatenation (path + basename)

        gchar *fullname;
        fullname = g_strconcat(entry->path, entry->name, NULL);

        if (!fullname) {
            continue;
        }


        // Skip a file if we want primary files and the file is not one

        if (primary && !is_primary(fullname)) {
            g_free(fullname);
            continue;
        }


        // ***********************************
        // Element: file
        // ************************************

        int rc;
        rc = xmlTextWriterStartElement(writer, BAD_CAST "file");
        if (rc < 0) {
            g_critical(MODULE"dump_files: Error at xmlTextWriterWriteElement");
            g_free(fullname);
            return;
        }

        // Write type
        if (entry->type && strcmp(entry->type, "file")) {
            rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "type", "%s", entry->type);
            if (rc < 0) {
                 g_critical(MODULE"dump_files: Error at xmlTextWriterWriteFormatAttribute");
                 g_free(fullname);
                 return;
            }

        }

        // Write text (file path)
        rc = xmlTextWriterWriteFormatString(writer, "%s", fullname);
        g_free(fullname);
        if (rc < 0) {
            g_critical(MODULE"dump_files: Error at xmlTextWriterWriteFormatString\n");
            return;
        }

        // Close file element
        rc = xmlTextWriterEndElement(writer);
        if (rc < 0) {
            g_critical(MODULE"dump_files: Error at xmlTextWriterEndElement\n");
            return;
        }
    }
}



struct XmlStruct xml_dump(Package *pkg)
{
    struct XmlStruct result;
    result.primary = xml_dump_primary(pkg);
    result.filelists = xml_dump_filelists(pkg);
    result.other = xml_dump_other(pkg);

    return result;
}
