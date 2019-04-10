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
    GSList *additional_metadata; /*!< list of cr_Metadatum: paths 
                                      to additional metadata such 
                                      as updateinfo, modulemd, .. */
    char *repomd;               /*!< path to repomd.xml */
    char *original_url;         /*!< original path of repo from commandline
                                     param */
    char *local_path;           /*!< local path to repo */
    int  tmp;                   /*!< if true - metadata were downloaded and
                                     will be removed during
                                     cr_metadata_location_free*/
};

/** Structure representing additional metadata location and type.
 *  It is used to first hold old and later new location while keeping
 *  type information.
 */
typedef struct {
    gchar *name;
    gchar *type;
} cr_Metadatum;

struct cr_MetadataLocation *
cr_parse_repomd(const char *repomd_path, const char *repopath, int ignore_sqlite);

/** Inserts additional metadatum to list of
 *  additional metadata if this type is already
 *  present it gets overridden
 *
 * @param path                  Path to metadatum
 * @param type                  Type of metadatum
 * @param additional_metadata   List of additional metadata
 * @return                      Original list with new element
 */
GSList*
cr_insert_additional_metadatum(const gchar *path,
                               const gchar *type,
                               GSList *additional_metadata);

/** Compares type (string) of specified metadatum 
 *  with second parameter string (type)
 *
 * @param metadatum             Cmp type of this metadatum
 * @param type                  String value
 * @return                      an integer less than, equal to, or greater than zero,
 *                              if metadatum type is <, == or > than type (string cmp)
 */
gint cr_cmp_metadatum_type(gconstpointer metadatum, gconstpointer type);

/** Compares type (string) of specified cr_RepomdRecord 
 *  with second parameter string (type)
 *
 * @param cr_RepomdRecord       Cmp type of this cr_RepomdRecord
 * @param type                  String value
 * @return                      an integer less than, equal to, or greater than zero,
 *                              if repomdRecord type is <, == or > than type (string cmp)
 */
gint cr_cmp_repomd_record_type(gconstpointer repomd_record, gconstpointer type);

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
                                               gboolean ignore_sqlite,
                                               GError **err);

/** Free cr_MetadataLocation. If repodata were downloaded remove
 * a temporary directory with repodata.
 * @param ml            MeatadaLocation
 */
void cr_metadatalocation_free(struct cr_MetadataLocation *ml);

/** Free cr_Metadatum. 
 * @param m            Meatadatum
 */
void cr_metadatum_free(cr_Metadatum *m);

/** Copies metadata files, exactly, even hashed name
 *  It first constructs target path (location + name),
 *  Then it copies file to that location
 *
 *  Metadatum as in singular of metadata, it is eg. groupfile, updateinfo..
 *
 * @param src               From where are we copying
 * @param tmp_out_repo      Copying destination dir
 * @param err               GError **
 * @return                  Path to copied file
 */
gchar* cr_copy_metadatum(const gchar *src, const gchar *tmp_out_repo, GError **err);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_LOCATE_METADATA_H__ */
