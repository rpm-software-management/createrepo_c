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

#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <magic.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <zlib.h>
#include <bzlib.h>
#include <lzma.h>
#include "logging.h"
#include "compression_wrapper.h"

/*
#define Z_CR_CW_NO_COMPRESSION         0
#define Z_BEST_SPEED             1
#define Z_BEST_COMPRESSION       9
#define Z_DEFAULT_COMPRESSION  (-1)
*/
#define CR_CW_GZ_COMPRESSION_LEVEL    Z_DEFAULT_COMPRESSION

/*
#define Z_FILTERED            1
#define Z_HUFFMAN_ONLY        2
#define Z_RLE                 3
#define Z_FIXED               4
#define Z_DEFAULT_STRATEGY    0
*/
#define GZ_STRATEGY             Z_DEFAULT_STRATEGY
#define GZ_BUFFER_SIZE          (1024*128)

#define BZ2_VERBOSITY           0
#define BZ2_BLOCKSIZE100K       5  // Higher gives better compression but takes
                                   // more memory
#define BZ2_WORK_FACTOR         0  // 0 == default == 30 (available 0-250)
#define BZ2_USE_LESS_MEMORY     0
#define BZ2_SKIP_FFLUSH         0

/*
number 0..9
or
LZMA_PRESET_DEFAULT default preset
LZMA_PRESET_EXTREME significantly slower, improving the compression ratio
                    marginally
*/
#define CR_CW_XZ_COMPRESSION_LEVEL    5

/*
LZMA_CHECK_NONE
LZMA_CHECK_CRC32
LZMA_CHECK_CRC64
LZMA_CHECK_SHA256
*/
#define XZ_CHECK                LZMA_CHECK_CRC32

/* UINT64_MAX effectively disable the limiter */
#define XZ_MEMORY_USAGE_LIMIT   UINT64_MAX
#define XZ_DECODER_FLAGS        0
#define XZ_BUFFER_SIZE          (1024*32)

#if ZLIB_VERNUM < 0x1240
// XXX: Zlib has gzbuffer since 1.2.4
#define gzbuffer(a,b) 0
#endif


typedef struct {
    lzma_stream stream;
    FILE *file;
    unsigned char buffer[XZ_BUFFER_SIZE];
} XzFile;



cr_CompressionType
cr_detect_compression(const char *filename)
{
    cr_CompressionType type = CR_CW_UNKNOWN_COMPRESSION;

    if (!g_file_test(filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
        g_debug("%s: File %s doesn't exists or it's not a regular file",
                __func__, filename);
        return type;
    }

    // Try determine compression type via filename suffix

    if (g_str_has_suffix(filename, ".gz") ||
        g_str_has_suffix(filename, ".gzip") ||
        g_str_has_suffix(filename, ".gunzip"))
    {
        return CR_CW_GZ_COMPRESSION;
    } else if (g_str_has_suffix(filename, ".bz2") ||
               g_str_has_suffix(filename, ".bzip2"))
    {
        return CR_CW_BZ2_COMPRESSION;
    } else if (g_str_has_suffix(filename, ".xz"))
    {
        return CR_CW_XZ_COMPRESSION;
    } else if (g_str_has_suffix(filename, ".xml"))
    {
        return CR_CW_NO_COMPRESSION;
    }


    // No success? Let's get hardcore... (Use magic bytes)

    //magic_t myt = magic_open(MAGIC_MIME_TYPE);
    magic_t myt = magic_open(MAGIC_MIME);
    magic_load(myt, NULL);
    if (magic_check(myt, NULL) == -1) {
        g_critical("%s: magic_check() failed", __func__);
        return type;
    }

    const char *mime_type = magic_file(myt, filename);

    if (mime_type) {
        g_debug("%s: Detected mime type: %s (%s)", __func__, mime_type,
                filename);

        if (g_str_has_prefix(mime_type, "application/x-gzip") ||
            g_str_has_prefix(mime_type, "application/gzip") ||
            g_str_has_prefix(mime_type, "application/gzip-compressed") ||
            g_str_has_prefix(mime_type, "application/gzipped") ||
            g_str_has_prefix(mime_type, "application/x-gzip-compressed") ||
            g_str_has_prefix(mime_type, "application/x-compress") ||
            g_str_has_prefix(mime_type, "application/x-gzip") ||
            g_str_has_prefix(mime_type, "application/x-gunzip") ||
            g_str_has_prefix(mime_type, "multipart/x-gzip"))
        {
            type = CR_CW_GZ_COMPRESSION;
        }

        else if (g_str_has_prefix(mime_type, "application/x-bzip2") ||
                 g_str_has_prefix(mime_type, "application/x-bz2") ||
                 g_str_has_prefix(mime_type, "application/bzip2") ||
                 g_str_has_prefix(mime_type, "application/bz2"))
        {
            type = CR_CW_BZ2_COMPRESSION;
        }

        else if (g_str_has_prefix(mime_type, "application/x-xz"))
        {
            type = CR_CW_XZ_COMPRESSION;
        }

        else if (g_str_has_prefix(mime_type, "text/plain") ||
                 g_str_has_prefix(mime_type, "text/xml") ||
                 g_str_has_prefix(mime_type, "application/xml") ||
                 g_str_has_prefix(mime_type, "application/x-xml") ||
                 g_str_has_prefix(mime_type, "application/x-empty") ||
                 g_str_has_prefix(mime_type, "inode/x-empty"))
        {
            type = CR_CW_NO_COMPRESSION;
        }
    } else {
        g_debug("%s: Mime type not detected! (%s)", __func__, filename);
    }


    // Xml detection

    if (type == CR_CW_UNKNOWN_COMPRESSION && g_str_has_suffix(filename, ".xml"))
    {
        type = CR_CW_NO_COMPRESSION;
    }

    magic_close(myt);

    return type;
}


const char *
cr_compression_suffix(cr_CompressionType comtype)
{
    char *suffix = NULL;
    switch (comtype) {
        case CR_CW_GZ_COMPRESSION:
            suffix = ".gz"; break;
        case CR_CW_BZ2_COMPRESSION:
            suffix = ".bz2"; break;
        case CR_CW_XZ_COMPRESSION:
            suffix = ".xz"; break;
        default:
            break;
    }

    return suffix;
}


CR_FILE *
cr_open(const char *filename, cr_OpenMode mode, cr_CompressionType comtype)
{
    CR_FILE *file = NULL;
    cr_CompressionType type;

    if (!filename || (mode != CR_CW_MODE_READ && mode != CR_CW_MODE_WRITE)) {
        g_debug("%s: Filename is NULL or bad mode value", __func__);
        return NULL;
    }


    // Compression type detection

    if (mode == CR_CW_MODE_WRITE) {
        if (comtype == CR_CW_AUTO_DETECT_COMPRESSION) {
            g_debug("%s: CR_CW_AUTO_DETECT_COMPRESSION cannot be used if "
                    "mode is CR_CW_MODE_WRITE", __func__);
            return NULL;
        }

        if (comtype == CR_CW_UNKNOWN_COMPRESSION) {
            g_debug("%s: CR_CW_UNKNOWN_COMPRESSION cannot be used if mode"
            " is CR_CW_MODE_WRITE", __func__);
            return NULL;
        }
    }


    if (comtype != CR_CW_AUTO_DETECT_COMPRESSION) {
        type = comtype;
    } else {
        type = cr_detect_compression(filename);
    }

    if (type == CR_CW_UNKNOWN_COMPRESSION) {
        g_debug("%s: Cannot detect compression type", __func__);
        return NULL;
    }


    // Open file

    file = g_malloc0(sizeof(CR_FILE));
    file->mode = mode;

    switch (type) {

        case (CR_CW_NO_COMPRESSION): // ----------------------------------------
            file->type = CR_CW_NO_COMPRESSION;
            if (mode == CR_CW_MODE_WRITE) {
                file->FILE = (void *) fopen(filename, "w");
            } else {
                file->FILE = (void *) fopen(filename, "r");
            }
            break;

        case (CR_CW_GZ_COMPRESSION): // ----------------------------------------
            file->type = CR_CW_GZ_COMPRESSION;
            if (mode == CR_CW_MODE_WRITE) {
                file->FILE = (void *) gzopen(filename, "wb");
                gzsetparams((gzFile) file->FILE,
                             CR_CW_GZ_COMPRESSION_LEVEL,
                             GZ_STRATEGY);
            } else {
                file->FILE = (void *) gzopen(filename, "rb");
            }

            if (gzbuffer((gzFile) file->FILE, GZ_BUFFER_SIZE) == -1) {
                g_debug("%s: gzbuffer() call failed", __func__);
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): // ---------------------------------------
            file->type = CR_CW_BZ2_COMPRESSION;
            FILE *f;
            if (mode == CR_CW_MODE_WRITE) {
                f = fopen(filename, "wb");
                if (f) {
                    file->FILE = (void *) BZ2_bzWriteOpen(NULL, f,
                                                          BZ2_BLOCKSIZE100K,
                                                          BZ2_VERBOSITY,
                                                          BZ2_WORK_FACTOR);
                }
            } else {
                f = fopen(filename, "rb");
                if (f) {
                    file->FILE = (void *) BZ2_bzReadOpen(NULL, f,
                                                         BZ2_VERBOSITY,
                                                         BZ2_USE_LESS_MEMORY,
                                                         NULL, 0);
                }
            }
            break;

        case (CR_CW_XZ_COMPRESSION): // ----------------------------------------
            file->type = CR_CW_XZ_COMPRESSION;
            XzFile *xz_file = g_malloc(sizeof(XzFile));
            lzma_stream *stream = &(xz_file->stream);
            memset(stream, 0, sizeof(lzma_stream));
            /* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ XXX: This part
             is a little tricky. Because in the default initializer
             LZMA_STREAM_INIT are some items NULL and (according to C standard)
             NULL may have different internal representation than zero.
             This should not be a problem nowadays.
            */

            // Prepare coder/decoder

            int ret;
            if (mode == CR_CW_MODE_WRITE)
                ret = lzma_easy_encoder(stream,
                                        CR_CW_XZ_COMPRESSION_LEVEL,
                                        XZ_CHECK);
            else
                ret = lzma_auto_decoder(stream,
                                        XZ_MEMORY_USAGE_LIMIT,
                                        XZ_DECODER_FLAGS);

            if (ret != LZMA_OK) {
                switch (ret) {
                    case LZMA_MEM_ERROR:
                        g_debug("%s: XZ: Cannot allocate memory",
                                __func__);
                        break;
                    case LZMA_OPTIONS_ERROR:
                        g_debug("%s: XZ: Unsupported flags (options)",
                                __func__);
                        break;
                    case LZMA_PROG_ERROR:
                        g_debug("%s: XZ: One or more of the parameters "
                                "have values that will never be valid.",
                                __func__);
                        break;
                    default:
                        g_debug("%s: XZ: Unknown error", __func__);
                }
                g_free((void *) xz_file);
                break;
            }

            // Open input/output file

            if (mode == CR_CW_MODE_WRITE)
                xz_file->file = fopen(filename, "wb");
            else
                xz_file->file = fopen(filename, "rb");

            if (!(xz_file->file)) {
                lzma_end(&(xz_file->stream));
                g_free((void *) xz_file);
            }

            file->FILE = (void *) xz_file;
            break;

        default: // ------------------------------------------------------------
            break;
    }


    if (!file->FILE) {
        // File is not open -> cleanup
        g_free(file);
        file = NULL;
    }

    return file;
}



int
cr_close(CR_FILE *cr_file)
{
    if (!cr_file) {
        return CR_CW_ERR;
    }


    int ret;
    int cw_ret = CR_CW_ERR;

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ----------------------------------------
            ret = fclose((FILE *) cr_file->FILE);
            if (ret == 0) {
                cw_ret = CR_CW_OK;
            }
            break;

        case (CR_CW_GZ_COMPRESSION): // ----------------------------------------
            ret = gzclose((gzFile) cr_file->FILE);
            if (ret == Z_OK) {
                cw_ret = CR_CW_OK;
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): // ---------------------------------------
            if (cr_file->mode == CR_CW_MODE_READ) {
                BZ2_bzReadClose(&ret, (BZFILE *) cr_file->FILE);
            } else {
                BZ2_bzWriteClose(&ret, (BZFILE *) cr_file->FILE,
                                 BZ2_SKIP_FFLUSH, NULL, NULL);
            }

            if (ret == BZ_OK) {
                cw_ret = CR_CW_OK;
            }
            break;

        case (CR_CW_XZ_COMPRESSION): { // --------------------------------------
            XzFile *xz_file = (XzFile *) cr_file->FILE;
            lzma_stream *stream = &(xz_file->stream);

            if (cr_file->mode == CR_CW_MODE_WRITE) {
                // Write out rest of buffer
                while (1) {
                    int ret;
                    stream->next_out = (uint8_t*) xz_file->buffer;
                    stream->avail_out = XZ_BUFFER_SIZE;
                    ret = lzma_code(stream, LZMA_FINISH);

                    if(ret != LZMA_OK && ret != LZMA_STREAM_END) {
                        // Error while coding
                        break;
                    }

                    size_t out_len = XZ_BUFFER_SIZE - stream->avail_out;
                    if(fwrite(xz_file->buffer, 1, out_len, xz_file->file) != out_len) {
                        // Error while writing
                        break;
                    }

                    if(ret == LZMA_STREAM_END) {
                        // Everything all right
                        cw_ret = CR_CW_OK;
                        break;
                    }
                }
            } else {
                cw_ret = CR_CW_OK;
            }

            fclose(xz_file->file);
            lzma_end(stream);
            g_free(stream);
            break;
        }

        default: // ------------------------------------------------------------
            break;
    }

    g_free(cr_file);

    return cw_ret;
}



int
cr_read(CR_FILE *cr_file, void *buffer, unsigned int len)
{
    if (!cr_file || !buffer || cr_file->mode != CR_CW_MODE_READ) {
        return CR_CW_ERR;
    }


    int bzerror;
    int ret;

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ----------------------------------------
            ret = fread(buffer, 1, len, (FILE *) cr_file->FILE);
            if ((ret != (int) len) && !feof((FILE *) cr_file->FILE)) {
                ret = CR_CW_ERR;
            }
            break;

        case (CR_CW_GZ_COMPRESSION): // ----------------------------------------
            return gzread((gzFile) cr_file->FILE, buffer, len);
            break;

        case (CR_CW_BZ2_COMPRESSION): // ---------------------------------------
            ret = BZ2_bzRead(&bzerror, (BZFILE *) cr_file->FILE, buffer, len);
            if (!ret && bzerror == BZ_SEQUENCE_ERROR)
                // Next read after BZ_STREAM_END (EOF)
                return 0;
            if (bzerror != BZ_OK && bzerror != BZ_STREAM_END) {
                return CR_CW_ERR;
            }
            break;

        case (CR_CW_XZ_COMPRESSION): { // --------------------------------------
            XzFile *xz_file = (XzFile *) cr_file->FILE;
            lzma_stream *stream = &(xz_file->stream);

            stream->next_out = buffer;
            stream->avail_out = len;

            while (stream->avail_out) {
                int lret;

                // Fill input buffer
                if (stream->avail_in == 0) {
                    if ((lret = fread(xz_file->buffer, 1, XZ_BUFFER_SIZE, xz_file->file)) < 0) {
                        g_debug("%s: XZ: Error while fread", __func__);
                        return CR_CW_ERR;   // Error while reading input file
                    } else if (lret == 0) {
                        g_debug("%s: EOF", __func__);
                        break;   // EOF
                    }
                    stream->next_in = xz_file->buffer;
                    stream->avail_in = lret;
                }

                // Decode
                lret = lzma_code(stream, LZMA_RUN);
                if (lret != LZMA_OK && lret != LZMA_STREAM_END) {
                    g_debug("%s: XZ: Error while decoding (%d)", __func__, lret);
                    return CR_CW_ERR;  // Error while decoding
                }
                if (lret == LZMA_STREAM_END) {
                    break;
                }
            }

            ret = len - stream->avail_out;
            break;
        }

        default: // ------------------------------------------------------------
            ret = CR_CW_ERR;
            break;
    }

    return ret;
}



int
cr_write(CR_FILE *cr_file, const void *buffer, unsigned int len)
{
    if (!cr_file || !buffer || cr_file->mode != CR_CW_MODE_WRITE) {
        return CR_CW_ERR;
    }


    int bzerror;
    int ret = CR_CW_ERR;

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ----------------------------------------
            if ((ret = (int) fwrite(buffer, 1, len, (FILE *) cr_file->FILE)) != (int) len) {
                ret = CR_CW_ERR;
            }
            break;

        case (CR_CW_GZ_COMPRESSION): // ----------------------------------------
            if (len == 0) {
                ret = 0;
                break;
            }
            if ((ret = gzwrite((gzFile) cr_file->FILE, buffer, len)) == 0) {
                ret = CR_CW_ERR;
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): // ---------------------------------------
            BZ2_bzWrite(&bzerror, (BZFILE *) cr_file->FILE, (void *) buffer, len);
            if (bzerror == BZ_OK) {
                ret = len;
            } else {
                ret = CR_CW_ERR;
            }
            break;

        case (CR_CW_XZ_COMPRESSION): { // --------------------------------------
            XzFile *xz_file = (XzFile *) cr_file->FILE;
            lzma_stream *stream = &(xz_file->stream);

            ret = len;
            stream->next_in = buffer;
            stream->avail_in = len;

            while (stream->avail_in) {
                int lret;
                stream->next_out = xz_file->buffer;
                stream->avail_out = XZ_BUFFER_SIZE;
                lret = lzma_code(stream, LZMA_RUN);
                if (lret != LZMA_OK) {
                    ret = CR_CW_ERR;
                    break;   // Error while coding
                }

                size_t out_len = XZ_BUFFER_SIZE - stream->avail_out;
                if ((fwrite(xz_file->buffer, 1, out_len, xz_file->file)) != out_len) {
                    ret = CR_CW_ERR;
                    break;   // Error while writing
                }
            }

            break;
        }

        default: // ------------------------------------------------------------
            break;
    }

    return ret;
}



int
cr_puts(CR_FILE *cr_file, const char *str)
{
    if (!cr_file || !str || cr_file->mode != CR_CW_MODE_WRITE) {
        return CR_CW_ERR;
    }


    size_t len;
    int bzerror;
    int ret = CR_CW_ERR;

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ----------------------------------------
            if (fputs(str, (FILE *) cr_file->FILE) != EOF) {
                ret = CR_CW_OK;
            }
            break;

        case (CR_CW_GZ_COMPRESSION): // ----------------------------------------
            if (gzputs((gzFile) cr_file->FILE, str) != -1) {
                ret = CR_CW_OK;
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): // ---------------------------------------
            len = strlen(str);
            BZ2_bzWrite(&bzerror, (BZFILE *) cr_file->FILE, (void *) str, len);
            if (bzerror == BZ_OK) {
                ret = CR_CW_OK;
            } else {
                ret = CR_CW_ERR;
            }
            break;

        case (CR_CW_XZ_COMPRESSION): // ----------------------------------------
            len = strlen(str);
            ret = cr_write(cr_file, str, len);
            if (ret == (int) len) {
                ret = CR_CW_OK;
            } else {
                ret = CR_CW_ERR;
            }
            break;

        default: // ------------------------------------------------------------
            break;
    }

    return ret;
}



int
cr_printf(CR_FILE *cr_file, const char *format, ...)
{
    if (!cr_file || !format || cr_file->mode != CR_CW_MODE_WRITE) {
        return CR_CW_ERR;
    }

    va_list vl;
    va_start(vl, format);

    // Fill format string
    int ret;
    gchar *buf = NULL;
    ret = g_vasprintf(&buf, format, vl);

    va_end(vl);

    if (ret < 0) {
        g_debug("%s: vasprintf() call failed", __func__);
        return CR_CW_ERR;
    }

    assert(buf);

    int tmp_ret;

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ----------------------------------------
            if ((ret = fwrite(buf, 1, ret, cr_file->FILE)) < 0) {
                ret = CR_CW_ERR;
            }
            break;

        case (CR_CW_GZ_COMPRESSION): // ----------------------------------------
            if (gzputs((gzFile) cr_file->FILE, buf) == -1) {
                ret = CR_CW_ERR;
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): // ---------------------------------------
            BZ2_bzWrite(&tmp_ret, (BZFILE *) cr_file->FILE, buf, ret);
            if (tmp_ret != BZ_OK) {
                ret = CR_CW_ERR;
            }
            break;

        case (CR_CW_XZ_COMPRESSION): // ----------------------------------------
            tmp_ret = cr_write(cr_file, buf, ret);
            if (tmp_ret != (int) ret) {
                ret = CR_CW_ERR;
            }
            break;

        default: // ------------------------------------------------------------
            ret = CR_CW_ERR;
            break;
    }

    g_free(buf);

    return ret;
}
