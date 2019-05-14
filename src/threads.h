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

#ifndef __C_CREATEREPOLIB_THREADS_H__
#define __C_CREATEREPOLIB_THREADS_H__

#include <glib.h>
#include "compression_wrapper.h"
#include "checksum.h"
#include "repomd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup   threads     Useful thread function to use in GThreadPool.
 *
 * Paralelized compression example:
 * \code
 * cr_CompressionTask *task_1, *task_2;
 * GThreadPool *pool;
 *
 * // Prepare tasks
 * task_1 = cr_compressiontask_new("foo", "foo.gz", CR_CW_GZ_COMPRESSION, 1,
 *                                 CR_CHECKSUM_SHA256, NULL);
 * task_2 = cr_compressiontask_new("bar", "bar.gz", CR_CW_GZ_COMPRESSION, 1,
 *                                 CR_CHECKSUM_SHA512, NULL);
 *
 * // Create pool for tasks
 * pool = g_thread_pool_new(cr_compressing_thread, NULL, 2, FALSE, NULL);
 *
 * // Push tasks to the pool
 * g_thread_pool_push(pool, task_1, NULL);
 * g_thread_pool_push(pool, task_2, NULL);
 *
 * // Wait until both treats finish and free the pool.
 * g_thread_pool_free(pool, FALSE, TRUE);
 *
 * // Use results
 * // Do whatever you want or need to do
 *
 * // Clean up
 * cr_compressiontask_free(task_1, NULL);
 * cr_compressiontask_free(task_2, NULL);
 * \endcode
 *
 * \addtogroup threads
 * @{
 */

/** Object representing a single compression task
 */
typedef struct {
    char *src; /*!<
        Path to the original file. Must be specified by user. */
    char *dst; /*!<
        Path to the destination file. If NULL, src+compression suffix will
        be used and this will be filled.*/
    cr_CompressionType type; /*!<
        Type of compression to use */
    cr_ContentStat *stat; /*!<
        Stats of compressed file or NULL */
    char *zck_dict_dir; /*!<
        Location of zchunk dictionaries */
    gboolean zck_auto_chunk; /*!<
        Whether zchunk file should be auto-chunked */
    int delsrc; /*!<
        Indicate if delete source file after successful compression. */
    GError *err; /*!<
        If error was encountered, it will be stored here, if no, then NULL*/
} cr_CompressionTask;

/** Function to prepare a new cr_CompressionTask.
 * @param src               Source filename.
 * @param dst               Destination filename or NULL (then src+compression
 *                          suffix will be used).
 * @param compression_type  Type of compression to use.
 * @param checksum_type     Checksum type for stat calculation. Note: Stat
 *                          is always use. If you don't need a stats use
 *                          CR_CHECKSUM_UNKNOWN, then no checksum calculation
 *                          will be performed, only size would be calculated.
 *                          Don't be afraid, size calculation has almost
 *                          no overhead.
 * @param delsrc            Delete src after successuful compression.
 *                          0 = Do not delete, delete otherwise
 * @param err               GError **. Note: This is a GError for the
 *                          cr_compresiontask_new function. The GError
 *                          that will be at created cr_CompressionTask is
 *                          different.
 * @return                  New cr_CompressionTask.
 */
cr_CompressionTask *
cr_compressiontask_new(const char *src,
                       const char *dst,
                       cr_CompressionType compression_type,
                       cr_ChecksumType checksum_type,
                       const char *zck_dict_dir,
                       gboolean zck_auto_chunk,
                       int delsrc,
                       GError **err);

/** Frees cr_CompressionTask and all its components.
 * @param task      cr_CompressionTask task
 * @param err       GError **
 */
void
cr_compressiontask_free(cr_CompressionTask *task, GError **err);

/** Function for GThreadPool.
 */
void
cr_compressing_thread(gpointer data, gpointer user_data);

/** Object representing a single repomd record fill task
 */
typedef struct {
    cr_RepomdRecord *record;        /*!< Repomd record to be filled */
    cr_ChecksumType checksum_type;  /*!< Type of checksum to be used */
    GError *err;                    /*!< GError ** */
} cr_RepomdRecordFillTask;

/** Function to prepare a new cr_RepomdRecordFillTask.
 * @param record            cr_RepomdRecord.
 * @param checksum_type     Type of checksum.
 * @param err               GError **
 * @return                  New cr_RepomdRecordFillTask.
 */
cr_RepomdRecordFillTask *
cr_repomdrecordfilltask_new(cr_RepomdRecord *record,
                            cr_ChecksumType checksum_type,
                            GError **err);

/** Frees cr_RepomdRecordFillTask
 */
void
cr_repomdrecordfilltask_free(cr_RepomdRecordFillTask *task, GError **err);

/** Function for GThread Pool.
 */
void
cr_repomd_record_fill_thread(gpointer data, gpointer user_data);

/** Function for GThread Pool.
 */
void
cr_rewrite_pkg_count_thread(gpointer data, gpointer user_data);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_THREADS_H__ */
