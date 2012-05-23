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

#ifndef __C_CREATEREPOLIB_PARSEPKG_H__
#define __C_CREATEREPOLIB_PARSEPKG_H__

#include <rpm/rpmlib.h>
#include <glib.h>
#include "constants.h"
#include "package.h"
#include "xml_dump.h"

/*
Package *parse_package(Header hdr, gint64 mtime, gint64 size, const char *checksum, const char *checksum_type,
                      const char *location_href, const char *location_base,
                      int changelog_limit, gint64 hdr_start, gint64 hdr_end);
*/

extern short initialized;
void init_package_parser();
void free_package_parser();


// TODO: swig map for struct stat stat_buf

// stat_buf can be NULL
struct XmlStruct xml_from_package_file(const char *filename, ChecksumType checksum_type,
                        const char *location_href, const char *location_base,
                        int changelog_limit, struct stat *stat_buf);

#endif /* __C_CREATEREPOLIB_PARSEPKG_H__ */
