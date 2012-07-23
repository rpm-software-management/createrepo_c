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

#ifndef __C_CREATEREPOLIB_LOCATE_METADATA_H__
#define __C_CREATEREPOLIB_LOCATE_METADATA_H__

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup   locate_metadata Locate metadata API.
 */

/** \ingroup locate_metadata
 * Structure representing metadata location.
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
    char *repomd;               /*!< path to repomd.xml */
    char *original_url;         /*!< original path of repo from commandline
                                     param */
    char *local_path;           /*!< local path to repo */
    char *tmp_dir;              /*!< path to dir where metadata are stored if
                                     metadata were downloaded */
};

/** \ingroup locate_metadata
 * Parses repomd.xml and returns a filled cr_MetadataLocation structure.
 * Remote repodata (repopath with prefix "ftp://" or "http://") are dowloaded
 * into a temporary directory and removed when the cr_free_metadata_location()
 * is called on the cr_MetadataLocation.
 * @param repopath      path to directory with repodata/ subdirectory
 * @param ignore_sqlite if ignore_sqlite != 0 sqlite dbs are ignored
 * @return              filled cr_MetadataLocation structure
 */
struct cr_MetadataLocation *cr_get_metadata_location(const char *repopath,
                                                     int ignore_sqlite);

/** \ingroup locate_metadata
 * Free cr_MetadataLocation. If repodata were downloaded remove
 * a temporary directory with repodata.
 * @param ml            MeatadaLocation
 */
void cr_free_metadata_location(struct cr_MetadataLocation *ml);

/** \ingroup locate_metadata
 * Remove files related to repodata from the specified path.
 * Files not listed in repomd.xml and with nonstandard names (standard names
 * are names with suffixes like primary.xml.*, primary.sqlite.*, other.xml.*,
 * etc.) are keep untouched (repodata/ subdirectory IS NOT removed!).
 * @param repopath      path to directory with repodata/ subdirectory
 * @return              number of removed files
 */
int cr_remove_metadata(const char *repopath);

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_LOCATE_METADATA_H__ */
