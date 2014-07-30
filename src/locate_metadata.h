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

#ifndef __C_CREATEREPOLIB_LOCATE_METADATA_H__
#define __C_CREATEREPOLIB_LOCATE_METADATA_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup   locate_metadata Locate metadata API.
 *  \addtogroup locate_metadata
 *  @{
 */

/** Structure representing metadata location.
 */
struct cr_MetadataLocation {
    char *pri_xml_href;         /*!< path to primary.xml */
    char *fil_xml_href;         /*!< path to filelists.xml */
    char *oth_xml_href;         /*!< path to other.xml */
    char *pri_sqlite_href;      /*!< path to primary.sqlite */
    char *fil_sqlite_href;      /*!< path to filelists.sqlite */
    char *oth_sqlite_href;      /*!< path to other.sqlite */
    char *groupfile_href;       /*!< path to groupfile */
    char *cgroupfile_href;      /*!< path to compressed groupfile */
    char *updateinfo_href;      /*!< path to updateinfo */
    char *repomd;               /*!< path to repomd.xml */
    char *original_url;         /*!< original path of repo from commandline
                                     param */
    char *local_path;           /*!< local path to repo */
    int  tmp;                   /*!< if true - metadata were downloaded and
                                     will be removed during
                                     cr_metadata_location_free*/
};

/** Parses repomd.xml and returns a filled cr_MetadataLocation structure.
 * Remote repodata (repopath with prefix "ftp://" or "http://") are dowloaded
 * into a temporary directory and removed when the cr_metadatalocation_free()
 * is called on the cr_MetadataLocation.
 * @param repopath      path to directory with repodata/ subdirectory
 * @param ignore_sqlite if ignore_sqlite != 0 sqlite dbs are ignored
 * @param err           GError **
 * @return              filled cr_MetadataLocation structure or NULL
 */
struct cr_MetadataLocation *cr_locate_metadata(const char *repopath,
                                               int ignore_sqlite,
                                               GError **err);

/** Free cr_MetadataLocation. If repodata were downloaded remove
 * a temporary directory with repodata.
 * @param ml            MeatadaLocation
 */
void cr_metadatalocation_free(struct cr_MetadataLocation *ml);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_LOCATE_METADATA_H__ */
