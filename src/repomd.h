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
#include "package.h"

/** \defgroup repomd            Repomd API.
 */


/** \ingroup repomd
 * Structure representing repomd.xml.
 */
struct repomdResult {
    char *pri_xml_location;             /*!< location of primary.xml */
    char *fil_xml_location;             /*!< location of filelists.xml */
    char *oth_xml_location;             /*!< location of other.xml */
    char *pri_sqlite_location;          /*!< location of primary.sqlite */
    char *fil_sqlite_location;          /*!< location of filelists.sqlite */
    char *oth_sqlite_location;          /*!< location of other.sqlite */
    char *groupfile_location;           /*!< location of groupfile */
    char *cgroupfile_location;          /*!< location of compressed groupfile */
    char *update_info_location;         /*!< location of updateinfo.xml */
    char *repomd_xml;                   /*!< xml representation of repomd_xml */
};

/** \ingroup repomd
 * Free repomdResult.
 * @param repomdResult          struct repomdReult
 */
void free_repomdresult(struct repomdResult *);

/** \ingroup repomd
 * Generate repomd.xml content and rename files (if rename_to_unique != 0).
 * @param path                  path to repository (to the directory contains repodata/ subdir)
 * @param rename_to_unique      rename files? - insert checksum into the metadata file names (=0 do not insert, !=0 insert)
 * @param pri_xml               location of compressed primary.xml (relative towards to the path)
 * @param fil_xml               location of compressed filelists.xml (relative towards to the path)
 * @param oth_xml               location of compressed other.xml (relative towards to the path)
 * @param pri_sqlite            location of compressed primary.sqlite (relative towards to the path)
 * @param fil_sqlite            location of compressed filelists.sqlite (relative towards to the path)
 * @param oth_sqlite            location of compressed other.sqlite (relative towards to the path)
 * @param groupfile             location of groupfile (non compressed!)
 * @param update_info           location of compressed update_info
 * @param checksum_type         type of checksum
 * @return                      repomdResult
 */
struct repomdResult *xml_repomd(const char *path, int rename_to_unique, const char *pri_xml,
                                const char *fil_xml, const char *oth_xml,
                                const char *pri_sqlite, const char *fil_sqlite,
                                const char *oth_sqlite, const char *groupfile,
                                const char *update_info, ChecksumType *checksum_type);

#endif /* __C_CREATEREPOLIB_REPOMD_H__ */
