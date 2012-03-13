#ifndef __C_CREATEREPOLIB_LOAD_METADATA_H__
#define __C_CREATEREPOLIB_LOAD_METADATA_H__

#include <glib.h>
#include "constants.h"

struct PackageMetadata {
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

struct MetadataLocation {
    char *pri_xml_href;
    char *fil_xml_href;
    char *oth_xml_href;
    char *pri_sqlite_href;
    char *fil_sqlite_href;
    char *oth_sqlite_href;
    char *groupfile_href;
    char *cgroupfile_href; // compressed groupfile location
    char *repomd;
};

// Key in GHashTable is filename (just filename without path)

GHashTable *new_old_metadata_hashtable();
void destroy_old_metadata_hashtable(GHashTable *hashtable);
void free_metadata_location(struct MetadataLocation *);
int load_gz_compressed_xml_metadata(GHashTable *hashtable, const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path);
int load_xml_metadata(GHashTable *hashtable, const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path);
struct MetadataLocation *locate_metadata_via_repomd(const char *);
int locate_and_load_xml_metadata(GHashTable *hashtable, const char *repopath);
int remove_old_metadata(const char *repopath);

#endif /* __C_CREATEREPOLIB_LOAD_METADATA_H__ */
