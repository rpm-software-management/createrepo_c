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

/** \defgroup repomd            Repomd API.
 */

/** \ingroup repomd
 * cr_RepomdRecord - object representing an item from repomd.xml
 */
typedef struct _cr_RepomdRecord * cr_RepomdRecord;

/** \ingroup repomd
 * cr_Repomd - object representing repomd.xml
 */
typedef struct _cr_Repomd * cr_Repomd;

/** \ingroup repomd
 * Internal representation of cr_RepomdRecord object
 */
struct _cr_RepomdRecord {
    const char *location_href;  /*!< location of the file */
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

/** \ingroup repomd
 * Internal representation of cr_Repomd object
 */
struct _cr_Repomd {
    cr_RepomdRecord pri_xml;
    cr_RepomdRecord fil_xml;
    cr_RepomdRecord oth_xml;
    cr_RepomdRecord pri_sql;
    cr_RepomdRecord fil_sql;
    cr_RepomdRecord oth_sql;
    cr_RepomdRecord groupfile;
    cr_RepomdRecord cgroupfile;
    cr_RepomdRecord updateinfo;
    GSList *repo_tags;
    GSList *distro_tags;
    GSList *content_tags;
    gchar *revision;
};

/** \ingroup repomd
 * Enum of repomd record types
 */
typedef enum {
    CR_MD_PRIMARY_XML,
    CR_MD_FILELISTS_XML,
    CR_MD_OTHER_XML,
    CR_MD_PRIMARY_SQLITE,
    CR_MD_FILELISTS_SQLITE,
    CR_MD_OTHER_SQLITE,
    CR_MD_GROUPFILE,
    CR_MD_COMPRESSED_GROUPFILE,
    CR_MD_UPDATEINFO
} cr_RepomdRecordType;

/** \ingroup repomd
 * Creates (alloc) new cr_RepomdRecord object
 * @param path                  path to the compressed file
 */
cr_RepomdRecord cr_new_repomdrecord(const char *path);

/** \ingroup repomd
 * Destroy cr_RepomdRecord object.
 * NOTE: Do NOT use this function on objects attached to cr_Repomd
 * (by cr_repomd_set_record).
 * @param record                cr_RepomdRecord object
 */
void cr_free_repomdrecord(cr_RepomdRecord record);

/** \ingroup repomd
 * Fill unfilled items in the cr_RepomdRecord (calculate checksums,
 * get file size before/after compression, etc.).
 * Note: For groupfile you shoud use cr_process_groupfile_repomdrecord function.
 * @param base_path             path to repo (to directory with repodata subdir)
 * @param record                cr_RepomdRecord object
 * @param checksum_type         type of checksum to use
 */
int cr_fill_repomdrecord(const char *base_path,
                         cr_RepomdRecord record,
                         cr_ChecksumType *checksum_type);

/** \ingroup repomd
 * Analogue of cr_fill_repomdrecord but for groupfile.
 * Groupfile must be set with the path to existing non compressed groupfile.
 * Compressed group file will be created and compressed_groupfile record
 * updated.
 * @param base_path             path to repo (to directory with repodata subdir)
 * @param groupfile             cr_RepomdRecord initialized to an existing
 *                              groupfile
 * @param compressed_groupfile  empty cr_RepomdRecord object that will by filled
 * @param checksum_type         type of checksums
 * @param compression           type of compression
 */
void cr_process_groupfile_repomdrecord(const char *base_path,
                                       cr_RepomdRecord groupfile,
                                       cr_RepomdRecord compressed_groupfile,
                                       cr_ChecksumType *checksum_type,
                                       cr_CompressionType compression);

/** \ingroup repomd
 * Add a hash as prefix to the filename.
 * @param base_path             path to repo (to directory with repodata/
 *                              subdir)
 * @param record                cr_RepomdRecord of file to be renamed
 */
void cr_rename_repomdrecord_file(const char *base_path, cr_RepomdRecord record);

/** \ingroup repomd
 * Create new empty cr_Repomd object wich represents content of repomd.xml.
 */
cr_Repomd cr_new_repomd();

/** \ingroup repomd
 * Set cr_Repomd record into cr_Repomd object.
 * @param repomd                cr_Repomd object
 * @param record                cr_RepomdRecord object
 * @param type                  type of cr_RepomdRecord object
 */
void cr_repomd_set_record(cr_Repomd repomd,
                          cr_RepomdRecord record,
                          cr_RepomdRecordType type);

/** \ingroup repomd
 * Set custom revision string of repomd.
 * @param repomd                cr_Repomd object
 * @param revision              revision string
 */
void cr_repomd_set_revision(cr_Repomd repomd, const char *revision);

/** \ingroup repomd
 * Add distro tag.
 * @param repomd                cr_Repomd object
 * @param cpeid                 cpeid string (could be NULL)
 * @param tag                   distro tag string
 */
void cr_repomd_add_distro_tag(cr_Repomd repomd,
                              const char *cpeid,
                              const char *tag);

/** \ingroup repomd
 * Add repo tag.
 * @param repomd                cr_Repomd object
 * @param repo                  repo tag
 */
void cr_repomd_add_repo_tag(cr_Repomd repomd, const char *tag);

/** \ingroup repomd
 * Add content tag.
 * @param repomd                cr_Repomd object
 * @param content               content tag
 */
void cr_repomd_add_content_tag(cr_Repomd repomd, const char *tag);

/** \ingroup repomd
 * Frees cr_Repomd object and all its cr_RepomdRecord objects
 * @param repomd                cr_Repomd object
 */
void cr_free_repomd(cr_Repomd repomd);


/** \ingroup repomd
 * Generate repomd.xml content.
 * @param repomd                cr_Repomd object
 * @return                      string with repomd.xml content
 */
gchar *cr_generate_repomd_xml(cr_Repomd repomd);

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_REPOMD_H__ */
