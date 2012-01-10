#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <string.h>
#include "misc.h"
#include "xml_dump.h"

//#define DEBUG
#undef DEBUG

//xmlCharEncodingHandlerPtr handler = NULL;

xmlChar *
ConvertInput(const char *in, xmlCharEncodingHandlerPtr handler)
{
    xmlChar *out;
    int ret;
    int size;
    int out_size;
    int temp;

    if (in == 0) {
        return NULL;
    }

    if (!handler) {
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
            out[out_size] = 0;  /*null terminating out */
        }
    } else {
        printf("ConvertInput: no mem\n");
    }

    return out;
}


void
dump_files(xmlTextWriterPtr writer, Package *package, int primary,
           xmlCharEncodingHandlerPtr handler)
{
#ifdef DEBUG
    printf("CALLED dump_files\n");
#endif

    xmlChar *tmp = NULL;

    if (!package->files) {
        return;
    }

    GSList *element = NULL;
    for(element = package->files; element; element=element->next) {
        PackageFile *entry = (PackageFile*) element->data;

        if (primary && !is_primary(entry->name)) {
            continue;
        }


        // ***********************************
        // Element: file
        // ************************************

        int rc;
        rc = xmlTextWriterStartElement(writer, BAD_CAST "file");
        if (rc < 0) {
            printf("Error at xmlTextWriterWriteElement\n");
            return;
        }

        // Write type
        if (entry->type && strlen(entry->type) && strcmp(entry->type, "file")) {
            tmp = ConvertInput(entry->type, handler);
            rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "type", tmp);
            if (rc < 0) {
                 printf("Error at xmlTextWriterWriteAttribute\n");
                 return;
            }
            if (handler && tmp != NULL) xmlFree(tmp);
        }

        // Write text (file path)
        tmp = ConvertInput(entry->name, handler);
        if (tmp) {
            xmlTextWriterWriteString(writer, BAD_CAST tmp);
            if (handler && tmp != NULL) xmlFree(tmp);
        }

        // Close file element
        rc = xmlTextWriterEndElement(writer);
        if (rc < 0) {
            printf("Error at xmlTextWriterEndElement\n");
            return;
        }
    }
}

