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

#ifndef __C_CREATEREPOLIB_CHECKSUM_H__
#define __C_CREATEREPOLIB_CHECKSUM_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup   checksum        API for checksum calculation.
 *  \addtogroup checksum
 *  @{
 */

/** Checksum context.
 */
typedef struct _cr_ChecksumCtx cr_ChecksumCtx;

/**
 * Enum of supported checksum types.
 * Note: SHA is just a "nickname" for the SHA1. This
 * is for the compatibility with original createrepo.
 */
typedef enum {
    CR_CHECKSUM_UNKNOWN,    /*!< Unknown checksum */
//    CR_CHECKSUM_MD2,        /*!< MD2 checksum */
    CR_CHECKSUM_MD5,        /*!< MD5 checksum */
    CR_CHECKSUM_SHA,        /*!< SHA checksum */
    CR_CHECKSUM_SHA1,       /*!< SHA1 checksum */
    CR_CHECKSUM_SHA224,     /*!< SHA224 checksum */
    CR_CHECKSUM_SHA256,     /*!< SHA256 checksum */
    CR_CHECKSUM_SHA384,     /*!< SHA384 checksum */
    CR_CHECKSUM_SHA512,     /*!< SHA512 checksum */
    CR_CHECKSUM_SENTINEL,   /*!< sentinel of the list */
} cr_ChecksumType;

/** Return checksum name.
 * @param type          checksum type
 * @return              constant null terminated string with checksum name
 *                      or NULL on error
 */
const char *cr_checksum_name_str(cr_ChecksumType type);

/** Return checksum type.
 * @param name          checksum name
 * @return              checksum type
 */
cr_ChecksumType cr_checksum_type(const char *name);

/** Compute file checksum.
 * @param filename      filename
 * @param type          type of checksum
 * @param err           GError **
 * @return              malloced null terminated string with checksum
 *                      or NULL on error
 */
char *cr_checksum_file(const char *filename,
                       cr_ChecksumType type,
                       GError **err);

/** Create new checksum context.
 * @param type      Checksum algorithm of the new checksum context.
 * @param err       GError **
 * @return          cr_ChecksumCtx or NULL on error
 */
cr_ChecksumCtx *cr_checksum_new(cr_ChecksumType type, GError **err);

/** Feeds data into the checksum.
 * @param ctx       Checksum context.
 * @param buf       Pointer to the data.
 * @param len       Length of the data.
 * @param err       GError **
 * @return          cr_Error code.
 */
int cr_checksum_update(cr_ChecksumCtx *ctx,
                       const void *buf,
                       size_t len,
                       GError **err);

/** Finalize checksum calculation, return checksum string and frees
 * all checksum context resources.
 * @param ctx       Checksum context.
 * @param err       GError **
 * @return          Checksum string or NULL on error.
 */
char *cr_checksum_final(cr_ChecksumCtx *ctx, GError **err);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_XML_PARSER_H__ */
