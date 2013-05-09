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

#ifndef __C_CREATEREPOLIB_REPOMD_H__
#define __C_CREATEREPOLIB_REPOMD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "compression_wrapper.h"
#include "package.h"

/** \defgroup   repomd            Repomd API.
 *
 * Module for generating repomd.xml.
 *
 * Example:
 *
 * \code
 * char *xml;
 * cr_Repomd md = cr_repomd_new();
 * cr_RepomdRecord rec;
 *
 * // Set some repomd stuff
 * cr_repomd_set_revision(md, "007");
 * cr_repomd_add_repo_tag(md, "repotag");
 * cr_repomd_add_content_tag(md, "contenttag");
 * cr_repomd_add_distro_tag(md, "foocpeid", "data");
 *
 * // Create record for new metadata file
 * rec = cr_repomd_record_new("/foo/bar/repodata/primary.xml.xz");
 * // Calculate all needed parameters (uncompresed size, checksum, ...)
 * cr_repomd_record_fill(rec, CR_CHECKSUM_SHA256);
 * // Rename source file - insert checksum into the filename
 * cr_repomd_record_rename_file(rec)
 * // Append the record into the repomd
 * cr_repomd_set_record(md, rec,  "primary");
 *
 * // Get repomd.xml content
 * // Note: cr_xml_dump_init() shoud be once called before dump
 * xml = cr_repomd_xml_dump(md);
 * cr_repomd_free(md);
 * \endcode
 *
 *  \addtogroup repomd
 *  @{
 */

/** cr_RepomdRecord - object representing an item from repomd.xml
 */
typedef struct _cr_RepomdRecord * cr_RepomdRecord;

/** cr_Repomd - object representing repomd.xml
 */
typedef struct _cr_Repomd * cr_Repomd;

/** Internal representation of cr_RepomdRecord object
 */
struct _cr_RepomdRecord {
    char *location_real;        /*!< real path to the file */
    char *location_href;        /*!< location of the file (in repomd.xml) */
    char *checksum;             /*!< checksum of file */
    char *checksum_type;        /*!< checksum type */
    char *checksum_open;        /*!< checksum of uncompressed file */
    char *checksum_open_type;   /*!< checksum type of uncompressed file*/
    long timestamp;             /*!< mtime of the file */
    long size;                  /*!< size of file in bytes */
    long size_open;             /*!< size of uncompressed file in bytes */
    int db_ver;                 /*!< version of database */

    GStringChunk *chunk;        /*!< string chunk */
};

/** Internal representation of cr_Repomd object
 */
struct _cr_Repomd {
    GHashTable *records;
    GSList *repo_tags;
    GSList *distro_tags;
    GSList *content_tags;
    gchar *revision;
};

/** Creates (alloc) new cr_RepomdRecord object
 * @param path                  path to the compressed file
 */
cr_RepomdRecord cr_repomd_record_new(const char *path);

/** Destroy cr_RepomdRecord object.
 * NOTE: Do NOT use this function on objects attached to cr_Repomd
 * (by cr_repomd_set_record).
 * @param record                cr_RepomdRecord object
 */
void cr_repomd_record_free(cr_RepomdRecord record);

/** Fill unfilled items in the cr_RepomdRecord (calculate checksums,
 * get file size before/after compression, etc.).
 * Note: For groupfile you shoud use cr_repomd_record_compress_and_fill
 * function.
 * @param record                cr_RepomdRecord object
 * @param checksum_type         type of checksum to use
 * @param err                   GError **
 * @return                      cr_Error code
 */
int cr_repomd_record_fill(cr_RepomdRecord record,
                          cr_ChecksumType checksum_type,
                          GError **err);

/** Almost analogue of cr_repomd_record_fill but suitable for groupfile.
 * Record must be set with the path to existing non compressed groupfile.
 * Compressed file will be created and compressed_record updated.
 * @param record                cr_RepomdRecord initialized to an existing
 *                              uncompressed file
 * @param compressed_record     empty cr_RepomdRecord object that will by filled
 * @param checksum_type         type of checksums
 * @param compression           type of compression
 * @param err                   GError **
 * @return                      cr_Error code
 */
int cr_repomd_record_compress_and_fill(cr_RepomdRecord record,
                                       cr_RepomdRecord compressed_record,
                                       cr_ChecksumType checksum_type,
                                       cr_CompressionType compression,
                                       GError **err);

/** Add a hash as prefix to the filename.
 * @param record                cr_RepomdRecord of file to be renamed
 * @param err                   GError **
 * @return                      cr_Error code
 */
int cr_repomd_record_rename_file(cr_RepomdRecord record, GError **err);

/** Create new empty cr_Repomd object wich represents content of repomd.xml.
 */
cr_Repomd cr_repomd_new();

/** Set cr_Repomd record into cr_Repomd object.
 * @param repomd                cr_Repomd object
 * @param record                cr_RepomdRecord object
 * @param type                  type of record ("primary, "groupfile", ...)
 */
void cr_repomd_set_record(cr_Repomd repomd,
                          cr_RepomdRecord record,
                          const char *type);

/** Set custom revision string of repomd.
 * @param repomd                cr_Repomd object
 * @param revision              revision string
 */
void cr_repomd_set_revision(cr_Repomd repomd, const char *revision);

/** Add distro tag.
 * @param repomd                cr_Repomd object
 * @param cpeid                 cpeid string (could be NULL)
 * @param tag                   distro tag string
 */
void cr_repomd_add_distro_tag(cr_Repomd repomd,
                              const char *cpeid,
                              const char *tag);

/** Add repo tag.
 * @param repomd                cr_Repomd object
 * @param tag                   repo tag
 */
void cr_repomd_add_repo_tag(cr_Repomd repomd, const char *tag);

/** Add content tag.
 * @param repomd                cr_Repomd object
 * @param tag                   content tag
 */
void cr_repomd_add_content_tag(cr_Repomd repomd, const char *tag);

/** Frees cr_Repomd object and all its cr_RepomdRecord objects
 * @param repomd                cr_Repomd object
 */
void cr_repomd_free(cr_Repomd repomd);


/** Generate repomd.xml content.
 * Don't forget to use cr_xml_dump_init() before call this function.
 * @param repomd                cr_Repomd object
 * @return                      string with repomd.xml content
 */
gchar *cr_repomd_xml_dump(cr_Repomd repomd);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_REPOMD_H__ */
