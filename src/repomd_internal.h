/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2013  Tomas Mlcoch
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

#ifndef __C_CREATEREPOLIB_REPOMD_INTERNAL_H__
#define __C_CREATEREPOLIB_REPOMD_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "error.h"

cr_DistroTag *cr_distrotag_new();

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_REPOMD_INTERNAL_H__ */
