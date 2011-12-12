#ifndef __XML_DUMP__
#define __XML_DUMP__

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "package.h"

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

#endif /* __XML_DUMP__ */
