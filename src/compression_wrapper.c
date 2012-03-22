#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <magic.h>
#include <assert.h>
#include <string.h>
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
#define BZ2_WORK_FACTOR         30
#define BZ2_USE_LESS_MEMORY     0
#define BZ2_SKIP_FFLUSH         0


CompressionType detect_compression(const char *filename)
{
    CompressionType type = UNKNOWN_COMPRESSION;

    if (!g_file_test(filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
        return type;
    }


    // Try determine compression type via filename suffix

    if (g_str_has_suffix(filename, ".gz") ||
        g_str_has_suffix(filename, ".gzip"))
    {
        return GZ_COMPRESSION;
    } else if (g_str_has_suffix(filename, ".bz2")) {
        return BZ2_COMPRESSION;
    }


    // No success? Let's get hardcore... (Use magic bytes)

    magic_t myt = magic_open(MAGIC_MIME_TYPE);
    magic_load(myt, NULL);
    if (magic_check(myt, NULL) == -1) {
        g_critical(MODULE"%s: magic_check() failed", __func__);
        return type;
    }

    const char *mime_type = magic_file(myt, filename);

    if (mime_type) {
        g_debug(MODULE"%s: Detected mime type: %s", __func__, mime_type);

        if (g_str_has_suffix(mime_type, "gzip") ||
            g_str_has_suffix(mime_type, "gunzip"))
        {
            type = GZ_COMPRESSION;
        }

        else if (g_str_has_suffix(mime_type, "bzip2")) {
            type = BZ2_COMPRESSION;
        }

        else if (!g_strcmp0(mime_type, "text/plain")) {
            type = NO_COMPRESSION;
        }
    } else {
        g_debug(MODULE"%s: Mime type not detected!", __func__);
    }


    // Xml detection

    if (type == UNKNOWN_COMPRESSION && g_str_has_suffix(filename, ".xml"))
    {
        type = NO_COMPRESSION;
    }

    magic_close(myt);

    return type;
}


CW_FILE *cw_open(const char *filename, int mode, CompressionType comtype)
{
    CW_FILE *file = NULL;
    CompressionType type;

    if (!filename || (mode != CW_MODE_READ && mode != CW_MODE_WRITE)) {
        return NULL;
    }


    // Compression type detection

    if (comtype != AUTO_DETECT_COMPRESSION) {
        type = comtype;
    } else {
        type = detect_compression(filename);
    }

    if (type == UNKNOWN_COMPRESSION) {
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
                    file->FILE = (void *) BZ2_bzWriteOpen(NULL, f, 2, BZ2_VERBOSITY, BZ2_WORK_FACTOR);
                }
            } else {
                f = fopen(filename, "rb");
                if (f) {
                    file->FILE = (void *) BZ2_bzReadOpen(NULL, f, BZ2_VERBOSITY, BZ2_USE_LESS_MEMORY, NULL, 0);
                }
            }
            break;

        default:
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

        default:
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
            if (bzerror != BZ_OK && bzerror != BZ_STREAM_END) {
                return CW_ERR;
            }
            break;

        default:
            ret = CW_ERR;
            break;
    }

    return ret;
}



int cw_write(CW_FILE *cw_file, void *buffer, unsigned int len)
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
            if ((ret = gzwrite((gzFile) cw_file->FILE, buffer, len)) == 0) {
                ret = CW_ERR;
            }
            break;

        case (BZ2_COMPRESSION): // ---------------------------------------------
            BZ2_bzWrite(&bzerror, (BZFILE *) cw_file->FILE, buffer, len);
            if (bzerror == BZ_OK) {
                ret = len;
            } else {
                ret = CW_ERR;
            }
            break;

        default:
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

        default:
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

    int bzerror;

    switch (cw_file->type) {

        case (NO_COMPRESSION): // ----------------------------------------------
            if ((ret = vfprintf((FILE *) cw_file->FILE, format, vl)) < 0) {
                ret = CW_ERR;
            }
            break;

        case (GZ_COMPRESSION): // ----------------------------------------------
            if (gzputs((gzFile) cw_file->FILE, buf) == -1) {
                ret = CW_ERR;
            }
            break;

        case (BZ2_COMPRESSION): // ---------------------------------------------
            BZ2_bzWrite(&bzerror, (BZFILE *) cw_file->FILE, buf, ret);
            if (bzerror != BZ_OK) {
                ret = CW_ERR;
            }
            break;

        default:
            ret = CW_ERR;
            break;
    }

    g_free(buf);

    return ret;
}
