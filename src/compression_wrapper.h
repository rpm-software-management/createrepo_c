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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __C_CREATEREPOLIB_COMPRESSION_WRAPPER_H__
#define __C_CREATEREPOLIB_COMPRESSION_WRAPPER_H__

/** \defgroup   compression_wrapper     Wrapper for compressed file.
 */

/** \ingroup compression_wrapper
 * Compression type.
 */
typedef enum {
    AUTO_DETECT_COMPRESSION,    /*!< Autodetection */
    UNKNOWN_COMPRESSION,        /*!< Unknown compression */
    NO_COMPRESSION,             /*!< No compression */
    GZ_COMPRESSION,             /*!< Gzip compression */
    BZ2_COMPRESSION,            /*!< BZip2 compression */
    XZ_COMPRESSION,             /*!< XZ compression */
} CompressionType;

/** \ingroup compression_wrapper
 * Open modes.
 */
typedef enum {
    CW_MODE_READ,               /*!< Read mode */
    CW_MODE_WRITE               /*!< Write mode */
} OpenMode;


/** \ingroup compression_wrapper
 * Structure represents a compressed file.
 */
typedef struct {
    CompressionType type;       /*!< Type of compression */
    void *FILE;                 /*!< Pointer to gzFile, BZFILE or plain FILE */
    OpenMode mode;              /*!< Mode */
} CW_FILE;

/**@{*/
#define CW_OK   0       /*!< Return value - Everything all right */
#define CW_ERR -1       /*!< Return value - Error */
/**@}*/


/** \ingroup compression_wrapper
 * Returns a common suffix for the specified CompressionType.
 * @param comtype       compression type
 * @return              common file suffix
 */
const char *get_suffix(CompressionType comtype);

/** \ingroup compression_wrapper
 * Detect a compression type of the specified file.
 * @param filename      filename
 * @return              detected type of compression
 */
CompressionType detect_compression(const char* filename);

/** \ingroup compression_wrapper
 * Open/Create the specified file.
 * @param filename      filename
 * @param mode          open mode
 * @param comtype       type of compression
 * @return              pointer to a CW_FILE or NULL
 */
CW_FILE *cw_open(const char *filename, OpenMode mode, CompressionType comtype);

/** \ingroup compression_wrapper
 * Reads an array of len bytes from the cw_file.
 * @param cw_file       CW_FILE pointer
 * @param buffer        target buffer
 * @param len           number of bytes to read
 * @return              CW_OK or CW_ERR
 */
int cw_read(CW_FILE *cw_file, void *buffer, unsigned int len);

/** \ingroup compression_wrapper
 * Writes the array of len bytes from buffer to the cw_file.
 * @param cw_file       CW_FILE pointer
 * @param buffer        source buffer
 * @param len           number of bytes to read
 * @return              number of uncompressed bytes readed (0 for EOF) or CW_ERR
 */
int cw_write(CW_FILE *cw_file, const void *buffer, unsigned int len);

/** \ingroup compression_wrapper
 * Writes the string pointed by str into the cw_file.
 * @param cw_file       CW_FILE pointer
 * @param str           null terminated ('\0') string
 * @return              number of uncompressed bytes writed or CW_ERR
 */
int cw_puts(CW_FILE *cw_file, const char *str);

/** \ingroup compression_wrapper
 * Writes a formatted string into the cw_file.
 * @param cw_file       CW_FILE pointer
 * @param format        format string
 * @param ...           list of additional arguments as specified in format
 * @return              CW_OK or CW_ERR
 */
int cw_printf(CW_FILE *cw_file, const char *format, ...);

/** \ingroup compression_wrapper
 * Closes the cw_file.
 * @param cw_file       CW_FILE pointer
 * @return              CW_OK or CW_ERR
 */
int cw_close(CW_FILE *cw_file);


#endif /* __C_CREATEREPOLIB_COMPRESSION_WRAPPER_H__ */
