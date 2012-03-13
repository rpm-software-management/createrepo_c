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


void dump_files(xmlTextWriterPtr, Package *, int);

char *xml_dump_primary(Package *);
char *xml_dump_filelists(Package *);
char *xml_dump_other(Package *);
struct XmlStruct xml_dump(Package *);

#endif /* __C_CREATEREPOLIB_XML_DUMP_H__ */
