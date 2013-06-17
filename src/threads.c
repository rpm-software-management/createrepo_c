/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2013      Tomas Mlcoch
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

#include <assert.h>
#include "threads.h"
#include "error.h"
#include "misc.h"

/** Compression */

cr_CompressionTask *
cr_compressiontask_new(const char *src,
                       const char *dst,
                       cr_CompressionType compression_type,
                       cr_ChecksumType checksum_type,
                       int delsrc,
                       GError **err)
{
    cr_ContentStat *stat;
    cr_CompressionTask *task;

    assert(src);
    assert(compression_type < CR_CW_COMPRESSION_SENTINEL);
    assert(checksum_type < CR_CHECKSUM_SENTINEL);
    assert(!err || *err == NULL);

    stat = cr_contentstat_new(checksum_type, err);
    if (!stat)
        return NULL;

    task = g_malloc0(sizeof(cr_CompressionTask));
    if (!task) {
        g_set_error(err, CR_THREADS_ERROR, CRE_MEMORY,
                   "Cannot allocate memory");
        return NULL;
    }

    task->src    = g_strdup(src);
    task->dst    = g_strdup(dst);
    task->type   = compression_type;
    task->stat   = stat;
    task->delsrc = delsrc;

    return task;
}

void
cr_compressiontask_free(cr_CompressionTask *task, GError **err)
{
    assert(!err || *err == NULL);

    if (!task)
        return;

    g_free(task->src);
    g_free(task->dst);
    cr_contentstat_free(task->stat, err);
    if (task->err)
        g_error_free(task->err);
    g_free(task);
}

void
cr_compressing_thread(gpointer data, gpointer user_data)
{
    cr_CompressionTask *task = data;
    GError *tmp_err = NULL;

    CR_UNUSED(user_data);

    assert(task);

    if (!task->dst)
        task->dst = g_strconcat(task->src,
                                cr_compression_suffix(task->type),
                                NULL);

    cr_compress_file_with_stat(task->src,
                               task->dst,
                               task->type,
                               task->stat,
                               &tmp_err);

    if (tmp_err) {
        // Error encountered
        g_propagate_error(&task->err, tmp_err);
    } else {
        // Compression was successful
        if (task->delsrc)
            remove(task->src);
    }
}
