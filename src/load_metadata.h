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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef __C_CREATEREPOLIB_LOAD_METADATA_H__
#define __C_CREATEREPOLIB_LOAD_METADATA_H__

#include <glib.h>
#include "locate_metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup   load_metadata   Load metadata API.
 */

/** \ingroup load_metadata
 * Package attribute used as key in the hashtable.
 */
typedef enum {
    HT_KEY_DEFAULT,               /*!< default = pkg hash */
    HT_KEY_HASH = HT_KEY_DEFAULT, /*!< pkg hash (Package ->pkgId) */
    HT_KEY_NAME,                  /*!< pkg name (Package ->name) */
    HT_KEY_FILENAME               /*!< pkg filename (Package ->location_href) */
} HashTableKey;

/**@{*/
#define LOAD_METADATA_OK        0  /*!< Metadata loaded successfully */
#define LOAD_METADATA_ERR       1  /*!< Error while loading metadata */
/**@}*/

/** \ingroup load_metadata
 * Create new (empty) metadata hashtable.
 * @return              empty metadata hashtable
 */
GHashTable *new_metadata_hashtable();

/** \ingroup load_metadata
 * Destroys all keys and values in the metadata hash table and decrements
 * its reference count by 1.
 * @param hashtable     metadata hashtable
 */
void destroy_metadata_hashtable(GHashTable *hashtable);

/** \ingroup load_metadata
 * Load metadata from the specified location.
 * @param hashtable     destination metadata hashtable
 * @param ml            metadata location
 * @param key           hashtable key
 * @return              return code (LOAD_METADATA_OK or LOAD_METADATA_ERR)
 */
int load_xml_metadata(GHashTable *hashtable, struct MetadataLocation *ml, HashTableKey key);

/** \ingroup load_metadata
 * Locate and load metadata from the specified path.
 * @param hashtable     destination metadata hashtable
 * @param repopath      path to repo (to directory with repodata/ subdir)
 * @param key           hashtable key
 * @return              return code (LOAD_METADATA_OK or LOAD_METADATA_ERR)
 */
int locate_and_load_xml_metadata(GHashTable *hashtable, const char *repopath, HashTableKey key);

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_LOAD_METADATA_H__ */
