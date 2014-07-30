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

#ifndef __C_CREATEREPOLIB_PARSEHDR_H__
#define __C_CREATEREPOLIB_PARSEHDR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <rpm/rpmlib.h>
#include "package.h"

/** \defgroup   parsehdr         Header parser API.
 *  \addtogroup parsehdr
 *  @{
 */

/** Flags
 */
typedef enum {
    CR_HDRR_NONE            = (1 << 0),
    CR_HDRR_LOADHDRID       = (1 << 1), /*!< Load hdrid */
    CR_HDRR_LOADSIGNATURES  = (1 << 2), /*!< Load siggpg and siggpg */
} cr_HeaderReadingFlags;

/** Read data from header and return filled cr_Package structure.
 * All const char * params could be NULL.
 * @param hdr                   Header
 * @param changelog_limit       number of changelog entries
 * @param flags                 Flags for header reading
 * @param err                   GError **
 * @return                      Newly allocated cr_Package or NULL on error
 */
cr_Package *cr_package_from_header(Header hdr,
                                   int changelog_limit,
                                   cr_HeaderReadingFlags flags,
                                   GError **err);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_PARSEHDR_H__ */
