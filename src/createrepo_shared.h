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

#ifndef __C_CREATEREPOLIB_CREATEREPO_SHARED_H__
#define __C_CREATEREPOLIB_CREATEREPO_SHARED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include "checksum.h"
#include "compression_wrapper.h"
#include "package.h"

/** \defgroup   createrepo_shared   Createrepo API.
 *
 * Module with createrepo API
 *
 *  \addtogroup createrepo_shared
 *  @{
 */

/**
 * Block process terminating signals.
 * (Useful for creating pseudo-atomic sections in code)
 */
gboolean
cr_block_terminating_signals(GError **err);

/**
 * Unblock process terminating signals.
 */
gboolean
cr_unblock_terminating_signals(GError **err);


/**
 * This function does:
 * - Tries to create repo/.repodata/ dir.
 * - If it doesn't exists, it's created and function returns TRUE.
 * - If it exists and ignore_lock is FALSE, returns FALSE and err is set.
 * - If it exists and ignore_lock is TRUE it:
 *  - Removes the existing .repodata/ dir and all its content
 *  - Creates (empty) new one (just as a lock dir - place holder)
 *  - Creates .repodata.pid.datetime.usec/ that should be used for
 *    repodata generation
 *
 * @param repo_dir          Path to repo (a dir that contains repodata/ subdir)
 * @param ignore_lock       Ignore existing .repodata/ dir - remove it and
 *                          create a new one.
 * @param lock_dir          Location to store path to a directory used as
 *                          a lock. Always repodir+"/.repodata/".
 *                          Even if FALSE is returned, the content of this
 *                          variable IS DEFINED.
 * @param tmp_repodata_dir  Location to store a path to a directory used as
 *                          a temporary directory for repodata generation.
 *                          If ignore_lock is FALSE than
 *                          lock_dir is same as tmp_repodata_dir.
 *                          If FALSE is returned, the content of this variable
 *                          is undefined.
 * @param err               GError **
 * @return                  FALSE if err is set, TRUE otherwise.
 */
gboolean
cr_lock_repo(const gchar *repo_dir,
             gboolean ignore_lock,
             gchar **lock_dir,
             gchar **tmp_repodata_dir,
             GError **err);
// TODO XXX

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_CREATEREPO_SHARED__ */
