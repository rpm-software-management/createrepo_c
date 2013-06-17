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

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup   threads     Useful thread function to use in GThreadPool.
 *
 * \addtogroup threads
 * @{
 */

/** Compression */

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
 */
cr_CompressionTask *
cr_compressiontask_new(const char *src,
                       const char *dst,
                       cr_CompressionType compression_type,
                       cr_ChecksumType checksum_type,
                       int delsrc,
                       GError **err);

/** Frees cr_CompressionTask and all its components.
 * @param task      cr_CompressionTask task
 * @param err       GError **.
 */
void
cr_compressiontask_free(cr_CompressionTask *task, GError **err);

/** Function for GThreadPool.
 */
void
cr_compressing_thread(gpointer data, gpointer user_data);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_THREADS_H__ */
