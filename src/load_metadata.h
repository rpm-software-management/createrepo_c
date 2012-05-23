/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __C_CREATEREPOLIB_LOAD_METADATA_H__
#define __C_CREATEREPOLIB_LOAD_METADATA_H__

#include <glib.h>
#include "locate_metadata.h"

typedef enum {
    HT_KEY_DEFAULT,
    HT_KEY_HASH = HT_KEY_DEFAULT,
    HT_KEY_NAME,
    HT_KEY_FILENAME
} HashTableKey;


#define LOAD_METADATA_OK        0
#define LOAD_METADATA_ERR       1

GHashTable *new_metadata_hashtable();
void destroy_metadata_hashtable(GHashTable *hashtable);
int load_xml_metadata(GHashTable *hashtable, struct MetadataLocation *ml, HashTableKey key);
int locate_and_load_xml_metadata(GHashTable *hashtable, const char *repopath, HashTableKey key);

#endif /* __C_CREATEREPOLIB_LOAD_METADATA_H__ */
