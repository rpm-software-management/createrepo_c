%module parsehdr

%include <typemaps.i>

%{
#include "package.h"
#include "parsehdr.h"
%}

%include "xml_dump.h"
%include "parsehdr.h"

/*
struct XmlStruct {
    const char *primary;
    const char *filelists;
    const char *other;
};

XmlStruct *xml_from_header(Header, gint64, const char*, const char*, const char*, const char*, gint64, gint64);
*/

/*
#ifdef SWIGPYTHON
%typemap(out) struct XmlStruct {
    $result = "asdf";
}
#endif
*/