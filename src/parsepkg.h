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

#ifndef __C_CREATEREPOLIB_PARSEPKG_H__
#define __C_CREATEREPOLIB_PARSEPKG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include "checksum.h"
#include "package.h"

/** \defgroup   parsepkg    Package parser API.
 *  \addtogroup parsepkg
 *  @{
 */

/** Status of initialization of global structures for package parsing.
 */
extern short cr_initialized;

/** Initialize global structures for package parsing.
 * This function call rpmReadConfigFiles() and create global transaction set.
 * This function should be called only once! This function is not thread safe!
 */
void cr_package_parser_init();

/** Free global structures for package parsing.
 */
void cr_package_parser_cleanup();

/** Generate package object from package file.
 * @param filename              filename
 * @param checksum_type         type of checksum to be used
 * @param location_href         package location inside repository
 * @param location_base         location (url) of repository
 * @param changelog_limit       number of changelog entries
 * @param stat_buf              struct stat of the filename
 *                              (optional - could be NULL)
 * @param err                   GError **
 * @return                      cr_Package
 */
cr_Package *cr_package_from_rpm(const char *filename,
                                cr_ChecksumType checksum_type,
                                const char *location_href,
                                const char *location_base,
                                int changelog_limit,
                                struct stat *stat_buf,
                                GError **err);

/** Generate XML for the specified package.
 * @param filename              rpm filename
 * @param checksum_type         type of checksum to be used
 * @param location_href         package location inside repository
 * @param location_base         location (url) of repository
 * @param changelog_limit       number of changelog entries
 * @param stat_buf              struct stat of the filename
 *                              (optional - could be NULL)
 * @param err                   GError **
 * @return                      struct cr_XmlStruct with primary, filelists and
 *                              other xmls
 */
struct cr_XmlStruct cr_xml_from_rpm(const char *filename,
                                    cr_ChecksumType checksum_type,
                                    const char *location_href,
                                    const char *location_base,
                                    int changelog_limit,
                                    struct stat *stat_buf,
                                    GError **err);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_PARSEPKG_H__ */
