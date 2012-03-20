#ifndef __C_CREATEREPOLIB_XML_DUMP_H__
#define __C_CREATEREPOLIB_XML_DUMP_H__

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "package.h"

// XML namespaces

#define XML_COMMON_NS           "http://linux.duke.edu/metadata/common"
#define XML_FILELISTS_NS        "http://linux.duke.edu/metadata/filelists"
#define XML_OTHER_NS            "http://linux.duke.edu/metadata/other"
#define XML_RPM_NS              "http://linux.duke.edu/metadata/rpm"


struct XmlStruct{
    char *primary;
    char *filelists;
    char *other;
};


void dump_files(xmlNodePtr, Package *, int);

char *xml_dump_primary(Package *);
char *xml_dump_filelists(Package *);
char *xml_dump_other(Package *);
struct XmlStruct xml_dump(Package *);

#endif /* __C_CREATEREPOLIB_XML_DUMP_H__ */
