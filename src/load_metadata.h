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
 *
 * Module for loading yum xml metadata.
 *
 * Example:
 *
 * \code
 * int ret;
 * cr_Metadata *oldmetadata;
 * GHashTable hashtable;
 *
 * // Create new metadata object
 * oldmetadata = cr_metadata_new(CR_HT_KEY_FILENAME, 1, NULL);
 * // Load metadata (path to directory which contains repodata/ subdir)
 * ret = cr_metadata_locate_and_load_xml(oldmetadata, "/foo/bar/repo/")
 * // Check return code
 * if (ret != CR_LOAD_METADATA_OK) exit(1);
 * // Get hash table with all loaded packages (key is package relative path)
 * hashtable = cr_metadata_hashtable(oldmetadata);
 * // What to do with hashtable?
 * // See: http://developer.gnome.org/glib/2.30/glib-Hash-Tables.html
 * \endcode
 *
 *  \addtogroup load_metadata
 *  @{
 */

/** Package attribute used as key in the hashtable.
 */
typedef enum {
    CR_HT_KEY_DEFAULT,                  /*!< default = pkg hash */
    CR_HT_KEY_HASH = CR_HT_KEY_DEFAULT, /*!< pkg hash (cr_Package ->pkgId) */
    CR_HT_KEY_NAME,                     /*!< pkg name (cr_Package ->name) */
    CR_HT_KEY_FILENAME,                 /*!< pkg filename (cr_Package
                                             ->location_href) */
    CR_HT_KEY_HREF,                     /*!< pkg location */
    CR_HT_KEY_SENTINEL,                 /*!< last element, terminator, .. */
} cr_HashTableKey;

/** Internally, and by default, the loaded metadata are
 *  indexed by pkgId (pkg's hash). But when you select different
 *  attribute for indexing (see cr_HashTableKey).
 *  The situation that multiple packages has the same (key) attribute.
 *  This enum lists provided methos for resolution of such conflicts.
 */
typedef enum {
    CR_HT_DUPACT_KEEPFIRST = 0,    /*!<
        First encontered item wins, other will be removed - Default */
    CR_HT_DUPACT_REMOVEALL,        /*!<
        Remove all conflicting items. */
    CR_HT_DUPACT_SENTINEL,         /*!<
         Last element, terminator, ... */
} cr_HashTableKeyDupAction;

/** Metadata object
 */
typedef struct _cr_Metadata cr_Metadata;

/** Return cr_HashTableKey from a cr_Metadata
 * @param md        cr_Metadata object.
 * @return          Key type
 */
cr_HashTableKey cr_metadata_key(cr_Metadata *md);

/** Return hashtable from a cr_Metadata
 * @param md        cr_Metadata object.
 * @return          Pointer to internal GHashTable.
 */
GHashTable *cr_metadata_hashtable(cr_Metadata *md);

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
cr_Metadata *cr_metadata_new(cr_HashTableKey key,
                             int use_single_chunk,
                             GSList *pkglist);

/** Set action how to deal with duplicated items.
 */
gboolean
cr_metadata_set_dupaction(cr_Metadata *md, cr_HashTableKeyDupAction dupaction);

/** Destroy metadata.
 * @param md            cr_Metadata object
 */
void cr_metadata_free(cr_Metadata *md);

/** Load metadata from the specified location.
 * @param md            metadata object
 * @param ml            metadata location
 * @param err           GError **
 * @return              cr_Error code
 */
int cr_metadata_load_xml(cr_Metadata *md,
                         struct cr_MetadataLocation *ml,
                         GError **err);

/** Locate and load metadata from the specified path.
 * @param md            metadata object
 * @param repopath      path to repo (to directory with repodata/ subdir)
 * @param err           GError **
 * @return              cr_Error code
 */
int cr_metadata_locate_and_load_xml(cr_Metadata *md,
                                    const char *repopath,
                                    GError **err);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_LOAD_METADATA_H__ */
