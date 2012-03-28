#ifndef __C_CREATEREPOLIB_LOAD_METADATA_H__
#define __C_CREATEREPOLIB_LOAD_METADATA_H__

#include <glib.h>

typedef enum {
    HT_KEY_DEFAULT,
    HT_KEY_HASH = HT_KEY_DEFAULT,
    HT_KEY_NAME,
    HT_KEY_FILENAME
} HashTableKey;


GHashTable *new_metadata_hashtable();
void destroy_metadata_hashtable(GHashTable *hashtable);
int locate_and_load_xml_metadata(GHashTable *hashtable, const char *repopath, HashTableKey key);

#endif /* __C_CREATEREPOLIB_LOAD_METADATA_H__ */
