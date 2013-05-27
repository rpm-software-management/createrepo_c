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
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <zlib.h>
#include <bzlib.h>
#include <lzma.h>
#include "logging.h"
#include "error.h"
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
cr_detect_compression(const char *filename, GError **err)
{
    cr_CompressionType type = CR_CW_UNKNOWN_COMPRESSION;

    assert(filename);
    assert(!err || *err == NULL);

    if (!g_file_test(filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
        g_debug("%s: File %s doesn't exists or it's not a regular file",
                __func__, filename);
        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_NOFILE,
                    "File %s doesn't exists or it's not a regular file.", filename);
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

    magic_t myt = magic_open(MAGIC_MIME);
    if (myt == NULL) {
        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_MAGIC,
                    "magic_open() failed: Cannot allocate the magic cookie");
        return type;
    }

    if (magic_load(myt, NULL) == -1) {
        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_MAGIC,
                    "magic_load() failed: %s", magic_error(myt));
        return type;
    }

    if (magic_check(myt, NULL) == -1) {
        g_critical("%s: magic_check() failed: %s", __func__, magic_error(myt));
        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_MAGIC,
                    "magic_check() failed: %s", magic_error(myt));
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
        g_debug("%s: Mime type not detected! (%s): %s", __func__, filename,
                magic_error(myt));
        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_MAGIC,
                    "mime_type() detection failed: %s", magic_error(myt));
    }


    // Xml detection

    if (type == CR_CW_UNKNOWN_COMPRESSION && g_str_has_suffix(filename, ".xml"))
        type = CR_CW_NO_COMPRESSION;

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


static char *
cr_gz_strerror(gzFile f)
{
    char *msg = "";
    int errnum = Z_ERRNO;

    msg = (char *) gzerror(f, &errnum);
    if (errnum == Z_ERRNO)
        msg = strerror(errno);
    return msg;
}


CR_FILE *
cr_open(const char *filename,
        cr_OpenMode mode,
        cr_CompressionType comtype,
        GError **err)
{
    CR_FILE *file = NULL;
    cr_CompressionType type = comtype;
    GError *tmp_err = NULL;

    assert(filename);
    assert(mode == CR_CW_MODE_READ || mode == CR_CW_MODE_WRITE);
    assert(mode < CR_CW_MODE_SENTINEL);
    assert(comtype < CR_CW_COMPRESSION_SENTINEL);
    assert(!err || *err == NULL);

    if (mode == CR_CW_MODE_WRITE) {
        if (comtype == CR_CW_AUTO_DETECT_COMPRESSION) {
            g_debug("%s: CR_CW_AUTO_DETECT_COMPRESSION cannot be used if "
                    "mode is CR_CW_MODE_WRITE", __func__);
            assert(0);
        }

        if (comtype == CR_CW_UNKNOWN_COMPRESSION) {
            g_debug("%s: CR_CW_UNKNOWN_COMPRESSION cannot be used if mode"
            " is CR_CW_MODE_WRITE", __func__);
            assert(0);
        }
    }


    if (comtype == CR_CW_AUTO_DETECT_COMPRESSION)
        type = cr_detect_compression(filename, &tmp_err);

    if (tmp_err) {
        // Error while detection
        g_propagate_error(err, tmp_err);
        return NULL;
    }

    if (type == CR_CW_UNKNOWN_COMPRESSION) {
        // Detection without error but compression type is unknown
        g_debug("%s: Cannot detect compression type", __func__);
        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_UNKNOWNCOMPRESSION,
                    "Cannot detect compression type");
        return NULL;
    }


    // Open file

    file = g_malloc0(sizeof(CR_FILE));
    file->mode = mode;

    switch (type) {

        case (CR_CW_NO_COMPRESSION): // ----------------------------------------
            file->type = CR_CW_NO_COMPRESSION;
            if (mode == CR_CW_MODE_WRITE)
                file->FILE = (void *) fopen(filename, "w");
            else
                file->FILE = (void *) fopen(filename, "r");

            if (!file->FILE)
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_IO,
                            "fopen(): %s", strerror(errno));

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

            if (!file->FILE)
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_IO,
                            "fopen(): %s", strerror(errno));

            if (gzbuffer((gzFile) file->FILE, GZ_BUFFER_SIZE) == -1) {
                g_debug("%s: gzbuffer() call failed", __func__);
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_GZ,
                            "gzbuffer() call failed");
            }

            break;

        case (CR_CW_BZ2_COMPRESSION): // ---------------------------------------
            file->type = CR_CW_BZ2_COMPRESSION;
            FILE *f;
            int bzerror = BZ_OK;

            if (mode == CR_CW_MODE_WRITE) {
                f = fopen(filename, "wb");
                if (f) {
                    file->FILE = (void *) BZ2_bzWriteOpen(&bzerror,
                                                          f,
                                                          BZ2_BLOCKSIZE100K,
                                                          BZ2_VERBOSITY,
                                                          BZ2_WORK_FACTOR);
                }
            } else {
                f = fopen(filename, "rb");
                if (f) {
                    file->FILE = (void *) BZ2_bzReadOpen(&bzerror,
                                                         f,
                                                         BZ2_VERBOSITY,
                                                         BZ2_USE_LESS_MEMORY,
                                                         NULL, 0);
                }
            }

            if (!f) {
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_IO,
                            "fopen(): %s", strerror(errno));
            } else if (bzerror != BZ_OK) {
                const char *err_msg;

                switch (bzerror) {
                    case BZ_CONFIG_ERROR:
                        err_msg = "library has been mis-compiled";
                        break;
                    case BZ_PARAM_ERROR:
                        err_msg = "bad function params";
                        break;
                    case BZ_IO_ERROR:
                        err_msg = "ferror(f) is nonzero";
                        break;
                    case BZ_MEM_ERROR:
                        err_msg = "insufficient memory is available";
                        break;
                    default:
                        err_msg = "other error";
                }

                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_BZ2,
                            "Bz2 error: %s", err_msg);
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
                const char *err_msg;

                switch (ret) {
                    case LZMA_MEM_ERROR:
                        err_msg = "Cannot allocate memory";
                        break;
                    case LZMA_OPTIONS_ERROR:
                        err_msg = "Unsupported flags (options)";
                        break;
                    case LZMA_PROG_ERROR:
                        err_msg = "One or more of the parameters "
                                  "have values that will never be valid.";
                        break;
                    default:
                        err_msg = "Unknown error";
                }

                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_XZ,
                            "XZ error: %s", err_msg);
                g_free((void *) xz_file);
                break;
            }

            // Open input/output file

            if (mode == CR_CW_MODE_WRITE)
                xz_file->file = fopen(filename, "wb");
            else
                xz_file->file = fopen(filename, "rb");

            if (!(xz_file->file)) {
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_XZ,
                            "fopen(): %s", strerror(errno));

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
        if (err && *err == NULL)
            g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_XZ,
                        "Unknown error while opening: %s", filename);
        return NULL;
    }

    assert(!err || (!file && *err != NULL) || (file && *err == NULL));

    return file;
}



int
cr_close(CR_FILE *cr_file, GError **err)
{
    int ret;
    int cw_ret = CR_CW_ERR;

    assert(!err || *err == NULL);

    if (!cr_file)
        return CRE_OK;

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ----------------------------------------
            ret = fclose((FILE *) cr_file->FILE);
            if (ret == 0)
                cw_ret = CR_CW_OK;
            else
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_IO,
                            "fclose(): %s", strerror(errno));

            break;

        case (CR_CW_GZ_COMPRESSION): // ----------------------------------------
            ret = gzclose((gzFile) cr_file->FILE);
            if (ret == Z_OK) {
                cw_ret = CR_CW_OK;
            } else {
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_GZ,
                    "gzclose(): %s", cr_gz_strerror((gzFile) cr_file->FILE));
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): // ---------------------------------------
            if (cr_file->mode == CR_CW_MODE_READ)
                BZ2_bzReadClose(&ret, (BZFILE *) cr_file->FILE);
            else
                BZ2_bzWriteClose(&ret, (BZFILE *) cr_file->FILE,
                                 BZ2_SKIP_FFLUSH, NULL, NULL);

            if (ret == BZ_OK) {
                cw_ret = CR_CW_OK;
            } else {
                const char *err_msg;

                switch (ret) {
                    case BZ_SEQUENCE_ERROR:
                        // This really shoud not happen
                        err_msg = "file was opened with BZ2_bzReadOpen";
                        break;
                    case BZ_IO_ERROR:
                        err_msg = "error writing the compressed file";
                        break;
                    default:
                        err_msg = "other error";
                }

                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_BZ2,
                            "Bz2 error: %s", err_msg);
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
                        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_XZ,
                                    "XZ: lzma_code() error");
                        break;
                    }

                    size_t out_len = XZ_BUFFER_SIZE - stream->avail_out;
                    if(fwrite(xz_file->buffer, 1, out_len, xz_file->file) != out_len) {
                        // Error while writing
                        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_XZ,
                                    "XZ: fwrite() error: %s", strerror(errno));
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
            g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_BADARG,
                        "Bad compressed file type");
            break;
    }

    g_free(cr_file);

    assert(!err || (cw_ret == CR_CW_ERR && *err != NULL)
           || (cw_ret != CR_CW_ERR && *err == NULL));

    return cw_ret;
}



int
cr_read(CR_FILE *cr_file, void *buffer, unsigned int len, GError **err)
{
    int bzerror;
    int ret;

    assert(cr_file);
    assert(buffer);
    assert(!err || *err == NULL);

    if (cr_file->mode != CR_CW_MODE_READ) {
        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_BADARG,
                    "File is not opened in read mode");
        return CR_CW_ERR;
    }

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ----------------------------------------
            ret = fread(buffer, 1, len, (FILE *) cr_file->FILE);
            if ((ret != (int) len) && !feof((FILE *) cr_file->FILE)) {
                ret = CR_CW_ERR;
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_IO,
                            "fread(): %s", strerror(errno));
            }
            break;

        case (CR_CW_GZ_COMPRESSION): // ----------------------------------------
            ret = gzread((gzFile) cr_file->FILE, buffer, len);
            if (ret == -1) {
                ret = CR_CW_ERR;
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_GZ,
                    "fread(): %s", cr_gz_strerror((gzFile) cr_file->FILE));
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): // ---------------------------------------
            ret = BZ2_bzRead(&bzerror, (BZFILE *) cr_file->FILE, buffer, len);
            if (!ret && bzerror == BZ_SEQUENCE_ERROR)
                // Next read after BZ_STREAM_END (EOF)
                return 0;

            if (bzerror != BZ_OK && bzerror != BZ_STREAM_END) {
                const char *err_msg;
                ret = CR_CW_ERR;

                switch (bzerror) {
                    case BZ_PARAM_ERROR:
                        // This shoud not happend
                        err_msg = "bad function params!";
                        break;
                    case BZ_SEQUENCE_ERROR:
                        // This shoud not happend
                        err_msg = "file was opened with BZ2_bzWriteOpen";
                        break;
                    case BZ_IO_ERROR:
                        err_msg = "error while reading from the compressed file";
                        break;
                    case BZ_UNEXPECTED_EOF:
                        err_msg = "the compressed file ended before "
                                  "the logical end-of-stream was detected";
                        break;
                    case BZ_DATA_ERROR:
                        err_msg = "data integrity error was detected in "
                                  "the compressed stream";
                        break;
                    case BZ_DATA_ERROR_MAGIC:
                        err_msg = "the stream does not begin with "
                                  "the requisite header bytes (ie, is not "
                                  "a bzip2 data file).";
                        break;
                    case BZ_MEM_ERROR:
                        err_msg = "insufficient memory was available";
                        break;
                    default:
                        err_msg = "other error";
                }

                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_BZ2,
                            "Bz2 error: %s", err_msg);
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
                        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_XZ,
                                    "XZ: fread(): %s", strerror(errno));
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
                    g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_XZ,
                                "XZ: Error while deconding(%d)", lret);
                    return CR_CW_ERR;  // Error while decoding
                }

                if (lret == LZMA_STREAM_END)
                    break;
            }

            ret = len - stream->avail_out;
            break;
        }

        default: // ------------------------------------------------------------
            ret = CR_CW_ERR;
            g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_BADARG,
                        "Bad compressed file type");
            break;
    }

    assert(!err || (ret == CR_CW_ERR && *err != NULL)
           || (ret != CR_CW_ERR && *err == NULL));

    return ret;
}



int
cr_write(CR_FILE *cr_file, const void *buffer, unsigned int len, GError **err)
{
    int bzerror;
    int ret = CR_CW_ERR;

    assert(cr_file);
    assert(buffer);
    assert(!err || *err == NULL);

    if (cr_file->mode != CR_CW_MODE_WRITE) {
        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_BADARG,
                    "File is not opened in read mode");
        return ret;
    }

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ----------------------------------------
            if ((ret = (int) fwrite(buffer, 1, len, (FILE *) cr_file->FILE)) != (int) len) {
                ret = CR_CW_ERR;
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_IO,
                            "fwrite(): %s", strerror(errno));
            }
            break;

        case (CR_CW_GZ_COMPRESSION): // ----------------------------------------
            if (len == 0) {
                ret = 0;
                break;
            }

            if ((ret = gzwrite((gzFile) cr_file->FILE, buffer, len)) == 0) {
                ret = CR_CW_ERR;
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_GZ,
                    "gzwrite(): %s", cr_gz_strerror((gzFile) cr_file->FILE));
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): // ---------------------------------------
            BZ2_bzWrite(&bzerror, (BZFILE *) cr_file->FILE, (void *) buffer, len);
            if (bzerror == BZ_OK) {
                ret = len;
            } else {
                const char *err_msg;
                ret = CR_CW_ERR;

                switch (bzerror) {
                    case BZ_PARAM_ERROR:
                        // This shoud not happend
                        err_msg = "bad function params!";
                        break;
                    case BZ_SEQUENCE_ERROR:
                        // This shoud not happend
                        err_msg = "file was opened with BZ2_bzReadOpen";
                        break;
                    case BZ_IO_ERROR:
                        err_msg = "error while reading from the compressed file";
                        break;
                    default:
                        err_msg = "other error";
                }

                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_BZ2,
                            "Bz2 error: %s", err_msg);
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
                    g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_XZ,
                                "XZ: lzma_code() error");
                    break;   // Error while coding
                }

                size_t out_len = XZ_BUFFER_SIZE - stream->avail_out;
                if ((fwrite(xz_file->buffer, 1, out_len, xz_file->file)) != out_len) {
                    ret = CR_CW_ERR;
                    g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_XZ,
                                "XZ: fwrite(): %s", strerror(errno));
                    break;   // Error while writing
                }
            }

            break;
        }

        default: // ------------------------------------------------------------
            g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_BADARG,
                        "Bad compressed file type");
            break;
    }

    assert(!err || (ret == CR_CW_ERR && *err != NULL)
           || (ret != CR_CW_ERR && *err == NULL));

    return ret;
}



int
cr_puts(CR_FILE *cr_file, const char *str, GError **err)
{
    size_t len;
    int ret = CR_CW_ERR;

    assert(cr_file);
    assert(!err || *err == NULL);

    if (!str)
        return CR_CW_OK;

    if (cr_file->mode != CR_CW_MODE_WRITE) {
        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_BADARG,
                    "File is not opened in write mode");
        return CR_CW_ERR;
    }

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ----------------------------------------
            if (fputs(str, (FILE *) cr_file->FILE) != EOF) {
                ret = CR_CW_OK;
            } else {
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_IO,
                            "fputs(): %s", strerror(errno));
            }
            break;

        case (CR_CW_GZ_COMPRESSION): // ----------------------------------------
            if (gzputs((gzFile) cr_file->FILE, str) != -1) {
                ret = CR_CW_OK;
            } else {
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_GZ,
                    "gzputs(): %s", cr_gz_strerror((gzFile) cr_file->FILE));
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): // ---------------------------------------
        case (CR_CW_XZ_COMPRESSION): // ----------------------------------------
            len = strlen(str);
            ret = cr_write(cr_file, str, len, err);
            if (ret == (int) len)
                ret = CR_CW_OK;
            else
                ret = CR_CW_ERR;
            break;

        default: // ------------------------------------------------------------
            g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_BADARG,
                        "Bad compressed file type");
            break;
    }

    assert(!err || (ret == CR_CW_ERR && *err != NULL)
           || (ret != CR_CW_ERR && *err == NULL));

    return ret;
}



int
cr_printf(GError **err, CR_FILE *cr_file, const char *format, ...)
{
    va_list vl;
    int ret;
    gchar *buf = NULL;

    assert(cr_file);
    assert(!err || *err == NULL);

    if (!format)
        return 0;

    if (cr_file->mode != CR_CW_MODE_WRITE) {
        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_BADARG,
                    "File is not opened in write mode");
        return CR_CW_ERR;
    }

    // Fill format string
    va_start(vl, format);
    ret = g_vasprintf(&buf, format, vl);
    va_end(vl);

    if (ret < 0) {
        g_debug("%s: vasprintf() call failed", __func__);
        g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_MEMORY,
                    "vasprintf() call failed");
        return CR_CW_ERR;
    }

    assert(buf);

    int tmp_ret;

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ----------------------------------------
            if ((ret = fwrite(buf, 1, ret, cr_file->FILE)) < 0) {
                ret = CR_CW_ERR;
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_IO,
                            "fwrite(): %s", strerror(errno));
            }
            break;

        case (CR_CW_GZ_COMPRESSION): // ----------------------------------------
            if (gzputs((gzFile) cr_file->FILE, buf) == -1) {
                ret = CR_CW_ERR;
                g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_GZ,
                    "gzputs(): %s", cr_gz_strerror((gzFile) cr_file->FILE));
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): // ---------------------------------------
        case (CR_CW_XZ_COMPRESSION): // ----------------------------------------
            tmp_ret = cr_write(cr_file, buf, ret, err);
            if (tmp_ret != (int) ret) {
                ret = CR_CW_ERR;
            }
            break;

        default: // ------------------------------------------------------------
            ret = CR_CW_ERR;
            g_set_error(err, CR_COMPRESSION_WRAPPER_ERROR, CRE_BADARG,
                        "Bad compressed file type");
            break;
    }

    g_free(buf);

    assert(!err || (ret == CR_CW_ERR && *err != NULL)
           || (ret != CR_CW_ERR && *err == NULL));

    return ret;
}
