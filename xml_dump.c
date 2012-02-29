#include <glib.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <string.h>
#include "logging.h"
#include "misc.h"
#include "xml_dump.h"

#undef MODULE
#define MODULE "xml_dump: "


//xmlCharEncodingHandlerPtr handler = NULL;

xmlChar *ConvertInput(const char *in, xmlCharEncodingHandlerPtr handler)
{
    return (xmlChar*) in;
/*
    xmlChar *out;
    int ret;
    int size;
    int out_size;
    int temp;

    if (!in || !handler) {
        return (xmlChar*) in;
    }

    size = (int) strlen(in) + 1;
    out_size = size * 2 - 1;
    out = (unsigned char *) xmlMalloc((size_t) out_size);
    printf("%s | size: %d, out_size: %d\n", in, size, out_size);


    if (out != NULL) {

        temp = size - 1;
        ret = handler->input(out, &out_size, (const xmlChar *) in, &temp);
        if ((ret < 0) || (temp - size + 1)) {
            if (ret < 0) {
                printf("ConvertInput: conversion wasn't successful.\n");
            } else {
                printf
                    ("ConvertInput: conversion wasn't successful. converted: %i octets.\n",
                     temp);
            }

            xmlFree(out);
            out = 0;
        } else {
            out = (unsigned char *) xmlRealloc(out, out_size + 1);
            out[out_size] = 0;  // null terminating out
        }
    } else {
        printf("ConvertInput: no mem\n");
    }

    return out;
*/
}


void
dump_files(xmlTextWriterPtr writer, Package *package, int primary,
           xmlCharEncodingHandlerPtr handler)
{
    xmlChar *tmp = NULL;

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


        // Skip a file if we want primary files and the file is not one

        if (primary && !is_primary(fullname)) {
            continue;
        }


        // ***********************************
        // Element: file
        // ************************************

        int rc;
        rc = xmlTextWriterStartElement(writer, BAD_CAST "file");
        if (rc < 0) {
            g_critical(MODULE"dump_files: Error at xmlTextWriterWriteElement");
            return;
        }

        // Write type
        if (entry->type && strlen(entry->type) && strcmp(entry->type, "file")) {
            tmp = ConvertInput(entry->type, handler);
            rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "type", tmp);
            if (handler && tmp != NULL) xmlFree(tmp);
            if (rc < 0) {
                 g_critical(MODULE"dump_files: Error at xmlTextWriterWriteAttribute");
                 return;
            }

        }

        // Write text (file path)
        tmp = ConvertInput(fullname, handler);
        if (tmp) {
            rc = xmlTextWriterWriteString(writer, BAD_CAST tmp);
            if (handler && tmp != NULL) xmlFree(tmp);
            if (rc < 0) {
                g_critical(MODULE"dump_files: Error at xmlTextWriterWriteString\n");
                return;
            }
        }

        // Close file element
        rc = xmlTextWriterEndElement(writer);
        if (rc < 0) {
            g_critical(MODULE"dump_files: Error at xmlTextWriterEndElement\n");
            return;
        }
    }
}



struct XmlStruct xml_dump(Package *pkg, const char *encoding)
{
    struct XmlStruct result;
    result.primary = xml_dump_primary(pkg, encoding);
    result.filelists = xml_dump_filelists(pkg, encoding);
    result.other = xml_dump_other(pkg, encoding);

    return result;
}

