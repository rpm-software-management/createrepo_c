/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2014  Tomas Mlcoch
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

#ifndef __C_CREATEREPOLIB_HELPERS_H__
#define __C_CREATEREPOLIB_HELPERS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include "checksum.h"
#include "compression_wrapper.h"
#include "package.h"

/** \defgroup   helpers   Helpers for createrepo_c, modifyrepo_c, mergerepo_c
 *
 * Module with helpers for createrepo_c, modifyrepo_c, mergerepo_c
 *
 *  \addtogroup helpers
 *  @{
 */

typedef enum {
    CR_RETENTION_DEFAULT,
    CR_RETENTION_COMPATIBILITY,
    CR_RETENTION_BYAGE,
} cr_RetentionType;

gboolean
cr_old_metadata_retention(const char *old_repo,
                          const char *new_repo,
                          cr_RetentionType type,
                          gint64 val,
                          GError **err);

/** Remove files related to repodata from the specified path.
 * Files not listed in repomd.xml and with nonstandard names (standard names
 * are names with suffixes like primary.xml.*, primary.sqlite.*, other.xml.*,
 * etc.) are keep untouched (repodata/ subdirectory IS NOT removed!).
 * @param repopath      path to directory with repodata/ subdirectory
 * @param err           GError **
 * @return              number of removed files
 */
int cr_remove_metadata(const char *repopath, GError **err);

/** Remove repodata in same manner as classic createrepo.
 * This function removes only (primary|filelists|other)[.sqlite].* files
 * from repodata.
 * @param repopath      path to directory with repodata/subdirectory
 * @param retain        keep around the latest N old, uniquely named primary,
 *                      filelists and otherdata xml and sqlite files.
 *                      If <1 no old files will be kept.
 * @param err           GError **
 * @return              cr_Error code
 */
int cr_remove_metadata_classic(const char *repopath, int retain, GError **err);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_HELPERS__ */
