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

#ifndef __C_CREATEREPOLIB_COMPRESSION_WRAPPER_H__
#define __C_CREATEREPOLIB_COMPRESSION_WRAPPER_H__

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup   compression_wrapper     Wrapper for compressed file.
 */

/** \ingroup compression_wrapper
 * Compression type.
 */
typedef enum {
    CR_CW_AUTO_DETECT_COMPRESSION,    /*!< Autodetection */
    CR_CW_UNKNOWN_COMPRESSION,        /*!< Unknown compression */
    CR_CW_NO_COMPRESSION,             /*!< No compression */
    CR_CW_GZ_COMPRESSION,             /*!< Gzip compression */
    CR_CW_BZ2_COMPRESSION,            /*!< BZip2 compression */
    CR_CW_XZ_COMPRESSION,             /*!< XZ compression */
} cr_CompressionType;

/** \ingroup compression_wrapper
 * Open modes.
 */
typedef enum {
    CR_CW_MODE_READ,               /*!< Read mode */
    CR_CW_MODE_WRITE               /*!< Write mode */
} cr_OpenMode;


/** \ingroup compression_wrapper
 * Structure represents a compressed file.
 */
typedef struct {
    cr_CompressionType type;    /*!< Type of compression */
    void *FILE;                 /*!< Pointer to gzFile, BZFILE or plain FILE */
    cr_OpenMode mode;           /*!< Mode */
} CR_FILE;

/**@{*/
#define CR_CW_OK   0       /*!< Return value - Everything all right */
#define CR_CW_ERR -1       /*!< Return value - Error */
/**@}*/


/** \ingroup compression_wrapper
 * Returns a common suffix for the specified cr_CompressionType.
 * @param comtype       compression type
 * @return              common file suffix
 */
const char *cr_get_suffix(cr_CompressionType comtype);

/** \ingroup compression_wrapper
 * Detect a compression type of the specified file.
 * @param filename      filename
 * @return              detected type of compression
 */
cr_CompressionType cr_detect_compression(const char* filename);

/** \ingroup compression_wrapper
 * Open/Create the specified file.
 * @param filename      filename
 * @param mode          open mode
 * @param comtype       type of compression
 * @return              pointer to a CR_FILE or NULL
 */
CR_FILE *cr_open(const char *filename, cr_OpenMode mode, cr_CompressionType comtype);

/** \ingroup compression_wrapper
 * Reads an array of len bytes from the CR_FILE.
 * @param CR_FILE       CR_FILE pointer
 * @param buffer        target buffer
 * @param len           number of bytes to read
 * @return              CR_CW_OK or CR_CW_ERR
 */
int cr_read(CR_FILE *cr_file, void *buffer, unsigned int len);

/** \ingroup compression_wrapper
 * Writes the array of len bytes from buffer to the cr_file.
 * @param CR_FILE       CR_FILE pointer
 * @param buffer        source buffer
 * @param len           number of bytes to read
 * @return              number of uncompressed bytes readed (0 = EOF) or CR_CW_ERR
 */
int cr_write(CR_FILE *cr_file, const void *buffer, unsigned int len);

/** \ingroup compression_wrapper
 * Writes the string pointed by str into the cr_file.
 * @param CR_FILE       CR_FILE pointer
 * @param str           null terminated ('\0') string
 * @return              number of uncompressed bytes writed or CR_CW_ERR
 */
int cr_puts(CR_FILE *cr_file, const char *str);

/** \ingroup compression_wrapper
 * Writes a formatted string into the cr_file.
 * @param CR_FILE       CR_FILE pointer
 * @param format        format string
 * @param ...           list of additional arguments as specified in format
 * @return              CR_CW_OK or CR_CW_ERR
 */
int cr_printf(CR_FILE *cr_file, const char *format, ...);

/** \ingroup compression_wrapper
 * Closes the CR_FILE.
 * @param CR_FILE       CR_FILE pointer
 * @return              CR_CW_OK or CR_CW_ERR
 */
int cr_close(CR_FILE *cr_file);

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_COMPRESSION_WRAPPER_H__ */
