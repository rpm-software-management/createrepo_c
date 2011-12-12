%module xml_dump

%{
#include "xml_dump.h"
%}

//%include "xml_dump.h"

char *xml_dump_primary(Package *, const char *);
char *xml_dump_filelists(Package *, const char *);
char *xml_dump_other(Package *, const char *);