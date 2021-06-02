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
#include <ctype.h>
#include <stdbool.h>
#include <zlib.h>
#include <bzlib.h>
#include <lzma.h>
#ifdef WITH_ZCHUNK
#include <zck.h>
#endif  // WITH_ZCHUNK
#include "error.h"
#include "compression_wrapper.h"


#define ERR_DOMAIN                      CREATEREPO_C_ERROR

/*
#define Z_CR_CW_NO_COMPRESSION          0
#define Z_BEST_SPEED                    1
#define Z_BEST_COMPRESSION              9
#define Z_DEFAULT_COMPRESSION           (-1)
*/
#define CR_CW_GZ_COMPRESSION_LEVEL      Z_DEFAULT_COMPRESSION

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

cr_ContentStat *
cr_contentstat_new(cr_ChecksumType type, GError **err)
{
    cr_ContentStat *cstat;

    assert(!err || *err == NULL);

    cstat = g_malloc0(sizeof(cr_ContentStat));
    cstat->checksum_type = type;

    return cstat;
}

void
cr_contentstat_free(cr_ContentStat *cstat, GError **err)
{
    assert(!err || *err == NULL);

    if (!cstat)
        return;

    g_free(cstat->hdr_checksum);
    g_free(cstat->checksum);
    g_free(cstat);
}

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

    if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        g_debug("%s: File %s doesn't exists or not a regular file",
                __func__, filename);
        g_set_error(err, ERR_DOMAIN, CRE_NOFILE,
                    "File %s doesn't exists or not a regular file", filename);
        return CR_CW_UNKNOWN_COMPRESSION;
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
    } else if (g_str_has_suffix(filename, ".zck"))
    {
        return CR_CW_ZCK_COMPRESSION;
    } else if (g_str_has_suffix(filename, ".xml") ||
               g_str_has_suffix(filename, ".tar") ||
               g_str_has_suffix(filename, ".yaml") ||
               g_str_has_suffix(filename, ".sqlite"))
    {
        return CR_CW_NO_COMPRESSION;
    }

    // No success? Let's get hardcore... (Use magic bytes)

    magic_t myt = magic_open(MAGIC_MIME | MAGIC_SYMLINK);
    if (myt == NULL) {
        g_set_error(err, ERR_DOMAIN, CRE_MAGIC,
                    "magic_open() failed: Cannot allocate the magic cookie");
        return CR_CW_UNKNOWN_COMPRESSION;
    }

    if (magic_load(myt, NULL) == -1) {
        g_set_error(err, ERR_DOMAIN, CRE_MAGIC,
                    "magic_load() failed: %s", magic_error(myt));
        return CR_CW_UNKNOWN_COMPRESSION;
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
                 g_str_has_prefix(mime_type, "application/x-tar") ||
                 g_str_has_prefix(mime_type, "inode/x-empty"))
        {
            type = CR_CW_NO_COMPRESSION;
        }
    } else {
        g_debug("%s: Mime type not detected! (%s): %s", __func__, filename,
                magic_error(myt));
        g_set_error(err, ERR_DOMAIN, CRE_MAGIC,
                    "mime_type() detection failed: %s", magic_error(myt));
        magic_close(myt);
        return CR_CW_UNKNOWN_COMPRESSION;
    }


    // Xml detection

    if (type == CR_CW_UNKNOWN_COMPRESSION && g_str_has_suffix(filename, ".xml"))
        type = CR_CW_NO_COMPRESSION;


    magic_close(myt);

    return type;
}

cr_CompressionType
cr_compression_type(const char *name)
{
    if (!name)
        return CR_CW_UNKNOWN_COMPRESSION;

    int type = CR_CW_UNKNOWN_COMPRESSION;
    gchar *name_lower = g_strdup(name);
    for (gchar *c = name_lower; *c; c++)
        *c = tolower(*c);

    if (!g_strcmp0(name_lower, "gz") || !g_strcmp0(name_lower, "gzip"))
        type = CR_CW_GZ_COMPRESSION;
    if (!g_strcmp0(name_lower, "bz2") || !g_strcmp0(name_lower, "bzip2"))
        type = CR_CW_BZ2_COMPRESSION;
    if (!g_strcmp0(name_lower, "xz"))
        type = CR_CW_XZ_COMPRESSION;
    if (!g_strcmp0(name_lower, "zck"))
        type = CR_CW_ZCK_COMPRESSION;
    g_free(name_lower);

    return type;
}

const char *
cr_compression_suffix(cr_CompressionType comtype)
{
    switch (comtype) {
        case CR_CW_GZ_COMPRESSION:
            return ".gz";
        case CR_CW_BZ2_COMPRESSION:
            return ".bz2";
        case CR_CW_XZ_COMPRESSION:
            return ".xz";
        case CR_CW_ZCK_COMPRESSION:
            return ".zck";
        default:
            return NULL;
    }
}


static const char *
cr_gz_strerror(gzFile f)
{
    int errnum;
    const char *msg = gzerror(f, &errnum);
    if (errnum == Z_ERRNO)
        msg = g_strerror(errno);
    return msg;
}

#ifdef WITH_ZCHUNK
cr_ChecksumType
cr_cktype_from_zck(zckCtx *zck, GError **err)
{
    int cktype = zck_get_full_hash_type(zck);
    if (cktype < 0) {
        g_set_error(err, ERR_DOMAIN, CRE_ZCK,
                    "Unable to read hash from zchunk file");
        return CR_CHECKSUM_UNKNOWN;
    }
    if (cktype == ZCK_HASH_SHA1)
        return CR_CHECKSUM_SHA1;
    else if (cktype == ZCK_HASH_SHA256)
        return CR_CHECKSUM_SHA256;
    else {
        const char *ckname = zck_hash_name_from_type(cktype);
        if (ckname == NULL)
            ckname = "Unknown";
        g_set_error(err, ERR_DOMAIN, CRE_ZCK,
                    "Unknown zchunk checksum type: %s", ckname);
        return CR_CHECKSUM_UNKNOWN;
    }
}
#endif // WITH_ZCHUNK

CR_FILE *
cr_sopen(const char *filename,
         cr_OpenMode mode,
         cr_CompressionType comtype,
         cr_ContentStat *stat,
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
            g_set_error(err, ERR_DOMAIN, CRE_ASSERT,
                        "CR_CW_AUTO_DETECT_COMPRESSION cannot be used if "
                        "mode is CR_CW_MODE_WRITE");
            return NULL;
        }

        if (comtype == CR_CW_UNKNOWN_COMPRESSION) {
            g_debug("%s: CR_CW_UNKNOWN_COMPRESSION cannot be used if mode"
                    " is CR_CW_MODE_WRITE", __func__);
            assert(0);
            g_set_error(err, ERR_DOMAIN, CRE_ASSERT,
                        "CR_CW_UNKNOWN_COMPRESSION cannot be used if mode "
                        "is CR_CW_MODE_WRITE");
            return NULL;
        }
    }


    if (comtype == CR_CW_AUTO_DETECT_COMPRESSION) {
        // Try to detect type of compression
        type = cr_detect_compression(filename, &tmp_err);
        if (tmp_err) {
            // Error while detection
            g_propagate_error(err, tmp_err);
            return NULL;
        }
    }

    if (type == CR_CW_UNKNOWN_COMPRESSION) {
        // Detection without error but compression type is unknown
        g_debug("%s: Cannot detect compression type", __func__);
        g_set_error(err, ERR_DOMAIN, CRE_UNKNOWNCOMPRESSION,
                    "Cannot detect compression type");
        return NULL;
    }


    // Open file

    const char *mode_str = (mode == CR_CW_MODE_WRITE) ? "wb" : "rb";

    file = g_malloc0(sizeof(CR_FILE));
    file->mode = mode;
    file->type = type;
    file->INNERFILE = NULL;

    switch (type) {

        case (CR_CW_NO_COMPRESSION): // ---------------------------------------
            mode_str = (mode == CR_CW_MODE_WRITE) ? "w" : "r";
            file->FILE = (void *) fopen(filename, mode_str);
            if (!file->FILE)
                g_set_error(err, ERR_DOMAIN, CRE_IO,
                            "fopen(): %s", g_strerror(errno));
            break;

        case (CR_CW_GZ_COMPRESSION): // ---------------------------------------
            file->FILE = (void *) gzopen(filename, mode_str);
            if (!file->FILE) {
                g_set_error(err, ERR_DOMAIN, CRE_GZ,
                            "gzopen(): %s", g_strerror(errno));
                break;
            }

            if (mode == CR_CW_MODE_WRITE)
                gzsetparams((gzFile) file->FILE,
                            CR_CW_GZ_COMPRESSION_LEVEL,
                            GZ_STRATEGY);

            if (gzbuffer((gzFile) file->FILE, GZ_BUFFER_SIZE) == -1) {
                g_debug("%s: gzbuffer() call failed", __func__);
                g_set_error(err, ERR_DOMAIN, CRE_GZ,
                            "gzbuffer() call failed");
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): { // ------------------------------------
            FILE *f = fopen(filename, mode_str);
            file->INNERFILE = f;
            int bzerror;

            if (!f) {
                g_set_error(err, ERR_DOMAIN, CRE_IO,
                            "fopen(): %s", g_strerror(errno));
                break;
            }

            if (mode == CR_CW_MODE_WRITE) {
                file->FILE = (void *) BZ2_bzWriteOpen(&bzerror,
                                                      f,
                                                      BZ2_BLOCKSIZE100K,
                                                      BZ2_VERBOSITY,
                                                      BZ2_WORK_FACTOR);
            } else {
                file->FILE = (void *) BZ2_bzReadOpen(&bzerror,
                                                     f,
                                                     BZ2_VERBOSITY,
                                                     BZ2_USE_LESS_MEMORY,
                                                     NULL, 0);
            }

            if (bzerror != BZ_OK) {
                const char *err_msg;

                fclose(f);

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

                g_set_error(err, ERR_DOMAIN, CRE_BZ2,
                            "Bz2 error: %s", err_msg);
            }

            break;
        }

        case (CR_CW_XZ_COMPRESSION): { // -------------------------------------
            int ret;
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

            if (mode == CR_CW_MODE_WRITE) {

#ifdef ENABLE_THREADED_XZ_ENCODER
                // The threaded encoder takes the options as pointer to
                // a lzma_mt structure.
                lzma_mt mt = {
                    // No flags are needed.
                    .flags = 0,

                    // Let liblzma determine a sane block size.
                    .block_size = 0,

                    // Use no timeout for lzma_code() calls by setting timeout
                    // to zero. That is, sometimes lzma_code() might block for
                    // a long time (from several seconds to even minutes).
                    // If this is not OK, for example due to progress indicator
                    // needing updates, specify a timeout in milliseconds here.
                    // See the documentation of lzma_mt in lzma/container.h for
                    // information how to choose a reasonable timeout.
                    .timeout = 0,

                    // Use the default preset (6) for LZMA2.
                    // To use a preset, filters must be set to NULL.
                    .preset = LZMA_PRESET_DEFAULT,
                    .filters = NULL,

                    // Integrity checking.
                    .check = XZ_CHECK,
                };

                // Detect how many threads the CPU supports.
                mt.threads = lzma_cputhreads();

                // If the number of CPU cores/threads cannot be detected,
                // use one thread.
                if (mt.threads == 0)
                    mt.threads = 1;

                // If the number of CPU cores/threads exceeds threads_max,
                // limit the number of threads to keep memory usage lower.
                const uint32_t threads_max = 2;
                if (mt.threads > threads_max)
                    mt.threads = threads_max;

                if (mt.threads > 1)
                    // Initialize the threaded encoder
                    ret = lzma_stream_encoder_mt(stream, &mt);
                else
#endif
                    // Initialize the single-threaded encoder
                    ret = lzma_easy_encoder(stream,
                                            CR_CW_XZ_COMPRESSION_LEVEL,
                                            XZ_CHECK);

            } else {
                ret = lzma_auto_decoder(stream,
                                        XZ_MEMORY_USAGE_LIMIT,
                                        XZ_DECODER_FLAGS);
            }

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
                                  "have values that will never be valid. "
                                  "(Possibly a bug)";
                        break;
                    case LZMA_UNSUPPORTED_CHECK:
		        err_msg = "Specified integrity check is not supported";
		        break;
                    default:
                        err_msg = "Unknown error";
                }

                g_set_error(err, ERR_DOMAIN, CRE_XZ,
                            "XZ error (%d): %s", ret, err_msg);
                g_free((void *) xz_file);
                break;
            }

            // Open input/output file

            FILE *f = fopen(filename, mode_str);
            if (!f) {
                g_set_error(err, ERR_DOMAIN, CRE_XZ,
                            "fopen(): %s", g_strerror(errno));
                lzma_end(&(xz_file->stream));
                g_free((void *) xz_file);
                break;
            }

            xz_file->file = f;
            file->FILE = (void *) xz_file;
            break;
        }
        case (CR_CW_ZCK_COMPRESSION): { // -------------------------------------
#ifdef WITH_ZCHUNK
            FILE *f = fopen(filename, mode_str);

            if (!f) {
                g_set_error(err, ERR_DOMAIN, CRE_IO,
                            "fopen(): %s", g_strerror(errno));
                break;
            }

            file->INNERFILE = f;
            int fd = fileno(f);

            file->FILE = (void *) zck_create();
            zckCtx *zck = file->FILE;
            if (mode == CR_CW_MODE_WRITE) {
                if (!file->FILE || !zck_init_write(zck, fd) ||
                   !zck_set_ioption(zck, ZCK_MANUAL_CHUNK, 1)) {
                    zck_set_log_fd(STDOUT_FILENO);
                    g_set_error(err, ERR_DOMAIN, CRE_IO, "%s",
                                zck_get_error(zck));
                    break;
                }
            } else {
                if (!file->FILE || !zck_init_read(zck, fd)) {
                    g_set_error(err, ERR_DOMAIN, CRE_IO,
                                "%s", zck_get_error(zck));
                    break;
                }
            }
            break;
#else
            g_set_error(err, ERR_DOMAIN, CRE_IO, "createrepo_c wasn't compiled "
                        "with zchunk support");
            break;
#endif // WITH_ZCHUNK
        }

        default: // -----------------------------------------------------------
            break;
    }

    if (!file->FILE) {
        // File is not open -> cleanup
        if (err && *err == NULL)
            g_set_error(err, ERR_DOMAIN, CRE_XZ,
                        "Unknown error while opening: %s", filename);
        g_free(file);
        return NULL;
    }

    if (stat) {
        file->stat = stat;

        if (stat->checksum_type == CR_CHECKSUM_UNKNOWN) {
            file->checksum_ctx = NULL;
        } else {
            file->checksum_ctx = cr_checksum_new(stat->checksum_type,
                                                 &tmp_err);
            if (tmp_err) {
                g_propagate_error(err, tmp_err);
                cr_close(file, NULL);
                return NULL;
            }
        }

#ifdef WITH_ZCHUNK
        /* Fill zchunk header_stat with header information */
        if (mode == CR_CW_MODE_READ && type == CR_CW_ZCK_COMPRESSION) {
            zckCtx *zck = (zckCtx *)file->FILE;
            cr_ChecksumType cktype = cr_cktype_from_zck(zck, err);
            if (cktype == CR_CHECKSUM_UNKNOWN) {
                /* Error is already set in cr_cktype_from_zck */
                g_free(file);
                return NULL;
            }
            file->stat->hdr_checksum_type = cktype;
            file->stat->hdr_checksum = zck_get_header_digest(zck);
            file->stat->hdr_size = zck_get_header_length(zck);
            if (*err != NULL || file->stat->hdr_checksum == NULL ||
               file->stat->hdr_size < 0) {
                g_free(file);
                return NULL;
            }
        }
#endif // WITH_ZCHUNK
    }

    assert(!err || (!file && *err != NULL) || (file && *err == NULL));

    return file;
}

int
cr_set_dict(CR_FILE *cr_file, const void *dict, unsigned int len, GError **err)
{
    int ret = CRE_OK;
    assert(!err || *err == NULL);

    if (len == 0)
        return CRE_OK;

    switch (cr_file->type) {

        case (CR_CW_ZCK_COMPRESSION): { // ------------------------------------
#ifdef WITH_ZCHUNK
            zckCtx *zck = (zckCtx *)cr_file->FILE;
            size_t wlen = (size_t)len;
            if (!zck_set_soption(zck, ZCK_COMP_DICT, dict, wlen)) {
                ret = CRE_ERROR;
                g_set_error(err, ERR_DOMAIN, CRE_ZCK,
                            "Error setting dict");
                break;
            }
            break;
#else
            g_set_error(err, ERR_DOMAIN, CRE_IO, "createrepo_c wasn't compiled "
                        "with zchunk support");
            break;
#endif // WITH_ZCHUNK
        }

        default: { // ---------------------------------------------------------
            ret = CRE_ERROR;
            g_set_error(err, ERR_DOMAIN, CRE_ERROR,
                            "Compression format doesn't support dict");
            break;
        }

    }
    return ret;
}

int
cr_close(CR_FILE *cr_file, GError **err)
{
    int ret = CRE_ERROR;
    int rc;

    assert(!err || *err == NULL);

    if (!cr_file)
        return CRE_OK;

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ---------------------------------------
            if (fclose((FILE *) cr_file->FILE) == 0) {
                ret = CRE_OK;
            } else {
                ret = CRE_IO;
                g_set_error(err, ERR_DOMAIN, CRE_IO,
                            "fclose(): %s", g_strerror(errno));
            }
            break;

        case (CR_CW_GZ_COMPRESSION): // ---------------------------------------
            rc = gzclose((gzFile) cr_file->FILE);
            if (rc == Z_OK)
                ret = CRE_OK;
            else {
                const char *err_msg;
                switch (rc) {
                    case Z_STREAM_ERROR:
                        err_msg = "file is not valid";
                        break;
                    case Z_ERRNO:
                        err_msg = "file operation error";
                        break;
                    case Z_MEM_ERROR:
                        err_msg = "if out of memory";
                        break;
                    case Z_BUF_ERROR:
                        err_msg = "last read ended in the middle of a stream";
                        break;
                    default:
                        err_msg = "error";
                }

                ret = CRE_GZ;
                g_set_error(err, ERR_DOMAIN, CRE_GZ,
                    "gzclose(): %s", err_msg);
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): // --------------------------------------
            if (cr_file->mode == CR_CW_MODE_READ)
                BZ2_bzReadClose(&rc, (BZFILE *) cr_file->FILE);
            else
                BZ2_bzWriteClose(&rc, (BZFILE *) cr_file->FILE,
                                 BZ2_SKIP_FFLUSH, NULL, NULL);

            fclose(cr_file->INNERFILE);

            if (rc == BZ_OK) {
                ret = CRE_OK;
            } else {
                const char *err_msg;

                switch (rc) {
                    case BZ_SEQUENCE_ERROR:
                        // This really should not happen
                        err_msg = "file was opened with BZ2_bzReadOpen";
                        break;
                    case BZ_IO_ERROR:
                        err_msg = "error writing the compressed file";
                        break;
                    default:
                        err_msg = "other error";
                }

                ret = CRE_BZ2;
                g_set_error(err, ERR_DOMAIN, CRE_BZ2,
                            "Bz2 error: %s", err_msg);
            }
            break;

        case (CR_CW_XZ_COMPRESSION): { // -------------------------------------
            XzFile *xz_file = (XzFile *) cr_file->FILE;
            lzma_stream *stream = &(xz_file->stream);

            if (cr_file->mode == CR_CW_MODE_WRITE) {
                // Write out rest of buffer
                while (1) {
                    stream->next_out = (uint8_t*) xz_file->buffer;
                    stream->avail_out = XZ_BUFFER_SIZE;

                    rc = lzma_code(stream, LZMA_FINISH);

                    if (rc != LZMA_OK && rc != LZMA_STREAM_END) {
                        // Error while coding
                        const char *err_msg;

                        switch (rc) {
                            case LZMA_MEM_ERROR:
                                err_msg = "Memory allocation failed";
                                break;
                            case LZMA_DATA_ERROR:
                                // This error is returned if the compressed
                                // or uncompressed size get near 8 EiB
                                // (2^63 bytes) because that's where the .xz
                                // file format size limits currently are.
                                // That is, the possibility of this error
                                // is mostly theoretical unless you are doing
                                // something very unusual.
                                //
                                // Note that strm->total_in and strm->total_out
                                // have nothing to do with this error. Changing
                                // those variables won't increase or decrease
                                // the chance of getting this error.
                                err_msg = "File size limits exceeded";
                                break;
                            default:
                                // This is most likely LZMA_PROG_ERROR.
                                err_msg = "Unknown error, possibly a bug";
                                break;
                        }

                        ret = CRE_XZ;
                        g_set_error(err, ERR_DOMAIN, CRE_XZ,
                                    "XZ: lzma_code() error (%d): %s",
                                    rc, err_msg);
                        break;
                    }

                    size_t olen = XZ_BUFFER_SIZE - stream->avail_out;
                    if (fwrite(xz_file->buffer, 1, olen, xz_file->file) != olen) {
                        // Error while writing
                        ret = CRE_XZ;
                        g_set_error(err, ERR_DOMAIN, CRE_XZ,
                                    "XZ: fwrite() error: %s", g_strerror(errno));
                        break;
                    }

                    if (rc == LZMA_STREAM_END) {
                        // Everything all right
                        ret = CRE_OK;
                        break;
                    }
                }
            } else {
                ret = CRE_OK;
            }

            fclose(xz_file->file);
            lzma_end(stream);
            g_free(stream);
            break;
        }
        case (CR_CW_ZCK_COMPRESSION): { // ------------------------------------
#ifdef WITH_ZCHUNK
            zckCtx *zck = (zckCtx *) cr_file->FILE;
            ret = CRE_OK;
            if (cr_file->mode == CR_CW_MODE_WRITE) {
                if (zck_end_chunk(zck) < 0) {
                    ret = CRE_ZCK;
                    g_set_error(err, ERR_DOMAIN, CRE_ZCK,
                        "Unable to end final chunk: %s", zck_get_error(zck));
                    break;
                }
            }
            if (!zck_close(zck)) {
                ret = CRE_ZCK;
                g_set_error(err, ERR_DOMAIN, CRE_ZCK,
                        "Unable to close zchunk file: %s", zck_get_error(zck));
                break;
            }
            cr_ChecksumType cktype = cr_cktype_from_zck(zck, err);
            if (cktype == CR_CHECKSUM_UNKNOWN) {
                /* Error is already set in cr_cktype_from_zck */
                break;
            }
            if (cr_file->stat) {
                cr_file->stat->hdr_checksum_type = cktype;
                cr_file->stat->hdr_checksum = zck_get_header_digest(zck);
                cr_file->stat->hdr_size = zck_get_header_length(zck);
                if ((err && *err) || cr_file->stat->hdr_checksum == NULL ||
                   cr_file->stat->hdr_size < 0) {
                    ret = CRE_ZCK;
                    g_set_error(err, ERR_DOMAIN, CRE_ZCK,
                                "Unable to get zchunk header information: %s",
                                zck_get_error(zck));
                    break;
                }
            }
            zck_free(&zck);
            fclose(cr_file->INNERFILE);
            break;
#else
            g_set_error(err, ERR_DOMAIN, CRE_IO, "createrepo_c wasn't compiled "
                        "with zchunk support");
            break;
#endif // WITH_ZCHUNK
        }
        default: // -----------------------------------------------------------
            ret = CRE_BADARG;
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                        "Bad compressed file type");
            break;
    }

    if (cr_file->stat) {
        g_free(cr_file->stat->checksum);
        if (cr_file->checksum_ctx)
            cr_file->stat->checksum = cr_checksum_final(cr_file->checksum_ctx,
                                                        NULL);
        else
            cr_file->stat->checksum = NULL;
    }

    g_free(cr_file);

    assert(!err || (ret != CRE_OK && *err != NULL)
           || (ret == CRE_OK && *err == NULL));

    return ret;
}



int
cr_read(CR_FILE *cr_file, void *buffer, unsigned int len, GError **err)
{
    int bzerror;
    int ret = CR_CW_ERR;

    assert(cr_file);
    assert(buffer);
    assert(!err || *err == NULL);

    if (cr_file->mode != CR_CW_MODE_READ) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "File is not opened in read mode");
        return CR_CW_ERR;
    }

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ---------------------------------------
            ret = fread(buffer, 1, len, (FILE *) cr_file->FILE);
            if ((ret != (int) len) && !feof((FILE *) cr_file->FILE)) {
                ret = CR_CW_ERR;
                g_set_error(err, ERR_DOMAIN, CRE_IO,
                            "fread(): %s", g_strerror(errno));
            }
            break;

        case (CR_CW_GZ_COMPRESSION): // ---------------------------------------
            ret = gzread((gzFile) cr_file->FILE, buffer, len);
            if (ret == -1) {
                ret = CR_CW_ERR;
                g_set_error(err, ERR_DOMAIN, CRE_GZ,
                    "fread(): %s", cr_gz_strerror((gzFile) cr_file->FILE));
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): // --------------------------------------
            ret = BZ2_bzRead(&bzerror, (BZFILE *) cr_file->FILE, buffer, len);
            if (!ret && bzerror == BZ_SEQUENCE_ERROR)
                // Next read after BZ_STREAM_END (EOF)
                return 0;

            if (bzerror != BZ_OK && bzerror != BZ_STREAM_END) {
                const char *err_msg;
                ret = CR_CW_ERR;

                switch (bzerror) {
                    case BZ_PARAM_ERROR:
                        // This should not happend
                        err_msg = "bad function params!";
                        break;
                    case BZ_SEQUENCE_ERROR:
                        // This should not happend
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

                g_set_error(err, ERR_DOMAIN, CRE_BZ2,
                            "Bz2 error: %s", err_msg);
            }
            break;

        case (CR_CW_XZ_COMPRESSION): { // -------------------------------------
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
                        g_set_error(err, ERR_DOMAIN, CRE_XZ,
                                    "XZ: fread(): %s", g_strerror(errno));
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
                    const char *err_msg;

                    switch (lret) {
                        case LZMA_MEM_ERROR:
                            err_msg = "Memory allocation failed";
                            break;
			case LZMA_FORMAT_ERROR:
                            // .xz magic bytes weren't found.
                            err_msg = "The input is not in the .xz format";
                            break;
			case LZMA_OPTIONS_ERROR:
                            // For example, the headers specify a filter
                            // that isn't supported by this liblzma
                            // version (or it hasn't been enabled when
                            // building liblzma, but no-one sane does
                            // that unless building liblzma for an
                            // embedded system). Upgrading to a newer
                            // liblzma might help.
                            //
                            // Note that it is unlikely that the file has
                            // accidentally became corrupt if you get this
                            // error. The integrity of the .xz headers is
                            // always verified with a CRC32, so
                            // unintentionally corrupt files can be
                            // distinguished from unsupported files.
                            err_msg = "Unsupported compression options";
                            break;
			case LZMA_DATA_ERROR:
                            err_msg = "Compressed file is corrupt";
                            break;
			case LZMA_BUF_ERROR:
                            // Typically this error means that a valid
                            // file has got truncated, but it might also
                            // be a damaged part in the file that makes
                            // the decoder think the file is truncated.
                            // If you prefer, you can use the same error
                            // message for this as for LZMA_DATA_ERROR.
                            err_msg = "Compressed file is truncated or "
                                      "otherwise corrupt";
                            break;
			default:
                            // This is most likely LZMA_PROG_ERROR.
                            err_msg = "Unknown error, possibly a bug";
                            break;
                    }

                    g_debug("%s: XZ: Error while decoding (%d): %s",
                            __func__, lret, err_msg);
                    g_set_error(err, ERR_DOMAIN, CRE_XZ,
                                "XZ: Error while decoding (%d): %s",
                                lret, err_msg);
                    return CR_CW_ERR;  // Error while decoding
                }

                if (lret == LZMA_STREAM_END)
                    break;
            }

            ret = len - stream->avail_out;
            break;
        }
        case (CR_CW_ZCK_COMPRESSION): { // ------------------------------------
#ifdef WITH_ZCHUNK
            zckCtx *zck = (zckCtx *) cr_file->FILE;
            ssize_t rb = zck_read(zck, buffer, len);
            if (rb < 0) {
                ret = CR_CW_ERR;
                g_set_error(err, ERR_DOMAIN, CRE_ZCK, "ZCK: Unable to read: %s",
                            zck_get_error(zck));
                break;
            }
            ret = rb;
            break;
#else
            g_set_error(err, ERR_DOMAIN, CRE_IO, "createrepo_c wasn't compiled "
                        "with zchunk support");
            break;
#endif // WITH_ZCHUNK
        }

        default: // -----------------------------------------------------------
            ret = CR_CW_ERR;
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                        "Bad compressed file type");
            break;
    }

    assert(!err || (ret == CR_CW_ERR && *err != NULL)
           || (ret != CR_CW_ERR && *err == NULL));

    if (cr_file->stat && ret != CR_CW_ERR) {
        cr_file->stat->size += ret;
        if (cr_file->checksum_ctx) {
            GError *tmp_err = NULL;
            cr_checksum_update(cr_file->checksum_ctx, buffer, ret, &tmp_err);
            if (tmp_err) {
                g_propagate_error(err, tmp_err);
                return CR_CW_ERR;
            }
        }
    }

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
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "File is not opened in read mode");
        return ret;
    }

    if (cr_file->stat) {
        cr_file->stat->size += len;
        if (cr_file->checksum_ctx) {
            GError *tmp_err = NULL;
            cr_checksum_update(cr_file->checksum_ctx, buffer, len, &tmp_err);
            if (tmp_err) {
                g_propagate_error(err, tmp_err);
                return CR_CW_ERR;
            }
        }
    }

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ---------------------------------------
            if ((ret = (int) fwrite(buffer, 1, len, (FILE *) cr_file->FILE)) != (int) len) {
                ret = CR_CW_ERR;
                g_set_error(err, ERR_DOMAIN, CRE_IO,
                            "fwrite(): %s", g_strerror(errno));
            }
            break;

        case (CR_CW_GZ_COMPRESSION): // ---------------------------------------
            if (len == 0) {
                ret = 0;
                break;
            }

            if ((ret = gzwrite((gzFile) cr_file->FILE, buffer, len)) == 0) {
                ret = CR_CW_ERR;
                g_set_error(err, ERR_DOMAIN, CRE_GZ,
                    "gzwrite(): %s", cr_gz_strerror((gzFile) cr_file->FILE));
            }
            break;

        case (CR_CW_BZ2_COMPRESSION): // --------------------------------------
            BZ2_bzWrite(&bzerror, (BZFILE *) cr_file->FILE, (void *) buffer, len);
            if (bzerror == BZ_OK) {
                ret = len;
            } else {
                const char *err_msg;
                ret = CR_CW_ERR;

                switch (bzerror) {
                    case BZ_PARAM_ERROR:
                        // This should not happend
                        err_msg = "bad function params!";
                        break;
                    case BZ_SEQUENCE_ERROR:
                        // This should not happend
                        err_msg = "file was opened with BZ2_bzReadOpen";
                        break;
                    case BZ_IO_ERROR:
                        err_msg = "error while reading from the compressed file";
                        break;
                    default:
                        err_msg = "other error";
                }

                g_set_error(err, ERR_DOMAIN, CRE_BZ2,
                            "Bz2 error: %s", err_msg);
            }
            break;

        case (CR_CW_XZ_COMPRESSION): { // -------------------------------------
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
                    const char *err_msg;
                    ret = CR_CW_ERR;

                    switch (lret) {
                        case LZMA_MEM_ERROR:
                            err_msg = "Memory allocation failed";
                            break;
			case LZMA_DATA_ERROR:
                            // This error is returned if the compressed
                            // or uncompressed size get near 8 EiB
                            // (2^63 bytes) because that's where the .xz
                            // file format size limits currently are.
                            // That is, the possibility of this error
                            // is mostly theoretical unless you are doing
                            // something very unusual.
                            //
                            // Note that strm->total_in and strm->total_out
                            // have nothing to do with this error. Changing
                            // those variables won't increase or decrease
                            // the chance of getting this error.
                            err_msg = "File size limits exceeded";
                            break;
			default:
                            // This is most likely LZMA_PROG_ERROR.
                            err_msg = "Unknown error, possibly a bug";
                            break;
                    }

                    g_set_error(err, ERR_DOMAIN, CRE_XZ,
                                "XZ: lzma_code() error (%d): %s",
                                lret, err_msg);
                    break;   // Error while coding
                }

                size_t out_len = XZ_BUFFER_SIZE - stream->avail_out;
                if ((fwrite(xz_file->buffer, 1, out_len, xz_file->file)) != out_len) {
                    ret = CR_CW_ERR;
                    g_set_error(err, ERR_DOMAIN, CRE_XZ,
                                "XZ: fwrite(): %s", g_strerror(errno));
                    break;   // Error while writing
                }
            }

            break;
        }

        case (CR_CW_ZCK_COMPRESSION): { // ------------------------------------
#ifdef WITH_ZCHUNK
            zckCtx *zck = (zckCtx *) cr_file->FILE;
            ssize_t wb = zck_write(zck, buffer, len);
            if (wb < 0) {
                ret = CR_CW_ERR;
                g_set_error(err, ERR_DOMAIN, CRE_ZCK,
                            "ZCK: Unable to write: %s", zck_get_error(zck));
                break;
            }
            ret = wb;
            break;
#else
            g_set_error(err, ERR_DOMAIN, CRE_IO, "createrepo_c wasn't compiled "
                        "with zchunk support");
            break;
#endif // WITH_ZCHUNK
        }

        default: // -----------------------------------------------------------
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
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
        return 0;

    if (cr_file->mode != CR_CW_MODE_WRITE) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "File is not opened in write mode");
        return CR_CW_ERR;
    }

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ---------------------------------------
        case (CR_CW_GZ_COMPRESSION): // ---------------------------------------
        case (CR_CW_BZ2_COMPRESSION): // --------------------------------------
        case (CR_CW_XZ_COMPRESSION): // ---------------------------------------
        case (CR_CW_ZCK_COMPRESSION): // --------------------------------------
            len = strlen(str);
            ret = cr_write(cr_file, str, len, err);
            if (ret != (int) len)
                ret = CR_CW_ERR;
            break;

        default: // -----------------------------------------------------------
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                        "Bad compressed file type");
            break;
    }

    assert(!err || (ret == CR_CW_ERR && *err != NULL)
           || (ret != CR_CW_ERR && *err == NULL));

    return ret;
}

int
cr_end_chunk(CR_FILE *cr_file, GError **err)
{
    int ret = CRE_OK;

    assert(cr_file);
    assert(!err || *err == NULL);

    if (cr_file->mode != CR_CW_MODE_WRITE) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "File is not opened in write mode");
        return CR_CW_ERR;
    }

    switch (cr_file->type) {
        case (CR_CW_NO_COMPRESSION): // ---------------------------------------
        case (CR_CW_GZ_COMPRESSION): // ---------------------------------------
        case (CR_CW_BZ2_COMPRESSION): // --------------------------------------
        case (CR_CW_XZ_COMPRESSION): // ---------------------------------------
            break;
        case (CR_CW_ZCK_COMPRESSION): { // ------------------------------------
#ifdef WITH_ZCHUNK
            zckCtx *zck = (zckCtx *) cr_file->FILE;
            ssize_t wb = zck_end_chunk(zck);
            if (wb < 0) {
                g_set_error(err, ERR_DOMAIN, CRE_ZCK,
                            "Error ending chunk: %s",
                            zck_get_error(zck));
                return CR_CW_ERR;
            }
            ret = wb;
            break;
#else
            g_set_error(err, ERR_DOMAIN, CRE_IO, "createrepo_c wasn't compiled "
                        "with zchunk support");
            break;
#endif // WITH_ZCHUNK
        }

        default: // -----------------------------------------------------------
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                        "Bad compressed file type");
            return CR_CW_ERR;
            break;
    }

    assert(!err || (ret == CR_CW_ERR && *err != NULL)
           || (ret != CR_CW_ERR && *err == NULL));

    return ret;
}

int
cr_set_autochunk(CR_FILE *cr_file, gboolean auto_chunk, GError **err)
{
    int ret = CRE_OK;

    assert(cr_file);
    assert(!err || *err == NULL);

    if (cr_file->mode != CR_CW_MODE_WRITE) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "File is not opened in write mode");
        return CR_CW_ERR;
    }

    switch (cr_file->type) {
        case (CR_CW_NO_COMPRESSION): // ---------------------------------------
        case (CR_CW_GZ_COMPRESSION): // ---------------------------------------
        case (CR_CW_BZ2_COMPRESSION): // --------------------------------------
        case (CR_CW_XZ_COMPRESSION): // ---------------------------------------
            break;
        case (CR_CW_ZCK_COMPRESSION): { // ------------------------------------
#ifdef WITH_ZCHUNK
            zckCtx *zck = (zckCtx *) cr_file->FILE;
            if (!zck_set_ioption(zck, ZCK_MANUAL_CHUNK, !auto_chunk)) {
                g_set_error(err, ERR_DOMAIN, CRE_ZCK,
                            "Error setting auto_chunk: %s",
                            zck_get_error(zck));
                return CR_CW_ERR;
            }
            break;
#else
            g_set_error(err, ERR_DOMAIN, CRE_IO, "createrepo_c wasn't compiled "
                        "with zchunk support");
            break;
#endif // WITH_ZCHUNK
        }

        default: // -----------------------------------------------------------
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                        "Bad compressed file type");
            return CR_CW_ERR;
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
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "File is not opened in write mode");
        return CR_CW_ERR;
    }

    // Fill format string
    va_start(vl, format);
    ret = g_vasprintf(&buf, format, vl);
    va_end(vl);

    if (ret < 0) {
        g_debug("%s: vasprintf() call failed", __func__);
        g_set_error(err, ERR_DOMAIN, CRE_MEMORY,
                    "vasprintf() call failed");
        g_free(buf);
        return CR_CW_ERR;
    }

    assert(buf);

    int tmp_ret;

    switch (cr_file->type) {

        case (CR_CW_NO_COMPRESSION): // ---------------------------------------
        case (CR_CW_GZ_COMPRESSION): // ---------------------------------------
        case (CR_CW_BZ2_COMPRESSION): // --------------------------------------
        case (CR_CW_XZ_COMPRESSION): // ---------------------------------------
        case (CR_CW_ZCK_COMPRESSION): // --------------------------------------
            tmp_ret = cr_write(cr_file, buf, ret, err);
            if (tmp_ret != (int) ret)
                ret = CR_CW_ERR;
            break;

        default: // -----------------------------------------------------------
            ret = CR_CW_ERR;
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                        "Bad compressed file type");
            break;
    }

    g_free(buf);

    assert(!err || (ret == CR_CW_ERR && *err != NULL)
           || (ret != CR_CW_ERR && *err == NULL));

    return ret;
}

ssize_t 
cr_get_zchunk_with_index(CR_FILE *cr_file, ssize_t zchunk_index, char **copy_buf, GError **err)
{
    assert(cr_file);
    assert(!err || *err == NULL);
    if (cr_file->mode != CR_CW_MODE_READ) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "File is not opened in read mode");
        return 0;
    }
    if (cr_file->type != CR_CW_ZCK_COMPRESSION){
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Bad compressed file type");
        return 0;
    }
#ifdef WITH_ZCHUNK
    zckCtx *zck = (zckCtx *) cr_file->FILE;
    zckChunk *idx = zck_get_chunk(zck, zchunk_index);
    ssize_t chunk_size = zck_get_chunk_size(idx);
    if (chunk_size <= 0)
        return 0;
    *copy_buf = g_malloc(chunk_size);
    return zck_get_chunk_data(idx, *copy_buf, chunk_size);
#else
    g_set_error(err, ERR_DOMAIN, CRE_IO, "createrepo_c wasn't compiled "
                        "with zchunk support");
    return 0;
#endif // WITH_ZCHUNK
}
