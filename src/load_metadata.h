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
 *  \addtogroup load_metadata
 *  @{
 */

/** Package attribute used as key in the hashtable.
 */
typedef enum {
    CR_HT_KEY_DEFAULT,                  /*!< default = pkg hash */
    CR_HT_KEY_HASH = CR_HT_KEY_DEFAULT, /*!< pkg hash (cr_Package ->pkgId) */
    CR_HT_KEY_NAME,                     /*!< pkg name (cr_Package ->name) */
    CR_HT_KEY_FILENAME                  /*!< pkg filename (cr_Package
                                             ->location_href) */
} cr_HashTableKey;

/** Return cr_HashTableKey from a cr_Metadata */
#define cr_metadata_key(X)          ((X)->key)
/** Return hashtable from a cr_Metadata */
#define cr_metadata_hashtable(X)    ((X)->ht)
/** Return GStringChunk from a cr_Metadata */
#define cr_metadata_chunk(X)        ((X)->chunk)

/** Structure for loaded metadata
 */
struct _cr_Metadata {
    cr_HashTableKey key;    /*!< key used in hashtable */
    GHashTable *ht;         /*!< hashtable with packages */
    GStringChunk *chunk;    /*!< NULL or string chunk with strings from htn */
    GHashTable *pkglist_ht; /*!< list of allowed package basenames to load */
};

/** Pointer to structure with loaded metadata
 */
typedef struct _cr_Metadata *cr_Metadata;

#define CR_LOAD_METADATA_OK        0  /*!< Metadata loaded successfully */
#define CR_LOAD_METADATA_ERR       1  /*!< Error while loading metadata */

/** Create new (empty) metadata hashtable.
 * It is NOT thread safe to load data into single cr_Metadata
 * from multiple threads. But non modifying access to the loaded data
 * in cr_Metadata is thread safe.
 * @param key               key specifies which value will be (is) used as key
 *                          in hash table
 * @param use_single_chunk  use only one string chunk (all loaded packages
 *                          share one string chunk in the cr_Metadata object)
 *                          Packages will not be standalone objects.
 *                          This option leads to less memory consumption.
 * @param pkglist           load only packages which base filename is in this
 *                          list. If param is NULL all packages are loaded.
 * @return                  empty cr_Metadata object
 */
cr_Metadata cr_metadata_new(cr_HashTableKey key,
                            int use_single_chunk,
                            GSList *pkglist);

/** Destroy metadata.
 * @param md            cr_Metadata object
 */
void cr_metadata_free(cr_Metadata md);

/** Load metadata from the specified location.
 * @param md            metadata object
 * @param ml            metadata location
 * @return              return code CR_LOAD_METADATA_OK or CR_LOAD_METADATA_ERR
 */
int cr_metadata_load_xml(cr_Metadata md,
                         struct cr_MetadataLocation *ml);

/** Locate and load metadata from the specified path.
 * @param md            metadata object
 * @param repopath      path to repo (to directory with repodata/ subdir)
 * @return              return code CR_LOAD_METADATA_OK or CR_LOAD_METADATA_ERR
 */
int cr_metadata_locate_and_load_xml(cr_Metadata md,
                                    const char *repopath);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_LOAD_METADATA_H__ */
