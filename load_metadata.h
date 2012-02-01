#ifndef __LOAD_METADATA__
#define __LOAD_METADATA__

#include <glib.h>
#include "constants.h"

struct package_metadata {
    gint64 time_file;
    gint64 size_package;
    char *checksum_type;
    char *primary_xml;
    char *filelists_xml;
    char *other_xml;
//    GStringChunk *chunk;
};


GHashTable *load_gz_compressed_xml_metadata(const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path);
GHashTable *load_xml_metadata(const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path);
GHashTable *locate_and_load_xml_metadata(const char *repopath);

#endif /* __LOAD_METADATA__ */
