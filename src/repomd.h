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

#ifndef __C_CREATEREPOLIB_REPOMD_H__
#define __C_CREATEREPOLIB_REPOMD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "compression_wrapper.h"
#include "package.h"

/** \defgroup repomd            Repomd API.
 */

/** \ingroup misc
 * cr_RepomdRecord object
 */
typedef struct _cr_RepomdRecord * cr_RepomdRecord;

/** \ingroup misc
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
 * Creates (alloc) new cr_RepomdRecord object
 * @param path                  path to the compressed file
 */
cr_RepomdRecord cr_new_repomdrecord(const char *path);

/** \ingroup repomd
 * Destroy cr_RepomdRecord object
 * @param record                cr_RepomdRecord object
 */
void cr_free_repomdrecord(cr_RepomdRecord record);

/** \ingroup repomd
 * Fill unfilled items in the cr_RepomdRecord (calculate checksums,
 * get file size before/after compression, etc.).
 * Note: For groupfile you shoud use cr_process_groupfile function.
 * @param base_path             path to repo (to directory with repodata subdir)
 * @param record                cr_RepomdRecord object
 * @param checksum_type         type of checksum to use
 */
int cr_fill_missing_data(const char *base_path,
                         cr_RepomdRecord record,
                         cr_ChecksumType *checksum_type);

/** \ingroup repomd
 * Analogue of cr_fill_missing_data but for groupfile.
 * Groupfile must be set with the path to existing non compressed groupfile.
 * Compressed group file will be created and compressed_groupfile record
 * updated.
 * @param base_path             path to repo (to directory with repodata subdir)
 * @param groupfile             cr_RepomdRecord initialized to an existing
 *                              groupfile
 * @param compressed_groupfile  cr_RepomdRecord object that will by filled
 * @param checksum_type         type of checksums
 * @param compression           type of compression
 */
void cr_process_groupfile(const char *base_path,
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
void cr_rename_file(const char *base_path, cr_RepomdRecord record);

/** \ingroup repomd
 * Generate repomd.xml content.
 * @param path                  path to repository (to the directory contains
 *                              repodata/ subdir)
 * @param pri_xml               cr_RepomdRecord of primary.xml.gz (relative
 *                              towards to the path param)
 * @param fil_xml               cr_RepomdRecord of filelists.xml.gz (relative
 *                              towards to the path param)
 * @param oth_xml               cr_RepomdRecord of other.xml.gz (relative
 *                              towards to the path param)
 * @param pri_sqlite            cr_RepomdRecord of primary.sqlite.* (relative
 *                              towards to the path param)
 * @param fil_sqlite            cr_RepomdRecord of filelists.sqlite.* (relative
 *                              towards to the path param)
 * @param oth_sqlite            cr_RepomdRecord of other.sqlite.* (relative
 *                              towards to the path param)
 * @param groupfile             cr_RepomdRecord of *.xml (relative towards to
 *                              the path param)
 * @param cgroupfile            cr_RepomdRecord of *.xml.* (relative towards to
 *                              the path param)
 * @param update_info           cr_RepomdRecord of updateinfo.xml.* (relative
 *                              towards to the path param)
 * @return                      string with repomd.xml content
 */
gchar * cr_xml_repomd(const char *path, cr_RepomdRecord pri_xml,
                      cr_RepomdRecord fil_xml, cr_RepomdRecord oth_xml,
                      cr_RepomdRecord pri_sqlite, cr_RepomdRecord fil_sqlite,
                      cr_RepomdRecord oth_sqlite, cr_RepomdRecord groupfile,
                      cr_RepomdRecord cgroupfile, cr_RepomdRecord update_info);

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_REPOMD_H__ */
