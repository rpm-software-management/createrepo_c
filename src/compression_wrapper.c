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

#undef MODULE
#define MODULE "compression_wrapper: "

/*
#define Z_NO_COMPRESSION         0
#define Z_BEST_SPEED             1
#define Z_BEST_COMPRESSION       9
#define Z_DEFAULT_COMPRESSION  (-1)
*/
#define GZ_COMPRESSION_LEVEL    Z_DEFAULT_COMPRESSION

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
#define BZ2_BLOCKSIZE100K       5  // Higher gives better compression but takes more memory
#define BZ2_WORK_FACTOR         0  // 0 == default == 30 (available values 0-250)
#define BZ2_USE_LESS_MEMORY     0
#define BZ2_SKIP_FFLUSH         0

/*
number 0..9
or
LZMA_PRESET_DEFAULT default preset
LZMA_PRESET_EXTREME significantly slower, improving the compression ratio marginally
*/
#define XZ_COMPRESSION_LEVEL    5

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



CompressionType detect_compression(const char *filename)
{
    CompressionType type = UNKNOWN_COMPRESSION;

    if (!g_file_test(filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
        g_debug(MODULE"%s: File %s doesn't exists or it's not a regular file", __func__, filename);
        return type;
    }

    // Try determine compression type via filename suffix

    if (g_str_has_suffix(filename, ".gz") ||
        g_str_has_suffix(filename, ".gzip") ||
        g_str_has_suffix(filename, ".gunzip"))
    {
        return GZ_COMPRESSION;
    } else if (g_str_has_suffix(filename, ".bz2") ||
               g_str_has_suffix(filename, ".bzip2"))
    {
        return BZ2_COMPRESSION;
    } else if (g_str_has_suffix(filename, ".xz"))
    {
        return XZ_COMPRESSION;
    } else if (g_str_has_suffix(filename, ".xml"))
    {
        return NO_COMPRESSION;
    }


    // No success? Let's get hardcore... (Use magic bytes)

    //magic_t myt = magic_open(MAGIC_MIME_TYPE);
    magic_t myt = magic_open(MAGIC_MIME);
    magic_load(myt, NULL);
    if (magic_check(myt, NULL) == -1) {
        g_critical(MODULE"%s: magic_check() failed", __func__);
        return type;
    }

    const char *mime_type = magic_file(myt, filename);

    if (mime_type) {
        g_debug(MODULE"%s: Detected mime type: %s (%s)", __func__, mime_type, filename);

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
            type = GZ_COMPRESSION;
        }

        else if (g_str_has_prefix(mime_type, "application/x-bzip2") ||
                 g_str_has_prefix(mime_type, "application/x-bz2") ||
                 g_str_has_prefix(mime_type, "application/bzip2") ||
                 g_str_has_prefix(mime_type, "application/bz2"))
        {
            type = BZ2_COMPRESSION;
        }

        else if (g_str_has_prefix(mime_type, "application/x-xz"))
        {
            type = XZ_COMPRESSION;
        }

        else if (g_str_has_prefix(mime_type, "text/plain") ||
                 g_str_has_prefix(mime_type, "text/xml") ||
                 g_str_has_prefix(mime_type, "application/xml") ||
                 g_str_has_prefix(mime_type, "application/x-xml") ||
                 g_str_has_prefix(mime_type, "application/x-empty") ||
                 g_str_has_prefix(mime_type, "inode/x-empty"))
        {
            type = NO_COMPRESSION;
        }
    } else {
        g_debug(MODULE"%s: Mime type not detected! (%s)", __func__, filename);
    }


    // Xml detection

    if (type == UNKNOWN_COMPRESSION && g_str_has_suffix(filename, ".xml"))
    {
        type = NO_COMPRESSION;
    }

    magic_close(myt);

    return type;
}


const char *get_suffix(CompressionType comtype)
{
    char *suffix = NULL;
    switch (comtype) {
        case GZ_COMPRESSION:
            suffix = ".gz"; break;
        case BZ2_COMPRESSION:
            suffix = ".bz2"; break;
        case XZ_COMPRESSION:
            suffix = ".xz"; break;
        default:
            break;
    }

    return suffix;
}


CW_FILE *cw_open(const char *filename, OpenMode mode, CompressionType comtype)
{
    CW_FILE *file = NULL;
    CompressionType type;

    if (!filename || (mode != CW_MODE_READ && mode != CW_MODE_WRITE)) {
        g_debug(MODULE"%s: Filename is NULL or bad mode value", __func__);
        return NULL;
    }


    // Compression type detection

    if (mode == CW_MODE_WRITE) {
        if (comtype == AUTO_DETECT_COMPRESSION) {
            g_debug(MODULE"%s: AUTO_DETECT_COMPRESSION cannot be used if mode is CW_MODE_WRITE", __func__);
            return NULL;
        }

        if (comtype == UNKNOWN_COMPRESSION) {
            g_debug(MODULE"%s: UNKNOWN_COMPRESSION cannot be used if mode is CW_MODE_WRITE", __func__);
            return NULL;
        }
    }


    if (comtype != AUTO_DETECT_COMPRESSION) {
        type = comtype;
    } else {
        type = detect_compression(filename);
    }

    if (type == UNKNOWN_COMPRESSION) {
        g_debug(MODULE"%s: Cannot detect compression type", __func__);
        return NULL;
    }


    // Open file

    file = g_malloc0(sizeof(CW_FILE));
    file->mode = mode;

    switch (type) {

        case (NO_COMPRESSION): // ----------------------------------------------
            file->type = NO_COMPRESSION;
            if (mode == CW_MODE_WRITE) {
                file->FILE = (void *) fopen(filename, "w");
            } else {
                file->FILE = (void *) fopen(filename, "r");
            }
            break;

        case (GZ_COMPRESSION): // ----------------------------------------------
            file->type = GZ_COMPRESSION;
            if (mode == CW_MODE_WRITE) {
                file->FILE = (void *) gzopen(filename, "wb");
                gzsetparams((gzFile) file->FILE, GZ_COMPRESSION_LEVEL, GZ_STRATEGY);
            } else {
                file->FILE = (void *) gzopen(filename, "rb");
            }

            if (gzbuffer((gzFile) file->FILE, GZ_BUFFER_SIZE) == -1) {
                g_debug(MODULE"%s: gzbuffer() call failed", __func__);
            }
            break;

        case (BZ2_COMPRESSION): // ---------------------------------------------
            file->type = BZ2_COMPRESSION;
            FILE *f;
            if (mode == CW_MODE_WRITE) {
                f = fopen(filename, "wb");
                if (f) {
                    file->FILE = (void *) BZ2_bzWriteOpen(NULL, f, BZ2_BLOCKSIZE100K, BZ2_VERBOSITY, BZ2_WORK_FACTOR);
                }
            } else {
                f = fopen(filename, "rb");
                if (f) {
                    file->FILE = (void *) BZ2_bzReadOpen(NULL, f, BZ2_VERBOSITY, BZ2_USE_LESS_MEMORY, NULL, 0);
                }
            }
            break;

        case (XZ_COMPRESSION): // ----------------------------------------------
            file->type = XZ_COMPRESSION;
            XzFile *xz_file = g_malloc(sizeof(XzFile));
            lzma_stream *stream = &(xz_file->stream);
            memset(stream, 0, sizeof(lzma_stream));
            /* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ XXX: This part
             is a little tricky. Because in the default initializer LZMA_STREAM_INIT
             are some items NULL and (according to C standard) NULL may have
             different internal representation than zero.
             This should not be a problem nowadays, but who knows what the future will bring.
            */

            // Prepare coder/decoder

            int ret;
            if (mode == CW_MODE_WRITE)
                ret = lzma_easy_encoder(stream, XZ_COMPRESSION_LEVEL, XZ_CHECK);
            else
                ret = lzma_auto_decoder(stream, XZ_MEMORY_USAGE_LIMIT, XZ_DECODER_FLAGS);

            if (ret != LZMA_OK) {
                switch (ret) {
                    case LZMA_MEM_ERROR:
                        g_debug(MODULE"%s: XZ: Cannot allocate memory", __func__);
                        break;
                    case LZMA_OPTIONS_ERROR:
                        g_debug(MODULE"%s: XZ: Unsupported flags (options)", __func__);
                        break;
                    case LZMA_PROG_ERROR:
                        g_debug(MODULE"%s: XZ: One or more of the parameters have values that will never be valid.", __func__);
                        break;
                    default:
                        g_debug(MODULE"%s: XZ: Unknown error", __func__);
                }
                g_free((void *) xz_file);
                break;
            }

            // Open input/output file

            if (mode == CW_MODE_WRITE)
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



int cw_close(CW_FILE *cw_file)
{
    if (!cw_file) {
        return CW_ERR;
    }


    int ret;
    int cw_ret = CW_ERR;

    switch (cw_file->type) {

        case (NO_COMPRESSION): // ----------------------------------------------
            ret = fclose((FILE *) cw_file->FILE);
            if (ret == 0) {
                cw_ret = CW_OK;
            }
            break;

        case (GZ_COMPRESSION): // ----------------------------------------------
            ret = gzclose((gzFile) cw_file->FILE);
            if (ret == Z_OK) {
                cw_ret = CW_OK;
            }
            break;

        case (BZ2_COMPRESSION): // ---------------------------------------------
            if (cw_file->mode == CW_MODE_READ) {
                BZ2_bzReadClose(&ret, (BZFILE *) cw_file->FILE);
            } else {
                BZ2_bzWriteClose(&ret, (BZFILE *) cw_file->FILE, BZ2_SKIP_FFLUSH, NULL, NULL);
            }

            if (ret == BZ_OK) {
                cw_ret = CW_OK;
            }
            break;

        case (XZ_COMPRESSION): { // --------------------------------------------
            XzFile *xz_file = (XzFile *) cw_file->FILE;
            lzma_stream *stream = &(xz_file->stream);

            if (cw_file->mode == CW_MODE_WRITE) {
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
                        cw_ret = CW_OK;
                        break;
                    }
                }
            } else {
                cw_ret = CW_OK;
            }

            fclose(xz_file->file);
            lzma_end(stream);
            g_free(stream);
            break;
        }

        default: // ------------------------------------------------------------
            break;
    }

    g_free(cw_file);

    return cw_ret;
}



int cw_read(CW_FILE *cw_file, void *buffer, unsigned int len)
{
    if (!cw_file || !buffer || cw_file->mode != CW_MODE_READ) {
        return CW_ERR;
    }


    int bzerror;
    int ret;

    switch (cw_file->type) {

        case (NO_COMPRESSION): // ----------------------------------------------
            ret = fread(buffer, 1, len, (FILE *) cw_file->FILE);
            if ((ret != (int) len) && !feof((FILE *) cw_file->FILE)) {
                ret = CW_ERR;
            }
            break;

        case (GZ_COMPRESSION): // ----------------------------------------------
            return gzread((gzFile) cw_file->FILE, buffer, len);
            break;

        case (BZ2_COMPRESSION): // ---------------------------------------------
            ret = BZ2_bzRead(&bzerror, (BZFILE *) cw_file->FILE, buffer, len);
            if (!ret && bzerror == BZ_SEQUENCE_ERROR)
                // Next read after BZ_STREAM_END (EOF)
                return 0;
            if (bzerror != BZ_OK && bzerror != BZ_STREAM_END) {
                return CW_ERR;
            }
            break;

        case (XZ_COMPRESSION): { // --------------------------------------------
            XzFile *xz_file = (XzFile *) cw_file->FILE;
            lzma_stream *stream = &(xz_file->stream);

            stream->next_out = buffer;
            stream->avail_out = len;

            while (stream->avail_out) {
                int lret;

                // Fill input buffer
                if (stream->avail_in == 0) {
                    if ((lret = fread(xz_file->buffer, 1, XZ_BUFFER_SIZE, xz_file->file)) < 0) {
                        g_debug(MODULE"%s: XZ: Error while fread", __func__);
                        return CW_ERR;   // Error while reading input file
                    } else if (lret == 0) {
                        g_debug(MODULE"%s: EOF", __func__);
                        break;   // EOF
                    }
                    stream->next_in = xz_file->buffer;
                    stream->avail_in = lret;
                }

                // Decode
                lret = lzma_code(stream, LZMA_RUN);
                if (lret != LZMA_OK && lret != LZMA_STREAM_END) {
                    g_debug(MODULE"%s: XZ: Error while decoding (%d)", __func__, lret);
                    return CW_ERR;  // Error while decoding
                }
                if (lret == LZMA_STREAM_END) {
                    break;
                }
            }

            ret = len - stream->avail_out;
            break;
        }

        default: // ------------------------------------------------------------
            ret = CW_ERR;
            break;
    }

    return ret;
}



int cw_write(CW_FILE *cw_file, const void *buffer, unsigned int len)
{
    if (!cw_file || !buffer || cw_file->mode != CW_MODE_WRITE) {
        return CW_ERR;
    }


    int bzerror;
    int ret = CW_ERR;

    switch (cw_file->type) {

        case (NO_COMPRESSION): // ----------------------------------------------
            if ((ret = (int) fwrite(buffer, 1, len, (FILE *) cw_file->FILE)) != (int) len) {
                ret = CW_ERR;
            }
            break;

        case (GZ_COMPRESSION): // ----------------------------------------------
            if (len == 0) {
                ret = 0;
                break;
            }
            if ((ret = gzwrite((gzFile) cw_file->FILE, buffer, len)) == 0) {
                ret = CW_ERR;
            }
            break;

        case (BZ2_COMPRESSION): // ---------------------------------------------
            BZ2_bzWrite(&bzerror, (BZFILE *) cw_file->FILE, (void *) buffer, len);
            if (bzerror == BZ_OK) {
                ret = len;
            } else {
                ret = CW_ERR;
            }
            break;

        case (XZ_COMPRESSION): { // --------------------------------------------
            XzFile *xz_file = (XzFile *) cw_file->FILE;
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
                    ret = CW_ERR;
                    break;   // Error while coding
                }

                size_t out_len = XZ_BUFFER_SIZE - stream->avail_out;
                if ((fwrite(xz_file->buffer, 1, out_len, xz_file->file)) != out_len) {
                    ret = CW_ERR;
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



int cw_puts(CW_FILE *cw_file, const char *str)
{
    if (!cw_file || !str || cw_file->mode != CW_MODE_WRITE) {
        return CW_ERR;
    }


    size_t len;
    int bzerror;
    int ret = CW_ERR;

    switch (cw_file->type) {

        case (NO_COMPRESSION): // ----------------------------------------------
            if (fputs(str, (FILE *) cw_file->FILE) != EOF) {
                ret = CW_OK;
            }
            break;

        case (GZ_COMPRESSION): // ----------------------------------------------
            if (gzputs((gzFile) cw_file->FILE, str) != -1) {
                ret = CW_OK;
            }
            break;

        case (BZ2_COMPRESSION): // ---------------------------------------------
            len = strlen(str);
            BZ2_bzWrite(&bzerror, (BZFILE *) cw_file->FILE, (void *) str, len);
            if (bzerror == BZ_OK) {
                ret = CW_OK;
            } else {
                ret = CW_ERR;
            }
            break;

        case (XZ_COMPRESSION): // ----------------------------------------------
            len = strlen(str);
            ret = cw_write(cw_file, str, len);
            if (ret == (int) len) {
                ret = CW_OK;
            } else {
                ret = CW_ERR;
            }
            break;

        default: // ------------------------------------------------------------
            break;
    }

    return ret;
}



int cw_printf(CW_FILE *cw_file, const char *format, ...)
{
    if (!cw_file || !format || cw_file->mode != CW_MODE_WRITE) {
        return CW_ERR;
    }

    va_list vl;
    va_start(vl, format);

    // Fill format string
    int ret;
    gchar *buf = NULL;
    ret = g_vasprintf(&buf, format, vl);

    va_end(vl);

    if (ret < 0) {
        g_debug(MODULE"%s: vasprintf() call failed", __func__);
        return CW_ERR;
    }

    assert(buf);

    int tmp_ret;

    switch (cw_file->type) {

        case (NO_COMPRESSION): // ----------------------------------------------
            if ((ret = fwrite(buf, 1, ret, cw_file->FILE)) < 0) {
                ret = CW_ERR;
            }
            break;

        case (GZ_COMPRESSION): // ----------------------------------------------
            if (gzputs((gzFile) cw_file->FILE, buf) == -1) {
                ret = CW_ERR;
            }
            break;

        case (BZ2_COMPRESSION): // ---------------------------------------------
            BZ2_bzWrite(&tmp_ret, (BZFILE *) cw_file->FILE, buf, ret);
            if (tmp_ret != BZ_OK) {
                ret = CW_ERR;
            }
            break;

        case (XZ_COMPRESSION): // ----------------------------------------------
            tmp_ret = cw_write(cw_file, buf, ret);
            if (tmp_ret != (int) ret) {
                ret = CW_ERR;
            }
            break;

        default: // ------------------------------------------------------------
            ret = CW_ERR;
            break;
    }

    g_free(buf);

    return ret;
}
