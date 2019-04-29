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

#ifndef __C_CREATEREPOLIB_MODIFYREPO_SHARED_H__
#define __C_CREATEREPOLIB_MODIFYREPO_SHARED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include "checksum.h"
#include "compression_wrapper.h"
#include "package.h"

/** \defgroup   modifyrepo_shared   Modifyrepo API.
 *
 * Module with modifyrepo API
 *
 *  \addtogroup modifyrepo_shared
 *  @{
 */

typedef struct {

    gchar *path;
    gchar *type;
    gboolean remove;
    gboolean compress;
    cr_CompressionType compress_type;
    gboolean unique_md_filenames;
    cr_ChecksumType checksum_type;
    gchar *new_name;
    gboolean zck;
    gchar *zck_dict_dir;

    // Internal use
    gchar *repopath;
    gchar *zck_repopath;
    gchar *dst_fn;
    GStringChunk *chunk;

} cr_ModifyRepoTask;

cr_ModifyRepoTask *
cr_modifyrepotask_new(void);

void
cr_modifyrepotask_free(cr_ModifyRepoTask *task);

gchar *
cr_write_file(gchar *repopath, cr_ModifyRepoTask *task,
           cr_CompressionType compress_type, GError **err);

gboolean
cr_modifyrepo(GSList *modifyrepotasks, gchar *repopath, GError **err);

gboolean
cr_modifyrepo_parse_batchfile(const gchar *path,
                              GSList **modifyrepotasks,
                              GError **err);

gchar *
cr_remove_compression_suffix_if_present(gchar* name, GError **err);
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_MODIFYREPO_SHARED__ */
