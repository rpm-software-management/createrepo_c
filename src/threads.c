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
#include "dumper_thread.h"

#define ERR_DOMAIN      CREATEREPO_C_ERROR


/** Parallel Compression */

cr_CompressionTask *
cr_compressiontask_new(const char *src,
                       const char *dst,
                       cr_CompressionType compression_type,
                       cr_ChecksumType checksum_type,
                       const char *zck_dict_dir,
                       gboolean zck_auto_chunk,
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
        g_set_error(err, ERR_DOMAIN, CRE_MEMORY,
                   "Cannot allocate memory");
        cr_contentstat_free(stat, NULL);
        return NULL;
    }

    task->src    = g_strdup(src);
    task->dst    = g_strdup(dst);
    task->type   = compression_type;
    task->stat   = stat;
    if (zck_dict_dir != NULL)
        task->zck_dict_dir = g_strdup(zck_dict_dir);
    task->zck_auto_chunk = zck_auto_chunk;
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
    if (task->zck_dict_dir)
        g_free(task->zck_dict_dir);
    g_free(task);
}

void
cr_compressing_thread(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
    cr_CompressionTask *task = data;
    GError *tmp_err = NULL;

    assert(task);

    if (!task->dst)
        task->dst = g_strconcat(task->src,
                                cr_compression_suffix(task->type),
                                NULL);

    cr_compress_file_with_stat(task->src,
                               task->dst,
                               task->type,
                               task->stat,
                               task->zck_dict_dir,
                               task->zck_auto_chunk,
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

void
cr_rewrite_pkg_count_thread(gpointer data, gpointer user_data)
{
    cr_CompressionTask *task = data;
    struct UserData *ud = user_data;
    GError *tmp_err = NULL;

    assert(task);

    cr_rewrite_header_package_count(task->src,
                                    task->type,
                                    ud->package_count,
                                    ud->task_count,
                                    task->stat,
                                    task->zck_dict_dir,
                                    &tmp_err);

    if (tmp_err) {
        // Error encountered
        g_propagate_error(&task->err, tmp_err);
    }
}

/** Parallel Repomd Record Fill */

cr_RepomdRecordFillTask *
cr_repomdrecordfilltask_new(cr_RepomdRecord *record,
                            cr_ChecksumType checksum_type,
                            GError **err)
{
    cr_RepomdRecordFillTask *task;

    assert(record);
    assert(!err || *err == NULL);

    task = g_malloc0(sizeof(cr_RepomdRecord));
    task->record = record;
    task->checksum_type = checksum_type;

    return task;
}

void
cr_repomdrecordfilltask_free(cr_RepomdRecordFillTask *task,
                             GError **err)
{
    assert(!err || *err == NULL);

    if (task->err)
        g_error_free(task->err);
    g_free(task);
}

void
cr_repomd_record_fill_thread(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
    cr_RepomdRecordFillTask *task = data;
    GError *tmp_err = NULL;

    assert(task);

    cr_repomd_record_fill(task->record, task->checksum_type, &tmp_err);

    if (tmp_err) {
        // Error encountered
        g_propagate_error(&task->err, tmp_err);
    }
}
