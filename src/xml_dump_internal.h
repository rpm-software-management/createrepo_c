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

#ifndef __C_CREATEREPOLIB_XML_DUMP_PRIVATE_H__
#define __C_CREATEREPOLIB_XML_DUMP_PRIVATE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "package.h"

/**
 * Dump files from the package and append them to the node as childrens.
 * @param node          parent xml node
 * @param package       cr_Package
 * @param primary       process only primary files (see cr_is_primary() function
 *                      in the misc module)
 */
void cr_dump_files(xmlNodePtr node, cr_Package *package, int primary);

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_XML_DUMP_PRIVATE_H__ */
