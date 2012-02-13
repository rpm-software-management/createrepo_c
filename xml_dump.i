%module xml_dump

%include <typemaps.i>

%{
#include "package.h"
#include "xml_dump.h"
%}

%newobject xml_dump_primary;
%newobject xml_dump_filelists;
%newobject xml_dump_other;

%include "xml_dump.h"

/*
char *xml_dump_primary(Package *, const char *);
char *xml_dump_filelists(Package *, const char *);
char *xml_dump_other(Package *, const char *);
struct XmlStruct xml_dump(Package *, const char *);
*/
