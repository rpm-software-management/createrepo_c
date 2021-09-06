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

#define _XOPEN_SOURCE 500

#include <glib/gstdio.h>
#include <glib.h>
#include <arpa/inet.h>
#include <assert.h>
#include <curl/curl.h>
#include <errno.h>
#include <ftw.h>
#include <rpm/rpmlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "cleanup.h"
#include "error.h"
#include "misc.h"
#include "version.h"

#define ERR_DOMAIN      CREATEREPO_C_ERROR
#define BUFFER_SIZE     4096

#define xstr(s) str(s)
#define str(s) #s


const char *
cr_flag_to_str(gint64 flags)
{
    flags &= 0xf;
    switch(flags) {
        case 0:
            return NULL;
        case 2:
            return "LT";
        case 4:
            return "GT";
        case 8:
            return "EQ";
        case 10:
            return "LE";
        case 12:
            return "GE";
        default:
            return NULL;
    }
}



/*
 * BE CAREFUL!
 *
 * In case chunk param is NULL:
 * Returned structure had all strings malloced!!!
 * Be so kind and don't forget use free() for all its element, before end of
 * structure lifecycle.
 *
 * In case chunk is pointer to a GStringChunk:
 * Returned structure had all string inserted in the passed chunk.
 *
 */
cr_EVR *
cr_str_to_evr(const char *string, GStringChunk *chunk)
{
    cr_EVR *evr = g_new0(cr_EVR, 1);
    evr->epoch = NULL;
    evr->version = NULL;
    evr->release = NULL;

    if (!string || !(strlen(string))) {
        return evr;
    }

    const char *ptr;  // These names are totally self explaining
    const char *ptr2; //


    // Epoch
    gboolean bad_epoch = FALSE;

    ptr = strstr(string, ":");
    if (ptr) {
        // Check if epoch str is a number
        char *p = NULL;
        strtol(string, &p, 10);
        if (p == ptr) { // epoch str seems to be a number
            size_t len = ptr - string;
            if (len) {
                if (chunk) {
                    evr->epoch = g_string_chunk_insert_len(chunk, string, len);
                } else {
                    evr->epoch = g_strndup(string, len);
                }
            }
        } else { // Bad (non-numerical) epoch
            bad_epoch = TRUE;
        }
    } else { // There is no epoch
        ptr = (char*) string-1;
    }

    if (!evr->epoch && !bad_epoch) {
        if (chunk) {
            evr->epoch = g_string_chunk_insert_const(chunk, "0");
        } else {
            evr->epoch = g_strdup("0");
        }
    }


    // Version + release

    ptr2 = strstr(ptr+1, "-");
    if (ptr2) {
        // Version
        size_t version_len = ptr2 - (ptr+1);
        if (chunk) {
            evr->version = g_string_chunk_insert_len(chunk, ptr+1, version_len);
        } else {
            evr->version = g_strndup(ptr+1, version_len);
        }

        // Release
        size_t release_len = strlen(ptr2+1);
        if (release_len) {
            if (chunk) {
                evr->release = g_string_chunk_insert_len(chunk, ptr2+1,
                                                        release_len);
            } else {
                evr->release = g_strndup(ptr2+1, release_len);
            }
        }
    } else { // Release is not here, just version
        if (chunk) {
            evr->version = g_string_chunk_insert_const(chunk, ptr+1);
        } else {
            evr->version = g_strdup(ptr+1);
        }
    }

    return evr;
}

void
cr_evr_free(cr_EVR *evr)
{
    if (!evr)
        return;
    g_free(evr->epoch);
    g_free(evr->version);
    g_free(evr->release);
    g_free(evr);
}

/*
inline int
cr_is_primary(const char *filename)
{

    This optimal piece of code cannot be used because of yum...
    We must match any string that contains "bin/" in dirname

    Response to my question from packaging team:
    ....
    It must still contain that. Atm. it's defined as taking anything
    with 'bin/' in the path. The idea was that it'd match /usr/kerberos/bin/
    and /opt/blah/sbin. So that is what all versions of createrepo generate,
    and what yum all versions of yum expect to be generated.
    We can't change one side, without breaking the expectation of the
    other.
    There have been plans to change the repodata, and one of the changes
    would almost certainly be in how files are represented ... likely via.
    lists of "known" paths, that can be computed at createrepo time.

    if (!strncmp(filename, "/bin/", 5)) {
        return 1;
    }

    if (!strncmp(filename, "/sbin/", 6)) {
        return 1;
    }

    if (!strncmp(filename, "/etc/", 5)) {
        return 1;
    }

    if (!strncmp(filename, "/usr/", 5)) {
        if (!strncmp(filename+5, "bin/", 4)) {
            return 1;
        }

        if (!strncmp(filename+5, "sbin/", 5)) {
            return 1;
        }

        if (!strcmp(filename+5, "lib/sendmail")) {
            return 1;
        }
    }


    if (!strncmp(filename, "/etc/", 5)) {
        return 1;
    }

    if (!strcmp(filename, "/usr/lib/sendmail")) {
        return 1;
    }

    if (strstr(filename, "bin/")) {
        return 1;
    }

    return 0;
}
*/

#define VAL_LEN         4       // Len of numeric values in rpm

struct cr_HeaderRangeStruct
cr_get_header_byte_range(const char *filename, GError **err)
{
    /* Values readed by fread are 4 bytes long and stored as big-endian.
     * So there is htonl function to convert this big-endian number into host
     * byte order.
     */

    struct cr_HeaderRangeStruct results;

    assert(!err || *err == NULL);

    results.start = 0;
    results.end   = 0;


    // Open file

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        g_debug("%s: Cannot open file %s (%s)", __func__, filename,
                g_strerror(errno));
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot open %s: %s", filename, g_strerror(errno));
        return results;
    }


    // Get header range

    if (fseek(fp, 104, SEEK_SET) != 0) {
        g_debug("%s: fseek fail on %s (%s)", __func__, filename,
                g_strerror(errno));
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot seek over %s: %s", filename, g_strerror(errno));
        fclose(fp);
        return results;
    }

    unsigned int sigindex = 0;
    unsigned int sigdata  = 0;
    if (fread(&sigindex, VAL_LEN, 1, fp) != 1) {
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "fread() error on %s: %s", filename, g_strerror(errno));
        fclose(fp);
        return results;
    }
    sigindex = htonl(sigindex);
    if (fread(&sigdata, VAL_LEN, 1, fp) != 1) {
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "fread() error on %s: %s", filename, g_strerror(errno));
        fclose(fp);
        return results;
    }
    sigdata = htonl(sigdata);

    unsigned int sigindexsize = sigindex * 16;
    unsigned int sigsize = sigdata + sigindexsize;
    unsigned int disttoboundary = sigsize % 8;
    if (disttoboundary) {
        disttoboundary = 8 - disttoboundary;
    }
    unsigned int hdrstart = 112 + sigsize + disttoboundary;

    fseek(fp, hdrstart, SEEK_SET);
    fseek(fp, 8, SEEK_CUR);

    unsigned int hdrindex = 0;
    unsigned int hdrdata  = 0;
    if (fread(&hdrindex, VAL_LEN, 1, fp) != 1) {
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "fread() error on %s: %s", filename, g_strerror(errno));
        fclose(fp);
        return results;
    }
    hdrindex = htonl(hdrindex);
    if (fread(&hdrdata, VAL_LEN, 1, fp) != 1) {
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "fread() error on %s: %s", filename, g_strerror(errno));
        fclose(fp);
        return results;
    }
    hdrdata = htonl(hdrdata);
    unsigned int hdrindexsize = hdrindex * 16;
    unsigned int hdrsize = hdrdata + hdrindexsize + 16;
    unsigned int hdrend = hdrstart + hdrsize;

    fclose(fp);


    // Check sanity

    if (hdrend < hdrstart) {
        g_debug("%s: sanity check fail on %s (%d > %d))", __func__,
                filename, hdrstart, hdrend);
        g_set_error(err, ERR_DOMAIN, CRE_ERROR,
                    "sanity check error on %s (hdrstart: %d > hdrend: %d)",
                    filename, hdrstart, hdrend);
        return results;
    }

    results.start = hdrstart;
    results.end   = hdrend;

    return results;
}

char *
cr_get_filename(const char *filepath)
{
    char *filename;

    if (!filepath)
        return NULL;

    filename = (char *) filepath;
    size_t x = 0;

    while (filepath[x] != '\0') {
        if (filepath[x] == '/') {
            filename = (char *) filepath+(x+1);
        }
        x++;
    }

    return filename;
}

char *
cr_get_cleaned_href(const char *filepath)
{
    char *filename;

    if (!filepath)
        return NULL;

    filename = (char *) filepath;

    while (filename[0] == '.' && filename[1] == '/')
        filename += 2;

    return filename;
}

gboolean
cr_copy_file(const char *src, const char *in_dst, GError **err)
{
    size_t readed;
    char buf[BUFFER_SIZE];
    _cleanup_free_ gchar *dst = NULL;
    _cleanup_file_fclose_ FILE *orig = NULL;
    _cleanup_file_fclose_ FILE *new  = NULL;

    assert(src);
    assert(in_dst);
    assert(!err || *err == NULL);

    // If destination is dir use filename from src
    if (g_str_has_suffix(in_dst, "/"))
        dst = g_strconcat(in_dst, cr_get_filename(src), NULL);
    else
        dst = g_strdup(in_dst);

    // Open src file
    if ((orig = fopen(src, "rb")) == NULL) {
        g_debug("%s: Cannot open source file %s (%s)", __func__, src,
                g_strerror(errno));
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot open file %s: %s", src, g_strerror(errno));
        return FALSE;
    }

    // Open dst file
    if ((new = fopen(dst, "wb")) == NULL) {
        g_debug("%s: Cannot open destination file %s (%s)", __func__, dst,
                g_strerror(errno));
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot open file %s: %s", dst, g_strerror(errno));
        return FALSE;
    }

    // Copy content from src -> dst
    while ((readed = fread(buf, 1, BUFFER_SIZE, orig)) > 0) {
        if (readed != BUFFER_SIZE && ferror(orig)) {
            g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Error while read %s: %s", src, g_strerror(errno));
            return FALSE;
        }

        if (fwrite(buf, 1, readed, new) != readed) {
            g_debug("%s: Error while copy %s -> %s (%s)", __func__, src,
                    dst, g_strerror(errno));
            g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Error while write %s: %s", dst, g_strerror(errno));
            return FALSE;
        }
    }

    return TRUE;
}



int
cr_compress_file_with_stat(const char *src,
                           const char *in_dst,
                           cr_CompressionType compression,
                           cr_ContentStat *stat,
                           const char *zck_dict_dir,
                           gboolean zck_auto_chunk,
                           GError **err)
{
    int ret = CRE_OK;
    int readed;
    char buf[BUFFER_SIZE];
    CR_FILE *orig = NULL;
    CR_FILE *new = NULL;
    gchar *dst = (gchar *) in_dst;
    GError *tmp_err = NULL;

    assert(src);
    assert(!err || *err == NULL);

    const char *c_suffix = cr_compression_suffix(compression);

    // Src must be a file NOT a directory
    if (!g_file_test(src, G_FILE_TEST_IS_REGULAR)) {
        g_debug("%s: Source (%s) must be a regular file!", __func__, src);
        g_set_error(err, ERR_DOMAIN, CRE_NOFILE,
                    "Not a regular file: %s", src);
        return CRE_NOFILE;
    }

    if (!dst) {
        // If destination is NULL, use src + compression suffix
        dst = g_strconcat(src, c_suffix, NULL);
    } else if (g_str_has_suffix(dst, "/")) {
        // If destination is dir use filename from src + compression suffix
        dst = g_strconcat(dst, cr_get_filename(src), c_suffix, NULL);
    } else if (c_suffix && !g_str_has_suffix(dst, c_suffix)) {
        // If destination is missing compression suffix or has a different one, use specified compression suffix
        cr_CompressionType old_type = cr_detect_compression(dst, &tmp_err);
        if (tmp_err) {
            g_debug("%s: Unable to detect compression type of %s, using the filename as is.", __func__, dst);
            g_clear_error(&tmp_err);
        } else if (old_type == CR_CW_NO_COMPRESSION) {
            dst = g_strconcat(dst, c_suffix, NULL);
        } else {
            _cleanup_free_ gchar *tmp_file = g_strndup(dst, strlen(dst) - strlen(cr_compression_suffix(old_type)));
            dst = g_strconcat(tmp_file, c_suffix, NULL);
        }
    }

    int mode = CR_CW_AUTO_DETECT_COMPRESSION;

    orig = cr_open(src,
                   CR_CW_MODE_READ,
                   mode,
                   &tmp_err);
    if (!orig) {
        ret = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Cannot open %s: ", src);
        if (dst != in_dst)
            g_free(dst);
        return ret;
    }

    _cleanup_free_ gchar *dict = NULL;
    size_t dict_size = 0;
    if (compression == CR_CW_ZCK_COMPRESSION && zck_dict_dir) {
        /* Find zdict */
        _cleanup_free_ gchar *file_basename = NULL;
        if (dst) {
            _cleanup_free_ gchar *dict_base = NULL;
            if (g_str_has_suffix(dst, ".zck"))
                dict_base = g_strndup(dst, strlen(dst)-4);
            else
                dict_base = g_strdup(dst);
            file_basename = g_path_get_basename(dict_base);
        } else {
            file_basename = g_path_get_basename(src);
        }
        _cleanup_free_ gchar *dict_file = cr_get_dict_file(zck_dict_dir, file_basename);

        /* Read dictionary from file */
        if (dict_file && !g_file_get_contents(dict_file, &dict,
                                             &dict_size, &tmp_err)) {
            g_set_error(err, ERR_DOMAIN, CRE_IO,
                        "Error reading zchunk dict %s: %s",
                        dict_file, tmp_err->message);
            g_clear_error(&tmp_err);
            ret = CRE_IO;
            goto compress_file_cleanup;
        }
    }

    new = cr_sopen(dst, CR_CW_MODE_WRITE, compression, stat, &tmp_err);
    if (tmp_err) {
        g_debug("%s: Cannot open destination file %s", __func__, dst);
        g_propagate_prefixed_error(err, tmp_err, "Cannot open %s: ", dst);
        ret = CRE_IO;
        goto compress_file_cleanup;
    }
    if (compression == CR_CW_ZCK_COMPRESSION) {
        if (dict && cr_set_dict(new, dict, dict_size, &tmp_err) != CRE_OK) {
            ret = tmp_err->code;
            g_propagate_prefixed_error(err, tmp_err, "Unable to set zdict for %s: ", dst);
            goto compress_file_cleanup;
        }
        if (zck_auto_chunk && cr_set_autochunk(new, TRUE, &tmp_err) != CRE_OK) {
            ret = tmp_err->code;
            g_propagate_prefixed_error(err, tmp_err, "Unable to set auto-chunking for %s: ", dst);
            goto compress_file_cleanup;
        }
    }
    while ((readed = cr_read(orig, buf, BUFFER_SIZE, &tmp_err)) > 0) {
        cr_write(new, buf, readed, &tmp_err);
        if (tmp_err) {
            ret = tmp_err->code;
            g_propagate_prefixed_error(err, tmp_err, "Unable to write to %s: ", dst);
            goto compress_file_cleanup;
        }
    }

compress_file_cleanup:

    if (dst != in_dst)
        g_free(dst);

    if (orig)
        cr_close(orig, NULL);

    if (new)
        cr_close(new, NULL);

    return ret;
}


int
cr_decompress_file_with_stat(const char *src,
                             const char *in_dst,
                             cr_CompressionType compression,
                             cr_ContentStat *stat,
                             GError **err)
{
    int ret = CRE_OK;
    int readed;
    char buf[BUFFER_SIZE];
    FILE *new = NULL;
    CR_FILE *orig = NULL;
    gchar *dst = (gchar *) in_dst;
    GError *tmp_err = NULL;

    assert(src);
    assert(!err || *err == NULL);

    // Src must be a file NOT a directory
    if (!g_file_test(src, G_FILE_TEST_IS_REGULAR)) {
        g_debug("%s: Source (%s) must be a regular file!", __func__, src);
        g_set_error(err, ERR_DOMAIN, CRE_NOFILE,
                    "Not a regular file: %s", src);
        return CRE_NOFILE;
    }

    if (compression == CR_CW_AUTO_DETECT_COMPRESSION ||
        compression == CR_CW_UNKNOWN_COMPRESSION)
    {
        compression = cr_detect_compression(src, NULL);
    }

    if (compression == CR_CW_UNKNOWN_COMPRESSION) {
        g_set_error(err, ERR_DOMAIN, CRE_UNKNOWNCOMPRESSION,
                    "Cannot detect compression type");
        return CRE_UNKNOWNCOMPRESSION;
    }

    const char *c_suffix = cr_compression_suffix(compression);

    if (!in_dst || g_str_has_suffix(in_dst, "/")) {
        char *filename = cr_get_filename(src);
        if (!filename) {
            g_debug("%s: Cannot get filename from: %s", __func__, src);
            g_set_error(err, ERR_DOMAIN, CRE_NOFILE,
                        "Cannot get filename from: %s", src);
            return CRE_NOFILE;
        }
        if (g_str_has_suffix(filename, c_suffix)) {
            filename = g_strndup(filename, strlen(filename) - strlen(c_suffix));
        } else {
            filename = g_strconcat(filename, ".decompressed", NULL);
        }

        if (!in_dst) {
            // in_dst is NULL, use same dir as src
            char *src_dir = g_strndup(src,
                                strlen(src) - strlen(cr_get_filename(src)));
            dst = g_strconcat(src_dir, filename, NULL);
            g_free(src_dir);
        } else {
            // in_dst is dir
            dst = g_strconcat(in_dst, filename, NULL);
        }

        g_free(filename);
    }

    orig = cr_sopen(src, CR_CW_MODE_READ, compression, stat, &tmp_err);
    if (orig == NULL) {
        g_debug("%s: Cannot open source file %s", __func__, src);
        g_propagate_prefixed_error(err, tmp_err, "Cannot open %s: ", src);
        ret = CRE_IO;
        goto compress_file_cleanup;
    }

    new = fopen(dst, "wb");
    if (!new) {
        g_debug("%s: Cannot open destination file %s (%s)",
                __func__, dst, g_strerror(errno));
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot open %s: %s", src, g_strerror(errno));
        ret = CRE_IO;
        goto compress_file_cleanup;
    }

    while ((readed = cr_read(orig, buf, BUFFER_SIZE, &tmp_err)) > 0) {
        if (tmp_err) {
            g_debug("%s: Error while copy %s -> %s (%s)", __func__, src,
                    dst, tmp_err->message);
            g_propagate_prefixed_error(err, tmp_err,
                                       "Error while read %s: ", src);
            ret = CRE_IO;
            goto compress_file_cleanup;
        }

        if (fwrite(buf, 1, readed, new) != (size_t) readed) {
            g_debug("%s: Error while copy %s -> %s (%s)",
                    __func__, src, dst, g_strerror(errno));
            g_set_error(err, ERR_DOMAIN, CRE_IO,
                        "Error while write %s: %s", dst, g_strerror(errno));
            ret = CRE_IO;
            goto compress_file_cleanup;
        }
    }

compress_file_cleanup:

    if (dst != in_dst)
        g_free(dst);

    if (orig)
        cr_close(orig, NULL);

    if (new)
        fclose(new);

    return ret;
}



int
cr_download(CURL *in_handle,
            const char *url,
            const char *in_dst,
            GError **err)
{
    CURL *handle = NULL;
    CURLcode rcode;
    char errorbuf[CURL_ERROR_SIZE];
    _cleanup_free_ gchar *dst = NULL;
    _cleanup_file_fclose_ FILE *file = NULL;

    assert(in_handle);
    assert(!err || *err == NULL);

    // If destination is dir use filename from src
    if (g_str_has_suffix(in_dst, "/"))
        dst = g_strconcat(in_dst, cr_get_filename(url), NULL);
    else if (g_file_test(in_dst, G_FILE_TEST_IS_DIR))
        dst = g_strconcat(in_dst, "/", cr_get_filename(url), NULL);
    else
        dst = g_strdup(in_dst);

    // Open dst file
    file = fopen(dst, "wb");
    if (!file) {
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot open %s: %s", dst, g_strerror(errno));
        remove(dst);
        return CRE_IO;
    }

    // Dup the input handle
    handle = curl_easy_duphandle(in_handle);

    // Set error buffer
    errorbuf[0] = '\0';
    rcode = curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errorbuf);
    if (rcode != CURLE_OK) {
        curl_easy_cleanup(handle);
        g_set_error(err, ERR_DOMAIN, CRE_CURL,
                    "curl_easy_setopt failed(CURLOPT_ERRORBUFFER): %s",
                    curl_easy_strerror(rcode));
        return CRE_CURL;
    }

    // Set URL
    rcode = curl_easy_setopt(handle, CURLOPT_URL, url);
    if (rcode != CURLE_OK) {
        curl_easy_cleanup(handle);
        g_set_error(err, ERR_DOMAIN, CRE_CURL,
                    "curl_easy_setopt failed(CURLOPT_URL): %s",
                    curl_easy_strerror(rcode));
        remove(dst);
        return CRE_CURL;
    }

    // Set output file descriptor
    rcode = curl_easy_setopt(handle, CURLOPT_WRITEDATA, file);
    if (rcode != CURLE_OK) {
        curl_easy_cleanup(handle);
        g_set_error(err, ERR_DOMAIN, CRE_CURL,
                    "curl_easy_setopt(CURLOPT_WRITEDATA) failed: %s",
                    curl_easy_strerror(rcode));
        remove(dst);
        return CRE_CURL;
    }

    // Download the file
    rcode = curl_easy_perform(handle);
    if (rcode != CURLE_OK) {
        curl_easy_cleanup(handle);
        g_set_error(err, ERR_DOMAIN, CRE_CURL,
                    "curl_easy_perform failed: %s: %s",
                    curl_easy_strerror(rcode), errorbuf);
        remove(dst);
        return CRE_CURL;
    }

    curl_easy_cleanup(handle);

    g_debug("%s: Successfully downloaded: %s", __func__, dst);

    return CRE_OK;
}



gboolean
cr_better_copy_file(const char *src, const char *in_dst, GError **err)
{
    GError *tmp_err = NULL;

    assert(!err || *err == NULL);

    if (!strstr(src, "://"))   // Probably local path
        return cr_copy_file(src, in_dst, err);

    CURL *handle = curl_easy_init();
    cr_download(handle, src, in_dst, &tmp_err);
    curl_easy_cleanup(handle);
    if (tmp_err) {
        g_debug("%s: Error while downloading %s: %s", __func__, src,
                tmp_err->message);
        g_propagate_prefixed_error(err, tmp_err,
                                   "Error while downloading %s: ", src);
        return FALSE;
    }

    return TRUE;
}


int
cr_remove_dir_cb(const char *fpath,
                 G_GNUC_UNUSED const struct stat *sb,
                 G_GNUC_UNUSED int typeflag,
                 G_GNUC_UNUSED struct FTW *ftwbuf)
{
    int rv = remove(fpath);
    if (rv)
        g_warning("%s: Cannot remove: %s: %s", __func__, fpath, g_strerror(errno));

    return rv;
}


int
cr_remove_dir(const char *path, GError **err)
{
    int ret;

    assert(!err || *err == NULL);

    ret = nftw(path, cr_remove_dir_cb, 64, FTW_DEPTH | FTW_PHYS);
    if (ret != 0) {
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot remove dir %s: %s", path, g_strerror(errno));
        return CRE_IO;
    }

    return CRE_OK;
}


// Return path with exactly one trailing '/'
char *
cr_normalize_dir_path(const char *path)
{
    char *normalized = NULL;

    if (!path)
        return normalized;

    int i = strlen(path);
    if (i == 0) {
        return g_strdup("./");
    }

    do { // Skip all trailing '/'
        i--;
    } while (i >= 0 && path[i] == '/');

    normalized = g_strndup(path, i+2);
    if (normalized[i+1] != '/') {
        normalized[i+1] = '/';
    }

    return normalized;
}


struct cr_Version
cr_str_to_version(const char *str)
{
    char *endptr;
    const char *ptr = str;
    struct cr_Version ver;
    ver.major  = 0;
    ver.minor  = 0;
    ver.patch  = 0;
    ver.suffix = NULL;

    if (!str || str[0] == '\0') {
        return ver;
    }


    // Major chunk

    ver.major = strtol(ptr, &endptr, 10);
    if (!endptr || endptr[0] == '\0') {
        // Whole string has been converted successfully
        return ver;
    } else {
        if (endptr[0] == '.') {
            // '.' is supposed to be delimiter -> skip it and go to next chunk
            ptr = endptr+1;
        } else {
            ver.suffix = g_strdup(endptr);
            return ver;
        }
    }


    // Minor chunk

    ver.minor = strtol(ptr, &endptr, 10);
    if (!endptr || endptr[0] == '\0') {
        // Whole string has been converted successfully
        return ver;
    } else {
        if (endptr[0] == '.') {
            // '.' is supposed to be delimiter -> skip it and go to next chunk
            ptr = endptr+1;
        } else {
            ver.suffix = g_strdup(endptr);
            return ver;
        }
    }


    // Patch chunk

    ver.patch = strtol(ptr, &endptr, 10);
    if (!endptr || endptr[0] == '\0') {
        // Whole string has been converted successfully
        return ver;
    } else {
        if (endptr[0] != '.') { // '.' is supposed to be delimiter
            ver.suffix = g_strdup(endptr);
            return ver;
        }
    }

    return ver;
}

static int
cr_compare_values(const char *str1, const char *str2)
{
    if (!str1 && !str2)
        return 0;
    else if (str1 && !str2)
        return 1;
    else if (!str1 && str2)
        return -1;
    return rpmvercmp(str1, str2);
}

// Return values:
// 0 - versions are same
// 1 - first string is bigger version
// 2 - second string is bigger version
// Examples:
// "6.3.2azb" > "6.3.2abc"
// "2.1" < "2.1.3"
int
cr_cmp_version_str(const char* str1, const char *str2)
{
    int rc = cr_compare_values(str1 ? str1 : "", str2 ? str2 : "");
    if (rc == -1) rc = 2;
    return rc;
}


void
cr_null_log_fn(G_GNUC_UNUSED const gchar *log_domain,
               G_GNUC_UNUSED GLogLevelFlags log_level,
               G_GNUC_UNUSED const gchar *message,
               G_GNUC_UNUSED gpointer user_data)
{
    return;
}


void
cr_log_fn(const gchar *log_domain,
          GLogLevelFlags log_level,
          const gchar *message,
          gpointer user_data)
{
    gint hidden_log_levels = GPOINTER_TO_INT(user_data);
    if (log_level & hidden_log_levels)
        return;
    switch(log_level) {
        case G_LOG_LEVEL_ERROR:
            if (log_domain) g_printerr("%s: ", log_domain);
            g_printerr("Error: %s\n", message);
            break;
        case G_LOG_LEVEL_CRITICAL:
            if (log_domain) g_printerr("%s: ", log_domain);
            g_printerr("Critical: %s\n", message);
            break;
        case G_LOG_LEVEL_WARNING:
            if (log_domain) g_printerr("%s: ", log_domain);
            g_printerr("Warning: %s\n", message);
            break;
        case G_LOG_LEVEL_DEBUG: {
            time_t rawtime;
            struct tm * timeinfo;
            char buffer[80];

            time ( &rawtime );
            timeinfo = localtime ( &rawtime );
            strftime (buffer, 80, "%H:%M:%S", timeinfo);

            //if (log_domain) g_printerr("%s: ", log_domain);
            g_printerr("%s: %s\n", buffer, message);
            break;
        }
        default:
            printf("%s\n", message);
    }

    return;
}


void
cr_slist_free_full(GSList *list, GDestroyNotify free_f)
{
    g_slist_free_full(list, free_f);
}


void cr_queue_free_full(GQueue *queue, GDestroyNotify free_f)
{
    g_queue_free_full(queue, free_f);
}


cr_NEVRA *
cr_split_rpm_filename(const char *filename)
{
    cr_NEVRA *nevra = NULL;
    gchar *str, *epoch = NULL;
    size_t len;

    filename = cr_get_filename(filename);

    if (!filename)
        return NULL;

    str = g_strdup(filename);

    // N-V-R.rpm:E
    if (strchr(str, ':')) {
        gchar **filename_epoch = g_strsplit(str, ":", 2);
        if (g_str_has_suffix(filename_epoch[0], ".rpm")) {
            g_free(str);
            str = filename_epoch[0];
            epoch = filename_epoch[1];
        } else {
            g_strfreev(filename_epoch);
        }
    }

    len = strlen(str);

    // Get rid off .rpm suffix
    if (len >= 4 && !strcmp(str+(len-4), ".rpm")) {
        len -= 4;
        str[len] = '\0';
    }

    nevra = cr_str_to_nevra(str);
    g_free(str);

    if (epoch) {
        g_free(nevra->epoch);
        nevra->epoch = epoch;
    }

    return nevra;
}

/** Split N-V-R:E or E:N-V-R or N-E:V-R
 */
cr_NEVR *
cr_str_to_nevr(const char *instr)
{
    gchar *nvr = NULL;
    gchar *epoch = NULL;
    gchar **nvr_epoch_list = NULL;
    cr_NEVR *nevr = NULL;
    size_t len;
    int i;

    if (!instr)
        return NULL;

    // 1)
    // Try to split by ':'
    // If we have N-V-R:E or E:N-V-R then nvr and epoch will be filled
    // If we have N-E:V-R or N-V-R then only nvr will be filed

    nvr_epoch_list = g_strsplit(instr, ":", 2);

    if (!nvr_epoch_list || !(*nvr_epoch_list)) {
        g_strfreev(nvr_epoch_list);
        return NULL;
    }

    nvr     = nvr_epoch_list[0];
    epoch   = nvr_epoch_list[1];  // May be NULL

    if (epoch && strchr(epoch, '-')) {
        if (!strchr(nvr, '-')) {
            // Switch nvr and epoch
            char *tmp = nvr;
            nvr = epoch;
            epoch = tmp;
        } else {
            // Probably the N-E:V-R format, handle it after the split
            g_free(nvr);
            g_free(epoch);
            nvr = g_strdup(instr);
            epoch = NULL;
        }
    }

    g_free(nvr_epoch_list);

    // 2)
    // Now split the nvr by the '-' into three parts

    nevr = g_new0(cr_NEVR, 1);
    len = strlen(nvr);

    // Get release
    for (i = len-1; i >= 0; i--)
        if (nvr[i] == '-') {
            nevr->release = g_strdup(nvr+i+1);
            nvr[i] = '\0';
            len = i;
            break;
        }

    // Get version
    for (i = len-1; i >= 0; i--)
        if (nvr[i] == '-') {
            nevr->version = g_strdup(nvr+i+1);
            nvr[i] = '\0';
            break;
        }

    // Get name
    nevr->name = g_strdup(nvr);

    g_free(nvr);

    // 3)
    // Now split the E:V

    if (epoch == NULL && (nevr->version && strchr(nevr->version, ':'))) {
        gchar **epoch_version = g_strsplit(nevr->version, ":", 2);
        g_free(nevr->version);
        nevr->epoch = epoch_version[0];
        nevr->version = epoch_version[1];
        g_free(epoch_version);
    } else {
        nevr->epoch = epoch;
    }

    return nevr;
}

void
cr_nevr_free(cr_NEVR *nevr)
{
    if (!nevr)
        return;
    g_free(nevr->name);
    g_free(nevr->epoch);
    g_free(nevr->version);
    g_free(nevr->release);
    g_free(nevr);
}


cr_NEVRA *
cr_str_to_nevra(const char *instr)
{
    cr_NEVR *nevr;
    cr_NEVRA *nevra = NULL;
    gchar *str, *epoch = NULL;
    int i;

    if (!instr)
        return NULL;

    nevra = g_new0(cr_NEVRA, 1);
    str = g_strdup(instr);

    // N-V-R.A:E
    if (strchr(str, ':')) {
        gchar **nvra_epoch = g_strsplit(str, ":", 2);
        char *epoch_candidate = nvra_epoch[1];
        if (epoch_candidate
            && !strchr(epoch_candidate, '-')
            && !strchr(epoch_candidate, '.'))
        {
            // Strip epoch from the very end
            epoch = epoch_candidate;
            g_free(str);
            str = nvra_epoch[0];
        } else {
            g_strfreev(nvra_epoch);
        }
    }

    // Get arch
    for (i = strlen(str)-1; i >= 0; i--)
        if (str[i] == '.') {
            nevra->arch = g_strdup(str+i+1);
            str[i] = '\0';
            break;
        }

    if (nevra->arch && strchr(nevra->arch, '-')) {
        g_warning("Invalid arch %s", nevra->arch);
        cr_nevra_free(nevra);
        g_free(str);
        return NULL;
    }

    nevr = cr_str_to_nevr(str);
    nevra->name     = nevr->name;
    nevra->epoch    = nevr->epoch;
    nevra->version  = nevr->version;
    nevra->release  = nevr->release;
    g_free(nevr);

    g_free(str);

    if (epoch) {
        g_free(nevra->epoch);
        nevra->epoch = epoch;
    }

    return nevra;
}


void
cr_nevra_free(cr_NEVRA *nevra)
{
    if (!nevra)
        return;
    g_free(nevra->name);
    g_free(nevra->epoch);
    g_free(nevra->version);
    g_free(nevra->release);
    g_free(nevra->arch);
    g_free(nevra);
}

int
cr_cmp_evr(const char *e1, const char *v1, const char *r1,
           const char *e2, const char *v2, const char *r2)
{
    int rc;

    if (e1 == NULL) e1 = "0";
    if (e2 == NULL) e2 = "0";

    rc = cr_compare_values(e1, e2);
    if (rc) return rc;
    rc = cr_compare_values(v1, v2);
    if (rc) return rc;
    rc = cr_compare_values(r1, r2);
    return rc;
}

int
cr_warning_cb(G_GNUC_UNUSED cr_XmlParserWarningType type,
              char *msg,
              void *cbdata,
              G_GNUC_UNUSED GError **err)
{
    g_warning("%s: %s", (char *) cbdata, msg);

    return CR_CB_RET_OK;
}

gboolean
cr_write_to_file(GError **err, gchar *filename, const char *format, ...)
{
    assert(filename);
    assert(!err || *err == NULL);

    if (!format)
        return TRUE;

    FILE *f = fopen(filename, "w");
    if (!f) {
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot open %s: %s", filename, g_strerror(errno));
        return FALSE;
    }

    va_list args;
    va_start(args, format);
    vfprintf (f, format, args);
    va_end(args);

    gboolean ret = TRUE;

    if (ferror(f)) {
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot write content to %s: %s",
                    filename, g_strerror(errno));
        ret = FALSE;
    }

    fclose(f);

    return ret;
}

static gboolean
cr_run_command(char **cmd, const char *working_dir, GError **err)
{
    assert(cmd);
    assert(!err || *err == NULL);

    GError *tmp_err = NULL;
    gint status = 0;
    gchar *error_str = NULL;
    int spawn_flags = G_SPAWN_SEARCH_PATH
                      | G_SPAWN_STDOUT_TO_DEV_NULL;

    g_spawn_sync(working_dir,
                 cmd,
                 NULL, // envp
                 spawn_flags,
                 NULL, // child setup function
                 NULL, // user data for child setup
                 NULL, // stdout
                 &error_str, // stderr
                 &status,
                 &tmp_err);

    if (tmp_err) {
        g_free(error_str);
        g_propagate_error(err, tmp_err);
        return FALSE;
    }

    gboolean ret = cr_spawn_check_exit_status(status, &tmp_err);
    if (!ret && error_str) {
        // Remove newlines from error message
        for (char *ptr = error_str; *ptr; ptr++)
            if (*ptr == '\n') *ptr = ' ';

        g_propagate_prefixed_error(err, tmp_err, "%s: ", error_str);
    }

    g_free(error_str);

    return ret;
}

gboolean
cr_cp(const char *src,
      const char *dst,
      cr_CpFlags flags,
      const char *working_dir,
      GError **err)
{
    assert(src);
    assert(dst);
    assert(!err || *err == NULL);

    GPtrArray *argv_array = g_ptr_array_new();
    g_ptr_array_add(argv_array, "cp");
    if (flags & CR_CP_RECURSIVE)
        g_ptr_array_add(argv_array, "-r");
    if (flags & CR_CP_PRESERVE_ALL)
        g_ptr_array_add(argv_array, "--preserve=all");
    g_ptr_array_add(argv_array, (char *) src);
    g_ptr_array_add(argv_array, (char *) dst);
    g_ptr_array_add(argv_array, (char *) NULL);

    gboolean ret = cr_run_command((char **) argv_array->pdata,
                                  working_dir,
                                  err);

    g_ptr_array_free(argv_array, TRUE);

    return ret;
}

gboolean
cr_rm(const char *path,
      cr_RmFlags flags,
      const char *working_dir,
      GError **err)
{
    assert(path);
    assert(!err || *err == NULL);

    GPtrArray *argv_array = g_ptr_array_new();
    g_ptr_array_add(argv_array, "rm");
    if (flags & CR_RM_RECURSIVE)
        g_ptr_array_add(argv_array, "-r");
    if (flags & CR_RM_FORCE)
        g_ptr_array_add(argv_array, "-f");
    g_ptr_array_add(argv_array, (char *) path);
    g_ptr_array_add(argv_array, (char *) NULL);

    gboolean ret = cr_run_command((char **) argv_array->pdata,
                                  working_dir,
                                  err);

    g_ptr_array_free(argv_array, TRUE);

    return ret;
}

gchar *
cr_append_pid_and_datetime(const char *str, const char *suffix)
{
    struct tm * timeinfo;
    struct timeval tv;
    char datetime[80];

    gettimeofday(&tv, NULL);
    timeinfo = localtime (&(tv.tv_sec));
    strftime(datetime, 80, "%Y%m%d%H%M%S", timeinfo);
    gchar *result = g_strdup_printf("%s%jd.%s.%ld%s",
                                    str ? str : "",
                                    (intmax_t) getpid(),
                                    datetime,
                                    tv.tv_usec,
                                    suffix ? suffix : "");
    return result;
}

gboolean
cr_spawn_check_exit_status(gint exit_status, GError **err)
{
    assert(!err || *err == NULL);

    if (WIFEXITED(exit_status)) {
        if (WEXITSTATUS(exit_status) == 0) {
            // Exit code == 0 means success
            return TRUE;
        } else {
            g_set_error (err, ERR_DOMAIN, CRE_SPAWNERRCODE,
                        "Child process exited with code %ld",
                        (long) WEXITSTATUS(exit_status));
        }
    } else if (WIFSIGNALED(exit_status)) {
        g_set_error (err, ERR_DOMAIN, CRE_SPAWNKILLED,
                     "Child process killed by signal %ld",
                     (long) WTERMSIG(exit_status));
    } else if (WIFSTOPPED(exit_status)) {
        g_set_error (err, ERR_DOMAIN, CRE_SPAWNSTOPED,
                     "Child process stopped by signal %ld",
                     (long) WSTOPSIG(exit_status));
    } else {
        g_set_error (err, ERR_DOMAIN, CRE_SPAWNABNORMAL,
                     "Child process exited abnormally");
    }

    return FALSE;
}

gboolean
cr_identical_files(const gchar *fn1,
                   const gchar *fn2,
                   gboolean *identical,
                   GError **err)
{
    int rc;
    GStatBuf buf1, buf2;

    *identical = FALSE;

    // Stat old file
    rc = g_stat(fn1, &buf1);
    if (rc == -1) {
        if (errno == ENOENT) // The first file doesn't exist
            return TRUE;

        g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                    "Cannot stat %s: %s", fn1, g_strerror(errno));
        return FALSE;
    }

    // Stat new file
    rc = g_stat(fn2, &buf2);
    if (rc == -1) {
        if (errno == ENOENT) // The second file doesn't exist
            return TRUE;

        g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                    "Cannot stat %s: %s", fn2, g_strerror(errno));
        return FALSE;
    }

    // Check if both paths point to the same file
    if (buf1.st_ino == buf2.st_ino)
        *identical = TRUE;

    return TRUE;
}

gchar *
cr_cut_dirs(gchar *path, gint cut_dirs)
{
    if (!path)
        return NULL;

    if (cut_dirs < 1)
        return path;

    gchar *last_component = NULL;
    for (gchar *p = path; *p; p++) {
        if (*p == '/')
            last_component = p;
    }

    if (last_component == NULL)
        return path;

    gchar *cut = path;
    gint n = 0;
    gint state = 0;

    for (gchar *p = path; p <= last_component; p++) {
        if (state == 0) {
            if (*p == '/') {
                cut = p;
            } else {
                state = 1;
                if (n == cut_dirs)
                    break;
            }
        } else if (state == 1) {
            if (*p == '/') {
                cut = p;
                state = 0;
                n++;
            }
        }
    }

    return cut+1;
}

const gchar *
cr_version_string_with_features(void)
{
    return (xstr(CR_VERSION_MAJOR)
            "."
            xstr(CR_VERSION_MINOR)
            "."
            xstr(CR_VERSION_PATCH)
            " (Features: "
#ifdef CR_DELTA_RPM_SUPPORT
            "DeltaRPM "
#endif
#ifdef ENABLE_LEGACY_WEAKDEPS
            "LegacyWeakdeps "
#endif
#ifdef ENABLE_THREADED_XZ_ENCODER
            "ThreadedXzEncoder "
#endif
            ")");
}

gchar *
cr_get_dict_file(const gchar *dir, const gchar *file)
{
    gchar *dict_file = malloc(strlen(file) + 7);
    assert(dict_file);

    snprintf(dict_file, strlen(file) + 7, "%s.zdict", file);
    gchar *full_path = g_build_path("/", dir, dict_file, NULL);
    assert(full_path);

    free(dict_file);
    if (!g_file_test(full_path, G_FILE_TEST_EXISTS)) {
        g_warning("%s: Zchunk dict %s doesn't exist", __func__, full_path);
        free(full_path);
        return NULL;
    }
    return full_path;
}
