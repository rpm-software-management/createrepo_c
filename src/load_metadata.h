#ifndef __C_CREATEREPOLIB_LOAD_METADATA_H__
#define __C_CREATEREPOLIB_LOAD_METADATA_H__

#include <glib.h>
#include "constants.h"

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


GHashTable *new_metadata_hashtable();
void destroy_metadata_hashtable(GHashTable *hashtable);
int locate_and_load_xml_metadata(GHashTable *hashtable, const char *repopath);

struct MetadataLocation *locate_metadata_via_repomd(const char *);
void free_metadata_location(struct MetadataLocation *);

int remove_old_metadata(const char *repopath);

#endif /* __C_CREATEREPOLIB_LOAD_METADATA_H__ */
