#ifndef __LOAD_METADATA__
#define __LOAD_METADATA__

#include <glib.h>
#include "constants.h"

struct package_metadata {
    gint64 time_file;
    gint64 size_package;
    char *location_href; // location href (relative path in repo + filename)
    char *location_base; // location base
    char *checksum_type;
    char *primary_xml;
    char *filelists_xml;
    char *other_xml;
//    GStringChunk *chunk;
};

// Key in GHashTable is filename (just filename without path)

GHashTable *load_gz_compressed_xml_metadata(const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path);
GHashTable *load_xml_metadata(const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path);
GHashTable *locate_and_load_xml_metadata(const char *repopath);

GHashTable *load_gz_compressed_xml_metadata_2(const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path);
GHashTable *load_xml_metadata_2(const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path);
GHashTable *locate_and_load_xml_metadata_2(const char *repopath);

#endif /* __LOAD_METADATA__ */
