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

#ifndef __C_CREATEREPOLIB_PARSEHDR_H__
#define __C_CREATEREPOLIB_PARSEHDR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <rpm/rpmlib.h>
#include <glib.h>
#include "package.h"
#include "xml_dump.h"

/** \defgroup parsehdr         Header parser API.
 */

/** \ingroup parsehdr
 * Read data from header and return filled Package structure.
 * @param hdr                   Header
 * @param mtime                 mtime of rpm file
 * @param size                  size of rpm file (in bytes)
 * @param checksum              checksum of rpm file
 * @param checksum_type         used checksum algorithm
 * @param location_href         location of package inside repository
 * @param location_base         location (url) of repository
 * @param changelog_limit       number of changelog entries
 * @param hdr_start             start byte of header
 * @param hdr_end               last byte of header
 * @return                      Package
 */
Package *parse_header(Header hdr, gint64 mtime, gint64 size,
                        const char *checksum, const char *checksum_type,
                        const char *location_href, const char *location_base,
                        int changelog_limit, gint64 hdr_start, gint64 hdr_end);

/** \ingroup parsehdr
 * Read data from header and return struct XmlStruct.
 * @param hdr                   Header
 * @param mtime                 mtime of rpm file
 * @param size                  size of rpm file (in bytes)
 * @param checksum              checksum of rpm file
 * @param checksum_type         used checksum algorithm
 * @param location_href         location of package inside repository
 * @param location_base         location (url) of repository
 * @param changelog_limit       number of changelog entries
 * @param hdr_start             start byte of header
 * @param hdr_end               last byte of header
 * @return                      XML chunks for primary, filelists and other
 *                              (in struct XmlStruct)
 */
struct XmlStruct xml_from_header(Header hdr, gint64 mtime, gint64 size,
                        const char *checksum, const char *checksum_type,
                        const char *location_href, const char *location_base,
                        int changelog_limit, gint64 hdr_start, gint64 hdr_end);

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_PARSEHDR_H__ */
