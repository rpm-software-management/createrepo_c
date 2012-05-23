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

#ifndef __C_CREATEREPOLIB_LOCATE_METADATA_H__
#define __C_CREATEREPOLIB_LOCATE_METADATA_H__


struct MetadataLocation {
    char *pri_xml_href;
    char *fil_xml_href;
    char *oth_xml_href;
    char *pri_sqlite_href;
    char *fil_sqlite_href;
    char *oth_sqlite_href;
    char *groupfile_href;
    char *cgroupfile_href; // compressed groupfile location
    char *repomd;
    char *original_url; // Original path of repo from commandline
    char *local_path; // local path to repo
    char *tmp_dir; // Path to dir where temporary metadata are stored (if metadata were downloaded)
};

struct MetadataLocation *get_metadata_location(const char *);
void free_metadata_location(struct MetadataLocation *);

int remove_metadata(const char *repopath);

#endif /* __C_CREATEREPOLIB_LOCATE_METADATA_H__ */
