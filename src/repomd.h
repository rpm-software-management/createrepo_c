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

#include <glib.h>
#include "checksum.h"
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
 * cr_Repomd *md = cr_repomd_new();
 * cr_RepomdRecord *rec;
 *
 * cr_xml_dump_init();
 *
 * // Set some repomd stuff
 * cr_repomd_set_revision(md, "007");
 * cr_repomd_add_repo_tag(md, "repotag");
 * cr_repomd_add_content_tag(md, "contenttag");
 * cr_repomd_add_distro_tag(md, "foocpeid", "data");
 *
 * // Create record for new metadata file
 * rec = cr_repomd_record_new("primary", "/foo/bar/repodata/primary.xml.xz");
 * // Calculate all needed parameters (uncompresed size, checksum, ...)
 * cr_repomd_record_fill(rec, CR_CHECKSUM_SHA256);
 * // Rename source file - insert checksum into the filename
 * cr_repomd_record_rename_file(rec)
 * // Append the record into the repomd
 * cr_repomd_set_record(md, rec);
 *
 * // Get repomd.xml content
 * xml = cr_xml_dump_repomd(md, NULL);
 *
 * // Cleanup
 * cr_repomd_free(md);
 * cr_xml_dump_cleanup();
 * \endcode
 *
 *  \addtogroup repomd
 *  @{
 */

/** Internal representation of cr_RepomdRecord object
 */
typedef struct {
    char *type;                 /*!< type of record */
    char *location_real;        /*!< real path to the file */
    char *location_href;        /*!< location of the file (in repomd.xml) */
    char *location_base;        /*!< base location of the file */
    char *checksum;             /*!< checksum of file */
    char *checksum_type;        /*!< checksum type */
    char *checksum_open;        /*!< checksum of uncompressed file */
    char *checksum_open_type;   /*!< checksum type of uncompressed file */
    char *checksum_header;      /*!< checksum of header */
    char *checksum_header_type; /*!< checksum type of header */
    gint64 timestamp;           /*!< mtime of the file */
    gint64 size;                /*!< size of file in bytes */
    gint64 size_open;           /*!< size of uncompressed file in bytes */
    gint64 size_header;         /*!< header size */
    int db_ver;                 /*!< version of database */

    GStringChunk *chunk;        /*!< String chunk */
} cr_RepomdRecord;

/** Distro tag structure
 */
typedef struct {
    gchar *cpeid;   /*!< cpeid value or NULL */
    gchar *val;     /*!< Tag value */
} cr_DistroTag;

/** Internal representation of cr_Repomd object
 */
typedef struct {
    gchar *revision;            /*!< Revison */
    gchar *repoid;              /*!< OBSOLETE, replaced by contenthash */
    gchar *repoid_type;         /*!< OBSOLETE, replaced by contenthash_type */
    gchar *contenthash;         /*!< Content hash */
    gchar *contenthash_type;    /*!< Content hash type ("sha256", ...) */
    GSList *repo_tags;          /*!< List of strings */
    GSList *content_tags;       /*!< List of strings */
    GSList *distro_tags;        /*!< List of cr_DistroTag* */
    GSList *records;            /*!< List with cr_RepomdRecords */

    GStringChunk *chunk;        /*!< String chunk for repomd strings
                                     (Note: RepomdRecord strings are stored
                                      in RepomdRecord->chunk) */
} cr_Repomd;

/** Creates (alloc) new cr_RepomdRecord object
 * @param type                  Type of record ("primary", "filelists", ..)
 * @param path                  path to the compressed file
 */
cr_RepomdRecord *cr_repomd_record_new(const char *type, const char *path);

/** Destroy cr_RepomdRecord object.
 * NOTE: Do NOT use this function on objects attached to cr_Repomd
 * (by cr_repomd_set_record).
 * @param record                cr_RepomdRecord object
 */
void cr_repomd_record_free(cr_RepomdRecord *record);

/** Copy cr_RepomdRecord object.
 * @param orig              cr_RepomdRecord object
 * @return                  copy of cr_RepomdRecord object
 */
cr_RepomdRecord *cr_repomd_record_copy(const cr_RepomdRecord *orig);

/** Fill unfilled items in the cr_RepomdRecord (calculate checksums,
 * get file size before/after compression, etc.).
 * Note: If checksum_open, checksum_open_type and size_open are filed
 * then their calculation will be skiped. This items could be filled
 * directly on our own or use function for load them from a cr_ContentStat.
 * If no open stats are supplied, then this function has to decompress
 * the file for the open checksum calculation.
 * @param record                cr_RepomdRecord object
 * @param checksum_type         type of checksum to use
 * @param err                   GError **
 * @return                      cr_Error code
 */
int cr_repomd_record_fill(cr_RepomdRecord *record,
                          cr_ChecksumType checksum_type,
                          GError **err);

/** Almost analogous to cr_repomd_record_fill but suitable for groupfile.
 * Record must be set with the path to existing non compressed groupfile.
 * Compressed file will be created and compressed_record updated.
 * @param record                cr_RepomdRecord initialized to an existing
 *                              uncompressed file
 * @param compressed_record     empty cr_RepomdRecord object that will by filled
 * @param checksum_type         type of checksums
 * @param compression           type of compression
 * @param zck_dict_dir          Location of zchunk dictionaries (NULL if unused)
 * @param err                   GError **
 * @return                      cr_Error code
 */
int cr_repomd_record_compress_and_fill(cr_RepomdRecord *record,
                                       cr_RepomdRecord *compressed_record,
                                       cr_ChecksumType checksum_type,
                                       cr_CompressionType compression,
                                       const char *zck_dict_dir,
                                       GError **err);

/** Add a hash as prefix to the filename.
 * @param record                cr_RepomdRecord of file to be renamed
 * @param err                   GError **
 * @return                      cr_Error code
 */
int cr_repomd_record_rename_file(cr_RepomdRecord *record, GError **err);

/** Set timestamp of the file. Needed to reproduce bit-by-bit identical metadata.
 * @param record                cr_RepomdRecord of file to be renamed
 * @param timestamp             timestamp in number of seconds since 1970-01-01
 */
void cr_repomd_record_set_timestamp(cr_RepomdRecord *record, gint64 timestamp);

/** Load the open stats (checksum_open, checksum_open_type and size_open)
 * from the cr_ContentStat object.
 * @param record                cr_RepomdRecord
 * @param stats                 cr_ContentStat
 */
void cr_repomd_record_load_contentstat(cr_RepomdRecord *record,
                                       cr_ContentStat *stats);

/** Load the zchunk stats (zck_header_checksum, zck_header_checksum_type and zck_header_size)
 * from the cr_ContentStat object.
 * @param record                cr_RepomdRecord
 * @param stats                 cr_ContentStat
 */
void cr_repomd_record_load_zck_contentstat(cr_RepomdRecord *record,
                                           cr_ContentStat *stats);

/** Create new empty cr_Repomd object wich represents content of repomd.xml.
 */
cr_Repomd *cr_repomd_new();

/** Create copy of cr_Repomd
 * @param repomd                cr_Repomd object
 * @return                      Copy of the input cr_Repomd object
 */
cr_Repomd *cr_repomd_copy(cr_Repomd *repomd);

/** Set cr_Repomd record into cr_Repomd object.
 * @param repomd                cr_Repomd object
 * @param record                cr_RepomdRecord object
 */
void cr_repomd_set_record(cr_Repomd *repomd, cr_RepomdRecord *record);

/** Set custom revision string of repomd.
 * @param repomd                cr_Repomd object
 * @param revision              revision string
 */
void cr_repomd_set_revision(cr_Repomd *repomd, const char *revision);

/** Set a repoid - OBSOLETE, use cr_repomd_set_contenthash instead
 * @param repomd                cr_Repomd object
 * @param repoid                RepoId
 * @param type                  Type of hash function used to calculate repoid
 */
void cr_repomd_set_repoid(cr_Repomd *repomd,
                          const char *repoid,
                          const char *type);

/** Set a contenthash
 * @param repomd                cr_Repomd object
 * @param hash                  content hash
 * @param type                  Type of hash function
 */
void cr_repomd_set_contenthash(cr_Repomd *repomd,
                               const char *hash,
                               const char *type);

/** Add distro tag.
 * @param repomd                cr_Repomd object
 * @param cpeid                 cpeid string (could be NULL)
 * @param tag                   distro tag string
 */
void cr_repomd_add_distro_tag(cr_Repomd *repomd,
                              const char *cpeid,
                              const char *tag);

/** Add repo tag.
 * @param repomd                cr_Repomd object
 * @param tag                   repo tag
 */
void cr_repomd_add_repo_tag(cr_Repomd *repomd, const char *tag);

/** Add content tag.
 * @param repomd                cr_Repomd object
 * @param tag                   content tag
 */
void cr_repomd_add_content_tag(cr_Repomd *repomd, const char *tag);

/** Get repomd record from the repomd object.
 * @param repomd                cr_Repomd object
 * @param type                  Type of the record ("primary", "primary_db", ..)
 * @return                      Pointer to a record of desired type or NULL
 */
cr_RepomdRecord *cr_repomd_get_record(cr_Repomd *repomd, const char *type);

/** Detach this record from the repomd records list.
 * Note: Responsibility of freeing record memory no longer belongs to
 * cr_Repomd object.
 * @param repomd                cr_Repomd object
 * @param rec                   Record to be removed
 */
void cr_repomd_detach_record(cr_Repomd *repomd, cr_RepomdRecord *rec);

/** Remove first record of the specified type
 * @param repomd                cr_Repomd object
 * @param type                  Type of the record ("primary", "primary_db", ..)
 */
void cr_repomd_remove_record(cr_Repomd *repomd, const char *type);

/** Records are stored in order they were added to the repomd.
 * Because sometimes deterministic output is desirable this function
 * exists.
 * @param repomd                cr_Repomd object
 */
void cr_repomd_sort_records(cr_Repomd *repomd);

/** Frees cr_Repomd object and all its cr_RepomdRecord objects
 * @param repomd                cr_Repomd object
 */
void cr_repomd_free(cr_Repomd *repomd);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_REPOMD_H__ */
