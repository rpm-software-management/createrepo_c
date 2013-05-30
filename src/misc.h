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

#ifndef __C_CREATEREPOLIB_MISC_H__
#define __C_CREATEREPOLIB_MISC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <string.h>
#include <curl/curl.h>
#include "compression_wrapper.h"
#include "constants.h"

/** \defgroup   misc    Miscellaneous useful functions and macros.
 *  \addtogroup misc
 *  @{
 */

/** Macro for supress compiler warning about unused param.
 */
#define CR_UNUSED(x) (void)(x)

/** Lenght of static string (including last '\0' byte)
 */
#define CR_STATICSTRLEN(s) (sizeof(s)/sizeof(s[0]))

/** Convert flags from RPM header to a string representation.
 * @param flags         flags
 * @return              flags as constant string
 */
const char *cr_flag_to_str(gint64 flags);

/** Epoch-Version-Release representation.
 */
struct cr_EVR {
    char *epoch;        /*!< epoch */
    char *version;      /*!< version */
    char *release;      /*!< release */
};

/** Name-Version-Release-Epoch-Arch representation.
 */
struct cr_NVREA {
    char *name;         /*!< name */
    char *version;      /*!< version */
    char *release;      /*!< release */
    char *epoch;        /*!< epoch */
    char *arch;         /*!< arch */
};

/** Version representation
 * e.g. for openssl-devel-1.0.0i = version: 1, release: 0, patch: 0, suffix: i
 */
struct cr_Version {
    long version;       /*!< version */
    long release;       /*!< release */
    long patch;         /*!< patch */
    char *suffix;       /*!< rest of version string after conversion */
};


/** Convert epoch-version-release string into cr_EVR structure.
 * If no GStringChunk passed, all non NULL items in returned structure
 * are malloced and in that case, you have to free all non-NULL element
 * yourself.
 * @param string        NULL terminated n-v-r string
 * @param chunk         string chunk for strings (optional - could be NULL)
 * @return              filled NVR
 */
struct cr_EVR cr_str_to_evr(const char *string, GStringChunk *chunk);

/** Check if the filename match pattern for primary files (files listed
 *  in primary.xml).
 * @param filename      full path to file
 * @return              1 if it is primary file, otherwise 0
 */
static inline int cr_is_primary(const char *filename) {
    if (!strncmp(filename, "/etc/", 5))
        return 1;
    if (!strcmp(filename, "/usr/lib/sendmail"))
        return 1;
    if (strstr(filename, "bin/"))
        return 1;
    return 0;
};

/** Compute file checksum.
 * @param filename      filename
 * @param type          type of checksum
 * @param err           GError **
 * @return              malloced null terminated string with checksum
 *                      or NULL on error
 */
char *cr_compute_file_checksum(const char *filename,
                               cr_ChecksumType type,
                               GError **err);

/** Header range
 */
struct cr_HeaderRangeStruct {
    unsigned int start;         /*!< First byte of header */
    unsigned int end;           /*!< Last byte of header */
};

/** Return header byte range.
 * @param filename      filename
 * @return              header range (start = end = 0 on error)
 */
struct cr_HeaderRangeStruct cr_get_header_byte_range(const char *filename);

/** Return checksum name.
 * @param type          checksum type
 * @return              constant null terminated string with checksum name
 *                      or NULL on error
 */
const char *cr_checksum_name_str(cr_ChecksumType type);

/** Return pointer to the rest of string after last '/'.
 * (e.g. for "/foo/bar" returns "bar")
 * @param filepath      path
 * @return              pointer into the path
 */
char *cr_get_filename(const char *filepath);

#define CR_COPY_OK              0       /*!< Copy successfully finished */
#define CR_COPY_ERR             1       /*!< Error while copy */

/** Download a file from the URL into the in_dst via curl handle.
 * If *error == NULL then download was successfull.
 * @param handle        CURL handle
 * @param url           source url
 * @param destination   destination (if destination is dir, filename from the
 *                      url is used)
 * @param error         pointer to string pointer for error message
 *                      (mandatory argument!)
 */
void cr_download(CURL *handle,
                 const char *url,
                 const char *destination,
                 char **error);

/** Copy file.
 * @param src           source filename
 * @param dst           destination (if dst is dir, filename of src is used)
 * @return              CR_COPY_OK or CR_COPY_ERR on error
 */
int cr_copy_file(const char *src, const char *dst);

/** Compress file.
 * @param src           source filename
 * @param dst           destination (If dst is dir, filename of src +
 *                      compression suffix is used.
 *                      If dst is NULL, src + compression suffix is used)
 * @param compression   type of compression
 * @return              CR_COPY_OK or CR_COPY_ERR on error
 */
int cr_compress_file(const char *src,
                     const char *dst,
                     cr_CompressionType compression);

/** Better copy file. Source (src) could be remote address (http:// or ftp://).
 * @param src           source filename
 * @param dst           destination (if dst is dir, filename of src is used)
 * @return              CR_COPY_OK or CR_COPY_ERR on error
 */
int cr_better_copy_file(const char *src, const char *dst);

/** Recursively remove directory.
 * @param path          filepath
 * @return              0 on success, nonzero on failure (errno is set)
 */
int cr_remove_dir(const char *path);

/** Normalize path (Path with exactly one trailing '/').
 *@param path           path
 *@return               mallocated string with normalized path or NULL
 */
char *cr_normalize_dir_path(const char *path);

/** Convert version string into cr_Version struct.
 * @param str           version string
 * @return              cr_Version
 */
struct cr_Version cr_str_to_version(const char *str);

/** Compare two version string.
 * @param str1          first version string
 * @param str2          second version string
 * @return              0 - versions are same, 1 - first string is bigger
 *                      version, 2 - second string is bigger version
 */
int cr_cmp_version_str(const char* str1, const char *str2);

/** Logging function with no output.
 * @param log_domain    logging domain
 * @param log_level     logging level
 * @param message       message
 * @param user_data     user data
 */
void cr_null_log_fn(const gchar *log_domain,
                    GLogLevelFlags log_level,
                    const gchar *message,
                    gpointer user_data);

/** Createrepo_c library standard logging function.
 * @param log_domain    logging domain
 * @param log_level     logging level
 * @param message       message
 * @param user_data     user data
 */
void cr_log_fn(const gchar *log_domain,
               GLogLevelFlags log_level,
               const gchar *message,
               gpointer user_data);

/** Frees all the memory used by a GSList, and calls the specified destroy
 * function on every element's data.
 * This is the same function as g_slist_free_full(). The original function
 * is implemented in glib since 2.28 but we need to support the older glib too.
 * @param list          pointer to GSList
 * @param free_f        the function to be called to free each element's data
 */
void cr_slist_free_full(GSList *list, GDestroyNotify free_f);

/** Split filename into the NVREA structure.
 * @param filename      filename
 * @return              struct cr_NVREA
 */
struct cr_NVREA *cr_split_rpm_filename(const char *filename);

/** Free struct cr_NVREA.
 * @param nvrea         struct cr_NVREA
 */
void cr_nvrea_free(struct cr_NVREA *nvrea);

/** Compare evr of two cr_NVREA. Name and arch are ignored.
 * @param A     pointer to first cr_NVREA
 * @param B     pointer to second cr_NVREA
 * @return      0 = same, 1 = first is newer, -1 = second is newer
 */
#define cr_cmp_nvrea(A, B) (cr_cmp_evr((A)->epoch, (A)->version, (A)->release,\
                                        (B)->epoch, (B)->version, (B)->release))

/** Compare two version strings splited into evr chunks.
 * @param e1     1. epoch
 * @param v1     1. version
 * @param r1     1. release
 * @param e2     2. epoch
 * @param v2     2. version
 * @param r2     2. release
 * @return       0 = same, 1 = first is newer, -1 = second is newer
 */
int cr_cmp_evr(const char *e1, const char *v1, const char *r1,
               const char *e2, const char *v2, const char *r2);


/** Safe insert into GStringChunk.
 * @param chunk     a GStringChunk
 * @param str       string to add or NULL
 * @return          pointer to the copy of str or NULL if str is NULL
 */
static inline gchar *
cr_safe_string_chunk_insert(GStringChunk *chunk, const char *str)
{
    if (!str) return NULL;
    return g_string_chunk_insert(chunk, str);
}

/** Safe insert into GStringChunk. If str is NULL or "\0" inserts nothing and
 * returns NULL.
 * @param chunk     a GStringChunk
 * @param str       string to add or NULL
 * @return          pointer to the copy of str or NULL if str is NULL
 */
static inline gchar *
cr_safe_string_chunk_insert_null(GStringChunk *chunk, const char *str)
{
    if (!str || *str == '\0') return NULL;
    return g_string_chunk_insert(chunk, str);
}


/** Safe const insert into GStringChunk.
 * @param chunk     a GStringChunk
 * @param str       string to add or NULL
 * @return          pointer to the copy of str or NULL if str is NULL
 */
static inline gchar *
cr_safe_string_chunk_insert_const(GStringChunk *chunk, const char *str)
{
    if (!str) return NULL;
    return g_string_chunk_insert_const(chunk, str);
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_MISC_H__ */
