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

#ifndef __C_CREATEREPOLIB_PARSEPKG_H__
#define __C_CREATEREPOLIB_PARSEPKG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <rpm/rpmlib.h>
#include <glib.h>
#include "constants.h"
#include "package.h"
#include "xml_dump.h"

/** \defgroup parsepkg  Package parser API.
 */

/** \ingroup parsepkg
 * Status of initialization of global structures for package parsing.
 */
extern short cr_initialized;

/** \ingroup parsepkg
 * Initialize global structures for package parsing.
 * This function call rpmReadConfigFiles() and create global transaction set.
 * This function should be called only once! This function is not thread safe!
 */
void cr_package_parser_init();

/** \ingroup parsepkg
 * Free global structures for package parsing.
 */
void cr_package_parser_shutdown();

/** \ingroup parsepkg
 * Generate package object from package file.
 * @param filename              filename
 * @param checksum_type         type of checksum to be used
 * @param location_href         package location inside repository
 * @param location_base         location (url) of repository
 * @param changelog_limit       number of changelog entries
 * @param stat_buf              struct stat of the filename
 *                              (optional - could be NULL)
 */
cr_Package *cr_package_from_file(const char *filename,
                                 cr_ChecksumType checksum_type,
                                 const char *location_href,
                                 const char *location_base,
                                 int changelog_limit,
                                 struct stat *stat_buf);

/** \ingroup parsepkg
 * Generate XML for the specified package.
 * @param filename              filename
 * @param checksum_type         type of checksum to be used
 * @param location_href         package location inside repository
 * @param location_base         location (url) of repository
 * @param changelog_limit       number of changelog entries
 * @param stat_buf              struct stat of the filename
 *                              (optional - could be NULL)
 */
struct cr_XmlStruct cr_xml_from_package_file(const char *filename,
                                             cr_ChecksumType checksum_type,
                                             const char *location_href,
                                             const char *location_base,
                                             int changelog_limit,
                                             struct stat *stat_buf);

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_PARSEPKG_H__ */
