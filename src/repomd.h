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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __C_CREATEREPOLIB_REPOMD_H__
#define __C_CREATEREPOLIB_REPOMD_H__

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "compression_wrapper.h"
#include "package.h"

/** \defgroup repomd            Repomd API.
 */

/** \ingroup misc
 * RepomdRecord object
 */
typedef struct _RepomdRecord * RepomdRecord;

/** \ingroup misc
 * Internal representation of RepomdRecord object
 */
struct _RepomdRecord {
    const char *location_href;  /*!< location of the file */
    char *checksum;             /*!< checksum of file */
    char *checksum_type;        /*!< checksum type */
    char *checksum_open;        /*!< checksum of uncompressed file */
    char *checksum_open_type;   /*!< checksum type of uncompressed file*/
    long timestamp;             /*!< mtime of the file */
    long size;                  /*!< size of file in bytes */
    long size_open;             /*!< size of uncompressed file in bytes */
    int db_ver;                 /*!< version of database */

    GStringChunk *chunk;        /*!< string chunk for string from this structure*/
};

/** \ingroup repomd
 * Creates (alloc) new RepomdRecord object
 * @param path                  path to the compressed file
 */
RepomdRecord new_repomdrecord(const char *path);

/** \ingroup repomd
 * Destroy RepomdRecord object
 * @param record                RepomdRecord object
 */
void free_repomdrecord(RepomdRecord record);

/** \ingroup repomd
 * Fill unfilled items in the RepomdRecord (calculate checksums,
 * get file size before/after compression, etc.).
 * Note: For groupfile you shoud use process_groupfile function.
 * @param base_path             path to repo (to directory with repodata subdir)
 * @param record                RepomdRecord object
 * @param checksum_type         type of checksum to use
 */
int fill_missing_data(const char *base_path, RepomdRecord record, ChecksumType *checksum_type);

/** \ingroup repomd
 * Analogue of fill_missing_data but for groupfile.
 * Groupfile must be set with the path to existing non compressed groupfile.
 * Compressed group file will be created and compressed_groupfile record updated.
 * @param base_path             path to repo (to directory with repodata subdir)
 * @groupfile                   RepomdRecord initialized to an existing groupfile
 * @compressed_groupfile        a RepomdRecord object that will by filled
 * @checksum_type               type of checksums
 * @compression                 type of compression
 */
void process_groupfile(const char *base_path, RepomdRecord groupfile,
                       RepomdRecord compressed_groupfile, ChecksumType *checksum_type,
                       CompressionType compression);

/** \ingroup repomd
 * Add a hash as prefix to the filename.
 * @base_path                   path to repo (to directory with repodata subdir)
 * @record                      RepomdRecord of file to be renamed
 */
void rename_file(const char *base_path, RepomdRecord record);

/** \ingroup repomd
 * Generate repomd.xml content.
 * @param path                  path to repository (to the directory contains repodata/ subdir)
 * @param pri_xml               RepomdRecord of primary.xml.gz (relative towards to the path param)
 * @param fil_xml               RepomdRecord of filelists.xml.gz (relative towards to the path param)
 * @param oth_xml               RepomdRecord of other.xml.gz (relative towards to the path param)
 * @param pri_sqlite            RepomdRecord of primary.sqlite.* (relative towards to the path param)
 * @param fil_sqlite            RepomdRecord of filelists.sqlite.* (relative towards to the path param)
 * @param oth_sqlite            RepomdRecord of other.sqlite.* (relative towards to the path param)
 * @param groupfile             RepomdRecord of *.xml (relative towards to the path param)
 * @param cgroupfile            RepomdRecord of *.xml.* (relative towards to the path param)
 * @param update_info           RepomdRecord of updateinfo.xml.* (relative towards to the path param)
 * @return                      string with repomd.xml content
 */
gchar * xml_repomd(const char *path, RepomdRecord pri_xml,
                   RepomdRecord fil_xml, RepomdRecord oth_xml,
                   RepomdRecord pri_sqlite, RepomdRecord fil_sqlite,
                   RepomdRecord oth_sqlite, RepomdRecord groupfile,
                   RepomdRecord cgroupfile, RepomdRecord update_info);

#endif /* __C_CREATEREPOLIB_REPOMD_H__ */
