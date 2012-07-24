/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
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
    CR_HT_KEY_DEFAULT,                  /*!< default = pkg hash */
    CR_HT_KEY_HASH = CR_HT_KEY_DEFAULT, /*!< pkg hash (cr_Package ->pkgId) */
    CR_HT_KEY_NAME,                     /*!< pkg name (cr_Package ->name) */
    CR_HT_KEY_FILENAME                  /*!< pkg filename (cr_Package
                                             ->location_href) */
} cr_HashTableKey;

/**@{*/
#define CR_LOAD_METADATA_OK        0  /*!< Metadata loaded successfully */
#define CR_LOAD_METADATA_ERR       1  /*!< Error while loading metadata */
/**@}*/

/** \ingroup load_metadata
 * Create new (empty) metadata hashtable.
 * @return              empty metadata hashtable
 */
GHashTable *cr_new_metadata_hashtable();

/** \ingroup load_metadata
 * Destroys all keys and values in the metadata hash table and decrements
 * its reference count by 1.
 * @param hashtable     metadata hashtable
 */
void cr_destroy_metadata_hashtable(GHashTable *hashtable);

/** \ingroup load_metadata
 * Load metadata from the specified location.
 * @param hashtable     destination metadata hashtable
 * @param ml            metadata location
 * @param key           hashtable key
 * @return              return code CR_LOAD_METADATA_OK or CR_LOAD_METADATA_ERR
 */
int cr_load_xml_metadata(GHashTable *hashtable,
                         struct cr_MetadataLocation *ml,
                         cr_HashTableKey key);

/** \ingroup load_metadata
 * Locate and load metadata from the specified path.
 * @param hashtable     destination metadata hashtable
 * @param repopath      path to repo (to directory with repodata/ subdir)
 * @param key           hashtable key
 * @return              return code CR_LOAD_METADATA_OK or CR_LOAD_METADATA_ERR
 */
int cr_locate_and_load_xml_metadata(GHashTable *hashtable,
                                    const char *repopath,
                                    cr_HashTableKey key);

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_LOAD_METADATA_H__ */
