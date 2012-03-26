#ifndef __C_CREATEREPOLIB_LOAD_METADATA_2_H__
#define __C_CREATEREPOLIB_LOAD_METADATA_2_H__

#include <glib.h>
#include "constants.h"

GHashTable *new_metadata_hashtable();
void destroy_metadata_hashtable(GHashTable *hashtable);

int locate_and_load_xml_metadata_2(GHashTable *hashtable, const char *repopath);

#endif /* __C_CREATEREPOLIB_LOAD_METADATA_2_H__ */
