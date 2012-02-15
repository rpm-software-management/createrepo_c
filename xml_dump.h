#ifndef __C_CREATEREPOLIB_XML_DUMP_H__
#define __C_CREATEREPOLIB_XML_DUMP_H__

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "package.h"

struct XmlStruct{
    char *primary;
    char *filelists;
    char *other;
};

/**
 * ConvertInput:
 * @in: string in a given encoding
 * @encoding: the encoding used
 *
 * Converts @in into UTF-8 for processing with libxml2 APIs
 *
 * Returns the converted UTF-8 string, or NULL in case of error.
 */
xmlChar *ConvertInput(const char *, xmlCharEncodingHandlerPtr);

void dump_files(xmlTextWriterPtr, Package *, int, xmlCharEncodingHandlerPtr);

char *xml_dump_primary(Package *, const char *);
char *xml_dump_filelists(Package *, const char *);
char *xml_dump_other(Package *, const char *);
struct XmlStruct xml_dump(Package *, const char *);

#endif /* __C_CREATEREPOLIB_XML_DUMP_H__ */
