/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
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

#ifndef __C_CREATEREPOLIB_COMPRESSION_WRAPPER_H__
#define __C_CREATEREPOLIB_COMPRESSION_WRAPPER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "checksum.h"

/** \defgroup   compression_wrapper     Wrapper for compressed file.
 *  \addtogroup compression_wrapper
 *  @{
 */

/** Compression type.
 */
typedef enum {
    CR_CW_AUTO_DETECT_COMPRESSION,    /*!< Autodetection */
    CR_CW_UNKNOWN_COMPRESSION,        /*!< Unknown compression */
    CR_CW_NO_COMPRESSION,             /*!< No compression */
    CR_CW_GZ_COMPRESSION,             /*!< Gzip compression */
    CR_CW_BZ2_COMPRESSION,            /*!< BZip2 compression */
    CR_CW_XZ_COMPRESSION,             /*!< XZ compression */
    CR_CW_COMPRESSION_SENTINEL,       /*!< Sentinel of the list */
} cr_CompressionType;

/** Open modes.
 */
typedef enum {
    CR_CW_MODE_READ,            /*!< Read mode */
    CR_CW_MODE_WRITE,           /*!< Write mode */
    CR_CW_MODE_SENTINEL,        /*!< Sentinel of the list */
} cr_OpenMode;

/** Stat build about open content during compression (writting).
 */
typedef struct {
    gint64          size;       /*!< Size of content */
    cr_ChecksumCtx  *checksum;  /*!< Checksum context */
} cr_ContentStat;

/** Creates new cr_ContentStat object
 * @param type      Type of checksum. (if CR_CHECKSUM_UNKNOWN is used,
 *                  no checksum calculation will be done)
 * @param err       GError **
 * @return          cr_ContentStat object
 */
cr_ContentStat *cr_contentstat_new(cr_ChecksumType type, GError **err);

/** Frees cr_ContentStat object.
 * @param cstat     cr_ContentStat object
 * @param err       GError **
 * @return          checksum or NULL on error or if checksum calculation
 *                  was disabled (CR_CHECKSUM_UNKNOWN was used)
 */
char *cr_contentstat_free(cr_ContentStat *cstat, GError **err);

/** Structure represents a compressed file.
 */
typedef struct {
    cr_CompressionType  type;   /*!< Type of compression */
    void                *FILE;  /*!< Pointer to gzFile, BZFILE or plain FILE */
    cr_OpenMode         mode;   /*!< Mode */
    cr_ContentStat      *stat;  /*!< Content stats */
} CR_FILE;

#define CR_CW_ERR       -1      /*!< Return value - Error */

/** Returns a common suffix for the specified cr_CompressionType.
 * @param comtype       compression type
 * @return              common file suffix
 */
const char *cr_compression_suffix(cr_CompressionType comtype);

/** Detect a compression type of the specified file.
 * @param filename      filename
 * @param err           GError **
 * @return              detected type of compression
 */
cr_CompressionType cr_detect_compression(const char* filename, GError **err);

/** Open/Create the specified file.
 * @param FILENAME      filename
 * @param MODE          open mode
 * @param COMTYPE       type of compression
 * @param ERR           GError **
 * @return              pointer to a CR_FILE or NULL
 */
#define cr_open(FILENAME, MODE, COMTYPE, ERR) \
                    cr_open_with_stat(FILENAME, MODE, COMTYPE, NULL, ERR)

/** Open/Create the specified file. For writting is possible pass
 * a cr_ContentStat object and after cr_close() get stats of
 * an open content (stats of uncompressed content).
 * @param filename      filename
 * @param mode          open mode
 * @param comtype       type of compression
 * @param stat          cr_ContentStat object or NULL
 * @param err           GError **
 * @return              pointer to a CR_FILE or NULL
 */
CR_FILE *cr_open_with_stat(const char *filename,
                            cr_OpenMode mode,
                            cr_CompressionType comtype,
                            cr_ContentStat *stat,
                            GError **err);

/** Reads an array of len bytes from the CR_FILE.
 * @param cr_file       CR_FILE pointer
 * @param buffer        target buffer
 * @param len           number of bytes to read
 * @param err           GError **
 * @return              number of readed bytes or CR_CW_ERR
 */
int cr_read(CR_FILE *cr_file, void *buffer, unsigned int len, GError **err);

/** Writes the array of len bytes from buffer to the cr_file.
 * @param cr_file       CR_FILE pointer
 * @param buffer        source buffer
 * @param len           number of bytes to read
 * @param err           GError **
 * @return              number of uncompressed bytes readed (0 = EOF)
 *                      or CR_CW_ERR
 */
int cr_write(CR_FILE *cr_file,
             const void *buffer,
             unsigned int len,
             GError **err);

/** Writes the string pointed by str into the cr_file.
 * @param cr_file       CR_FILE pointer
 * @param str           null terminated ('\0') string
 * @param err           GError **
 * @return              number of uncompressed bytes writed or CR_CW_ERR
 */
int cr_puts(CR_FILE *cr_file, const char *str, GError **err);

/** Writes a formatted string into the cr_file.
 * @param err           GError **
 * @param cr_file       CR_FILE pointer
 * @param format        format string
 * @param ...           list of additional arguments as specified in format
 * @return              Number of bytes written or CR_CW_ERR (-1)
 */
int cr_printf(GError **err, CR_FILE *cr_file, const char *format, ...);

/** Closes the CR_FILE.
 * @param cr_file       CR_FILE pointer
 * @param err           GError **
 * @return              cr_Error code
 */
int cr_close(CR_FILE *cr_file, GError **err);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_COMPRESSION_WRAPPER_H__ */
