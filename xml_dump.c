#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <string.h>
#include "xml_dump.h"

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
//        printf("Nic neprevadim\n");
        return (xmlChar*) in;
    }


    size = (int) strlen(in) + 1;
    out_size = size * 2 - 1;
    out = (unsigned char *) xmlMalloc((size_t) out_size);
    printf("%s | size: %d, out_size: %d\n", in, size, out_size);


    if (out != NULL) {

        temp = size - 1;
//        printf("JDU PREVADET\n");
        ret = handler->input(out, &out_size, (const xmlChar *) in, &temp);
//        printf("JDU PREVADET\n");
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


GRegex *pri_re_1 = NULL;
GRegex *pri_re_2 = NULL;
GRegex *pri_re_3 = NULL;

void
dump_files(xmlTextWriterPtr writer, Package *package, int primary, 
           xmlCharEncodingHandlerPtr handler)
{
#ifdef DEBUG
    printf("CALLED dump_files\n");
#endif

    // Regex compilation
    if (!pri_re_1) {
        GRegexMatchFlags compile_flags = G_REGEX_OPTIMIZE|G_REGEX_MATCH_ANCHORED;
        pri_re_1 = g_regex_new(".*bin/.*", compile_flags, 0, NULL);
        pri_re_2 = g_regex_new("/etc/.*", compile_flags, 0, NULL);
        pri_re_3 = g_regex_new("/usr/lib/sendmail$", compile_flags, 0, NULL);
    }

    xmlChar *tmp = NULL;

    if (!package->files) {
        return;
    }

    GSList *element = NULL;
    for(element = package->files; element; element=element->next) {

        PackageFile *entry = (PackageFile*) element->data;

        if (primary) {
            // Only files or dirs could be primary - sorry ghost files :(
            if (entry->type[0] != '\0' && strcmp(entry->type, "file") && strcmp(entry->type, "dir")) {
                continue;
            }

            // Check if file name match pattern for primary files
            if (!g_regex_match(pri_re_1, entry->name, 0, NULL)
               && !g_regex_match(pri_re_2, entry->name, 0, NULL)
               && !g_regex_match(pri_re_3, entry->name, 0, NULL)) {
                continue;
            }
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
        if (entry->type && strlen(entry->type)) {
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

