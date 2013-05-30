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

#include <glib.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ftw.h>
#include <time.h>
#include <curl/curl.h>
#include <rpm/rpmlib.h>
#include "error.h"
#include "logging.h"
#include "constants.h"
#include "misc.h"

#define BUFFER_SIZE     4096


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
struct cr_EVR
cr_str_to_evr(const char *string, GStringChunk *chunk)
{
    struct cr_EVR evr;
    evr.epoch = NULL;
    evr.version = NULL;
    evr.release = NULL;

    if (!string || !(strlen(string))) {
        return evr;
    }

    const char *ptr;  // These names are totally self explaining
    const char *ptr2; //


    // Epoch

    ptr = strstr(string, ":");
    if (ptr) {
        // Check if epoch str is a number
        char *p = NULL;
        strtol(string, &p, 10);
        if (p == ptr) { // epoch str seems to be a number
            size_t len = ptr - string;
            if (len) {
                if (chunk) {
                    evr.epoch = g_string_chunk_insert_len(chunk, string, len);
                } else {
                    evr.epoch = g_strndup(string, len);
                }
            }
        }
    } else { // There is no epoch
        ptr = (char*) string-1;
    }

    if (!evr.epoch) {
        if (chunk) {
            evr.epoch = g_string_chunk_insert_const(chunk, "0");
        } else {
            evr.epoch = g_strdup("0");
        }
    }


    // Version + release

    ptr2 = strstr(ptr+1, "-");
    if (ptr2) {
        // Version
        size_t version_len = ptr2 - (ptr+1);
        if (chunk) {
            evr.version = g_string_chunk_insert_len(chunk, ptr+1, version_len);
        } else {
            evr.version = g_strndup(ptr+1, version_len);
        }

        // Release
        size_t release_len = strlen(ptr2+1);
        if (release_len) {
            if (chunk) {
                evr.release = g_string_chunk_insert_len(chunk, ptr2+1,
                                                        release_len);
            } else {
                evr.release = g_strndup(ptr2+1, release_len);
            }
        }
    } else { // Release is not here, just version
        if (chunk) {
            evr.version = g_string_chunk_insert_const(chunk, ptr+1);
        } else {
            evr.version = g_strdup(ptr+1);
        }
    }

    return evr;
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


char *
cr_compute_file_checksum(const char *filename,
                         cr_ChecksumType type,
                         GError **err)
{
    GChecksumType gchecksumtype;

    assert(filename);
    assert(!err || *err == NULL);

    // Check if file exists and if it is a regular file (not a directory)

    if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        g_debug("%s: File %s doesn't exists or not a regular file",
                __func__, filename);
        g_set_error(err, CR_CHECKSUM_ERROR, CRE_NOFILE,
                    "File %s doesn't exists or not a regular file", filename);
        return NULL;
    }


    // Convert our checksum type into glib type

    switch (type) {
        case CR_CHECKSUM_MD5:
            gchecksumtype = G_CHECKSUM_MD5;
            break;
        case CR_CHECKSUM_SHA1:
            gchecksumtype = G_CHECKSUM_SHA1;
            break;
        case CR_CHECKSUM_SHA256:
            gchecksumtype = G_CHECKSUM_SHA256;
            break;
        default:
            g_debug("%s: Unknown checksum type", __func__);
            g_set_error(err, CR_CHECKSUM_ERROR, CRE_UNKNOWNCHECKSUMTYPE,
                        "Unknown checksum type: %d", type);
            return NULL;
    };


    // Open file and initialize checksum structure

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        g_critical("%s: Cannot open %s (%s)", __func__, filename,
                                                    strerror(errno));
        g_set_error(err, CR_CHECKSUM_ERROR, CRE_IO,
                    "Cannot open %s: %s", filename, strerror(errno));
        return NULL;
    }


    // Calculate checksum

    GChecksum *checksum = g_checksum_new(gchecksumtype);
    unsigned char buffer[BUFFER_SIZE];

    while (1) {
        size_t input_len;
        input_len = fread((void *) buffer, sizeof(unsigned char),
                          BUFFER_SIZE, fp);
        g_checksum_update(checksum, (const guchar *) buffer, input_len);
        if (input_len < BUFFER_SIZE) {
            break;
        }
    }

    if (ferror(fp)) {
        g_set_error(err, CR_CHECKSUM_ERROR, CRE_IO,
                    "fread call faied: %s", strerror(errno));
        fclose(fp);
        g_checksum_free(checksum);
        return NULL;
    }

    fclose(fp);


    // Get checksum

    char *checksum_str = g_strdup(g_checksum_get_string(checksum));
    g_checksum_free(checksum);

    if (!checksum_str) {
        g_critical("%s: Cannot get checksum %s (low memory?)", __func__,
                   filename);
        g_set_error(err, CR_CHECKSUM_ERROR, CRE_MEMORY,
                    "Cannot calculate checksum (low memory?)");
    }

    return checksum_str;
}



#define VAL_LEN         4       // Len of numeric values in rpm

struct cr_HeaderRangeStruct
cr_get_header_byte_range(const char *filename)
{
    /* Values readed by fread are 4 bytes long and stored as big-endian.
     * So there is htonl function to convert this big-endian number into host
     * byte order.
     */

    struct cr_HeaderRangeStruct results;

    results.start = 0;
    results.end   = 0;


    // Open file

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        g_debug("%s: Cannot open file %s (%s)", __func__, filename,
                strerror(errno));
        return results;
    }


    // Get header range

    if (fseek(fp, 104, SEEK_SET) != 0) {
        g_debug("%s: fseek fail on %s (%s)", __func__, filename,
                strerror(errno));
        fclose(fp);
        return results;
    }

    unsigned int sigindex = 0;
    unsigned int sigdata  = 0;
    fread(&sigindex, VAL_LEN, 1, fp);
    sigindex = htonl(sigindex);
    fread(&sigdata, VAL_LEN, 1, fp);
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
    fread(&hdrindex, VAL_LEN, 1, fp);
    hdrindex = htonl(hdrindex);
    fread(&hdrdata, VAL_LEN, 1, fp);
    hdrdata = htonl(hdrdata);
    unsigned int hdrindexsize = hdrindex * 16;
    unsigned int hdrsize = hdrdata + hdrindexsize + 16;
    unsigned int hdrend = hdrstart + hdrsize;

    fclose(fp);


    // Check sanity

    if (hdrend < hdrstart) {
        g_debug("%s: sanity check fail on %s (%d > %d))", __func__,
                filename, hdrstart, hdrend);
        return results;
    }

    results.start = hdrstart;
    results.end   = hdrend;

    return results;
}


const char *
cr_checksum_name_str(cr_ChecksumType type)
{
    char *name = NULL;

    switch (type) {
        case CR_CHECKSUM_MD5:
            name = "md5";
            break;
        case CR_CHECKSUM_SHA1:
            name = "sha";
            break;
        case CR_CHECKSUM_SHA256:
            name = "sha256";
            break;
        default:
            g_debug("%s: Unknown checksum (%d)", __func__, type);
            break;
    }

    return name;
}


char *
cr_get_filename(const char *filepath)
{
    char *filename = NULL;

    if (!filepath)
        return filename;

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


int
cr_copy_file(const char *src, const char *in_dst)
{
    size_t readed;
    char buf[BUFFER_SIZE];

    FILE *orig;
    FILE *new;

    if (!src || !in_dst) {
        g_debug("%s: File name cannot be NULL", __func__);
        return CR_COPY_ERR;
    }

    // If destination is dir use filename from src
    gchar *dst = (gchar *) in_dst;
    if (g_str_has_suffix(in_dst, "/")) {
        dst = g_strconcat(in_dst, cr_get_filename(src), NULL);
    }

    if ((orig = fopen(src, "r")) == NULL) {
        g_debug("%s: Cannot open source file %s (%s)", __func__, src,
                strerror(errno));
        return CR_COPY_ERR;
    }

    if ((new = fopen(dst, "w")) == NULL) {
        g_debug("%s: Cannot open destination file %s (%s)", __func__, dst,
                strerror(errno));
        fclose(orig);
        return CR_COPY_ERR;
    }

    while ((readed = fread(buf, 1, BUFFER_SIZE, orig)) > 0) {
        if (fwrite(buf, 1, readed, new) != readed) {
            g_debug("%s: Error while copy %s -> %s (%s)", __func__, src,
                    dst, strerror(errno));
            fclose(new);
            fclose(orig);
            return CR_COPY_ERR;
        }

        if (readed != BUFFER_SIZE && ferror(orig)) {
            g_debug("%s: Error while copy %s -> %s (%s)", __func__, src,
                    dst, strerror(errno));
            fclose(new);
            fclose(orig);
            return CR_COPY_ERR;
        }
    }

    if (dst != in_dst) {
        g_free(dst);
    }

    fclose(new);
    fclose(orig);

    return CR_COPY_OK;
}



int
cr_compress_file(const char *src,
                 const char *in_dst,
                 cr_CompressionType compression)
{
    int readed;
    char buf[BUFFER_SIZE];

    FILE *orig;
    CR_FILE *new;

    if (!src) {
        g_debug("%s: File name cannot be NULL", __func__);
        return CR_COPY_ERR;
    }

    if (compression == CR_CW_AUTO_DETECT_COMPRESSION ||
        compression == CR_CW_UNKNOWN_COMPRESSION) {
        g_debug("%s: Bad compression type", __func__);
        return CR_COPY_ERR;
    }

    // Src must be a file NOT a directory
    if (!g_file_test(src, G_FILE_TEST_IS_REGULAR)) {
        g_debug("%s: Source (%s) must be directory!", __func__, src);
        return CR_COPY_ERR;
    }

    gchar *dst = (gchar *) in_dst;
    if (!dst) {
        // If destination is NULL, use src + compression suffix
        const gchar *suffix = cr_compression_suffix(compression);
        dst = g_strconcat(src, suffix, NULL);
    } else {
        // If destination is dir use filename from src + compression suffix
        if (g_str_has_suffix(in_dst, "/")) {
            const gchar *suffix = cr_compression_suffix(compression);
            dst = g_strconcat(in_dst, cr_get_filename(src), suffix, NULL);
        }
    }

    if ((orig = fopen(src, "r")) == NULL) {
        g_debug("%s: Cannot open source file %s (%s)", __func__, src,
                strerror(errno));
        return CR_COPY_ERR;
    }

    if ((new = cr_open(dst, CR_CW_MODE_WRITE, compression, NULL)) == NULL) {
        g_debug("%s: Cannot open destination file %s", __func__, dst);
        fclose(orig);
        return CR_COPY_ERR;
    }

    while ((readed = fread(buf, 1, BUFFER_SIZE, orig)) > 0) {
        if (cr_write(new, buf, readed, NULL) != readed) {
            g_debug("%s: Error while copy %s -> %s", __func__, src, dst);
            cr_close(new, NULL);
            fclose(orig);
            return CR_COPY_ERR;
        }

        if (readed != BUFFER_SIZE && ferror(orig)) {
            g_debug("%s: Error while copy %s -> %s (%s)", __func__, src,
                    dst, strerror(errno));
            cr_close(new, NULL);
            fclose(orig);
            return CR_COPY_ERR;
        }
    }

    if (dst != in_dst)
        g_free(dst);

    cr_close(new, NULL);
    fclose(orig);

    return CR_COPY_OK;
}



void
cr_download(CURL *handle, const char *url, const char *in_dst, char **error)
{
    CURLcode rcode;
    FILE *file = NULL;


    // If destination is dir use filename from src

    gchar *dst = NULL;
    if (g_str_has_suffix(in_dst, "/")) {
        dst = g_strconcat(in_dst, cr_get_filename(url), NULL);
    } else if (g_file_test(in_dst, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))) {
        dst = g_strconcat(in_dst, "/", cr_get_filename(url), NULL);
    } else {
        dst = g_strdup(in_dst);
    }

    file = fopen(dst, "w");
    if (!file) {
        *error = g_strdup_printf("%s: Cannot open %s", __func__, dst);
        remove(dst);
        g_free(dst);
        return;
    }


    // Set URL

    if (curl_easy_setopt(handle, CURLOPT_URL, url) != CURLE_OK) {
        *error = g_strdup_printf("%s: curl_easy_setopt(CURLOPT_URL) error",
                                 __func__);
        fclose(file);
        remove(dst);
        g_free(dst);
        return;
    }


    // Set output file descriptor

    if (curl_easy_setopt(handle, CURLOPT_WRITEDATA, file) != CURLE_OK) {
        *error = g_strdup_printf(
                        "%s: curl_easy_setopt(CURLOPT_WRITEDATA) error",
                         __func__);
        fclose(file);
        remove(dst);
        g_free(dst);
        return;
    }

    rcode = curl_easy_perform(handle);
    if (rcode != 0) {
        *error = g_strdup_printf("%s: curl_easy_perform() error: %s",
                                 __func__, curl_easy_strerror(rcode));
        fclose(file);
        remove(dst);
        g_free(dst);
        return;
    }


    g_debug("%s: Successfully downloaded: %s", __func__, dst);

    fclose(file);
    g_free(dst);
}



int
cr_better_copy_file(const char *src, const char *in_dst)
{
    if (!strstr(src, "://")) {
        // Probably local path
        return cr_copy_file(src, in_dst);
    }

    char *error = NULL;
    CURL *handle = curl_easy_init();
    cr_download(handle, src, in_dst, &error);
    curl_easy_cleanup(handle);
    if (error) {
        g_debug("%s: Error while downloading %s: %s", __func__, src,
                error);
        return CR_COPY_ERR;
    }

    return CR_COPY_OK;
}


int
cr_remove_dir_cb(const char *fpath,
                 const struct stat *sb,
                 int typeflag,
                 struct FTW *ftwbuf)
{
    CR_UNUSED(sb);
    CR_UNUSED(typeflag);
    CR_UNUSED(ftwbuf);
    int rv = remove(fpath);
    if (rv)
        g_warning("%s: Cannot remove: %s: %s", __func__, fpath, strerror(errno));

    return rv;
}


int
cr_remove_dir(const char *path)
{
    return nftw(path, cr_remove_dir_cb, 64, FTW_DEPTH | FTW_PHYS);
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
    ver.version = 0;
    ver.release = 0;
    ver.patch   = 0;
    ver.suffix  = NULL;

    if (!str || str[0] == '\0') {
        return ver;
    }


    // Version chunk

    ver.version = strtol(ptr, &endptr, 10);
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


    // Release chunk

    ver.release = strtol(ptr, &endptr, 10);
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
        if (endptr[0] == '.') {
            // '.' is supposed to be delimiter -> skip it and go to next chunk
            ptr = endptr+1;
        } else {
            ver.suffix = g_strdup(endptr);
            return ver;
        }
    }

    return ver;
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
    struct cr_Version ver1, ver2;

    if (!str1 && !str2) {
        return 0;
    }

    // Get version
    ver1 = cr_str_to_version(str1);
    ver2 = cr_str_to_version(str2);

    if (ver1.version > ver2.version) {
        return 1;
    } else if (ver1.version < ver2.version) {
        return 2;
    } else if (ver1.release > ver2.release) {
        return 1;
    } else if (ver1.release < ver2.release) {
        return 2;
    } else if (ver1.patch > ver2. patch) {
        return 1;
    } else if (ver1.patch < ver2.patch) {
        return 2;
    }

    int strcmp_res = g_strcmp0(ver1.suffix, ver2.suffix);

    g_free(ver1.suffix);
    g_free(ver2.suffix);

    if (strcmp_res > 0) {
        return 1;
    } else if (strcmp_res < 0) {
        return 2;
    }

    return 0;
}


void
cr_null_log_fn(const gchar *log_domain,
               GLogLevelFlags log_level,
               const gchar *message,
               gpointer user_data)
{
    CR_UNUSED(log_domain);
    CR_UNUSED(log_level);
    CR_UNUSED(message);
    CR_UNUSED(user_data);
    return;
}


void
cr_log_fn(const gchar *log_domain,
          GLogLevelFlags log_level,
          const gchar *message,
          gpointer user_data)
{
    CR_UNUSED(user_data);


    switch(log_level) {
        case G_LOG_LEVEL_ERROR:
            if (log_domain) fprintf(stderr, "%s: ", log_domain);
            fprintf(stderr, "Error: %s\n", message);
            break;
        case G_LOG_LEVEL_CRITICAL:
            if (log_domain) fprintf(stderr, "%s: ", log_domain);
            fprintf(stderr, "Critical: %s\n", message);
            break;
        case G_LOG_LEVEL_WARNING:
            if (log_domain) fprintf(stderr, "%s: ", log_domain);
            fprintf(stderr, "Warning: %s\n", message);
            break;
        case G_LOG_LEVEL_DEBUG: {
            time_t rawtime;
            struct tm * timeinfo;
            char buffer[80];

            time ( &rawtime );
            timeinfo = localtime ( &rawtime );
            strftime (buffer, 80, "%H:%M:%S", timeinfo);

            if (log_domain) fprintf(stderr, "%s: ", log_domain);
            fprintf(stderr, "%s: %s\n", buffer, message);
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
    g_slist_foreach (list, (GFunc) free_f, NULL);
    g_slist_free(list);
}


struct cr_NVREA *
cr_split_rpm_filename(const char *filename)
{
    struct cr_NVREA *res = NULL;
    gchar *str, *copy;
    size_t len;
    int i;

    if (!filename)
        return res;

    res = g_malloc0(sizeof(struct cr_NVREA));
    str = g_strdup(filename);
    copy = str;
    len = strlen(str);

    // Get rid off .rpm suffix
    if (len >= 4 && !strcmp(str+(len-4), ".rpm")) {
        len -= 4;
        str[len] = '\0';
    }

    // Get arch
    for (i = len-1; i >= 0; i--)
        if (str[i] == '.') {
            res->arch = g_strdup(str+i+1);
            str[i] = '\0';
            len = i;
            break;
        }

    // Get release
    for (i = len-1; i >= 0; i--)
        if (str[i] == '-') {
            res->release = g_strdup(str+i+1);
            str[i] = '\0';
            len = i;
            break;
        }

    // Get version
    for (i = len-1; i >= 0; i--)
        if (str[i] == '-') {
            res->version = g_strdup(str+i+1);
            str[i] = '\0';
            len = i;
            break;
        }

    // Get epoch
    for (i = 0; i < (int) len; i++)
        if (str[i] == ':') {
            str[i] = '\0';
            res->epoch = g_strdup(str);
            str += i + 1;
            break;
        }

    // Get name
    res->name = g_strdup(str);
    g_free(copy);

    return res;
}


void
cr_nvrea_free(struct cr_NVREA *nvrea)
{
    if (!nvrea)
        return;

    g_free(nvrea->name);
    g_free(nvrea->version);
    g_free(nvrea->release);
    g_free(nvrea->epoch);
    g_free(nvrea->arch);
    g_free(nvrea);
}


int
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
