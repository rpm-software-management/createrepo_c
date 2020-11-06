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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "fixtures.h"
#include "createrepo/checksum.h"
#include "createrepo/misc.h"
#include "createrepo/error.h"

#define PACKAGE_01              TEST_PACKAGES_PATH"super_kernel-6.0.1-2.x86_64.rpm"
#define PACKAGE_01_HEADER_START 280
#define PACKAGE_01_HEADER_END   2637

#define PACKAGE_02              TEST_PACKAGES_PATH"fake_bash-1.1.1-1.x86_64.rpm"
#define PACKAGE_02_HEADER_START 280
#define PACKAGE_02_HEADER_END   2057

#define VALID_URL_01    "http://google.com/index.html"
#define URL_FILENAME_01 "index.html"
#define INVALID_URL     "htp://foo.bar"

static void
test_cr_str_to_evr(void)
{
    cr_EVR *evr;

    // V

    evr = cr_str_to_evr("5.0.0", NULL);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "5.0.0");
    g_assert_cmpstr(evr->release, ==, NULL);
    cr_evr_free(evr);

    evr = cr_str_to_evr("6.1", NULL);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "6.1");
    g_assert_cmpstr(evr->release, ==, NULL);
    cr_evr_free(evr);

    evr = cr_str_to_evr("7", NULL);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "7");
    g_assert_cmpstr(evr->release, ==, NULL);
    cr_evr_free(evr);

    // VR

    evr = cr_str_to_evr("5.0.0-2", NULL);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "5.0.0");
    g_assert_cmpstr(evr->release, ==, "2");
    cr_evr_free(evr);

    evr = cr_str_to_evr("6.1-3", NULL);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "6.1");
    g_assert_cmpstr(evr->release, ==, "3");
    cr_evr_free(evr);

    evr = cr_str_to_evr("7-4", NULL);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "7");
    g_assert_cmpstr(evr->release, ==, "4");
    cr_evr_free(evr);

    // EV

    evr = cr_str_to_evr("1:5.0.0", NULL);
    g_assert_cmpstr(evr->epoch, ==, "1");
    g_assert_cmpstr(evr->version, ==, "5.0.0");
    g_assert_cmpstr(evr->release, ==, NULL);
    cr_evr_free(evr);

    evr = cr_str_to_evr("2:6.1", NULL);
    g_assert_cmpstr(evr->epoch, ==, "2");
    g_assert_cmpstr(evr->version, ==, "6.1");
    g_assert_cmpstr(evr->release, ==, NULL);
    cr_evr_free(evr);

    evr = cr_str_to_evr("3:7", NULL);
    g_assert_cmpstr(evr->epoch, ==, "3");
    g_assert_cmpstr(evr->version, ==, "7");
    g_assert_cmpstr(evr->release, ==, NULL);
    cr_evr_free(evr);

    //cr_EVR

    evr = cr_str_to_evr("1:5.0.0-11", NULL);
    g_assert_cmpstr(evr->epoch, ==, "1");
    g_assert_cmpstr(evr->version, ==, "5.0.0");
    g_assert_cmpstr(evr->release, ==, "11");
    cr_evr_free(evr);

    evr = cr_str_to_evr("2:6.1-22", NULL);
    g_assert_cmpstr(evr->epoch, ==, "2");
    g_assert_cmpstr(evr->version, ==, "6.1");
    g_assert_cmpstr(evr->release, ==, "22");
    cr_evr_free(evr);

    evr = cr_str_to_evr("3:7-33", NULL);
    g_assert_cmpstr(evr->epoch, ==, "3");
    g_assert_cmpstr(evr->version, ==, "7");
    g_assert_cmpstr(evr->release, ==, "33");
    cr_evr_free(evr);

    // Bad strings

    evr = cr_str_to_evr(":", NULL);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "");
    g_assert_cmpstr(evr->release, ==, NULL);
    cr_evr_free(evr);

    evr = cr_str_to_evr(":-", NULL);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "");
    g_assert_cmpstr(evr->release, ==, NULL);
    cr_evr_free(evr);

    // Really bad values

    evr = cr_str_to_evr(NULL, NULL);
    g_assert_cmpstr(evr->epoch, ==, NULL);
    g_assert_cmpstr(evr->version, ==, NULL);
    g_assert_cmpstr(evr->release, ==, NULL);
    cr_evr_free(evr);

    evr = cr_str_to_evr("", NULL);
    g_assert_cmpstr(evr->epoch, ==, NULL);
    g_assert_cmpstr(evr->version, ==, NULL);
    g_assert_cmpstr(evr->release, ==, NULL);
    cr_evr_free(evr);

    evr = cr_str_to_evr("-", NULL);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "");
    g_assert_cmpstr(evr->release, ==, NULL);
    cr_evr_free(evr);

    evr = cr_str_to_evr("-:", NULL);
    g_assert_cmpstr(evr->epoch, ==, NULL);
    g_assert_cmpstr(evr->version, ==, "");
    g_assert_cmpstr(evr->release, ==, NULL);
    cr_evr_free(evr);

    evr = cr_str_to_evr("foo:bar", NULL);
    g_assert_cmpstr(evr->epoch, ==, NULL);
    g_assert_cmpstr(evr->version, ==, "bar");
    g_assert_cmpstr(evr->release, ==, NULL);
    cr_evr_free(evr);
}


static void
test_cr_str_to_evr_with_chunk(void)
{
    cr_EVR *evr;
    GStringChunk *chunk;
    chunk = g_string_chunk_new(512);

    // V

    evr = cr_str_to_evr("5.0.0", chunk);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "5.0.0");
    g_assert_cmpstr(evr->release, ==, NULL);
    g_free(evr);

    evr = cr_str_to_evr("6.1", chunk);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "6.1");
    g_assert_cmpstr(evr->release, ==, NULL);
    g_free(evr);

    evr = cr_str_to_evr("7", chunk);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "7");
    g_assert_cmpstr(evr->release, ==, NULL);
    g_free(evr);

    // VR

    evr = cr_str_to_evr("5.0.0-2", chunk);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "5.0.0");
    g_assert_cmpstr(evr->release, ==, "2");
    g_free(evr);

    evr = cr_str_to_evr("6.1-3", chunk);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "6.1");
    g_assert_cmpstr(evr->release, ==, "3");
    g_free(evr);

    evr = cr_str_to_evr("7-4", chunk);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "7");
    g_assert_cmpstr(evr->release, ==, "4");
    g_free(evr);

    // EV

    evr = cr_str_to_evr("1:5.0.0", chunk);
    g_assert_cmpstr(evr->epoch, ==, "1");
    g_assert_cmpstr(evr->version, ==, "5.0.0");
    g_assert_cmpstr(evr->release, ==, NULL);
    g_free(evr);

    evr = cr_str_to_evr("2:6.1", chunk);
    g_assert_cmpstr(evr->epoch, ==, "2");
    g_assert_cmpstr(evr->version, ==, "6.1");
    g_assert_cmpstr(evr->release, ==, NULL);
    g_free(evr);

    evr = cr_str_to_evr("3:7", chunk);
    g_assert_cmpstr(evr->epoch, ==, "3");
    g_assert_cmpstr(evr->version, ==, "7");
    g_assert_cmpstr(evr->release, ==, NULL);
    g_free(evr);

    //cr_EVR

    evr = cr_str_to_evr("1:5.0.0-11", chunk);
    g_assert_cmpstr(evr->epoch, ==, "1");
    g_assert_cmpstr(evr->version, ==, "5.0.0");
    g_assert_cmpstr(evr->release, ==, "11");
    g_free(evr);

    evr = cr_str_to_evr("2:6.1-22", chunk);
    g_assert_cmpstr(evr->epoch, ==, "2");
    g_assert_cmpstr(evr->version, ==, "6.1");
    g_assert_cmpstr(evr->release, ==, "22");
    g_free(evr);

    evr = cr_str_to_evr("3:7-33", chunk);
    g_assert_cmpstr(evr->epoch, ==, "3");
    g_assert_cmpstr(evr->version, ==, "7");
    g_assert_cmpstr(evr->release, ==, "33");
    g_free(evr);

    // Bad strings

    evr = cr_str_to_evr(":", chunk);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "");
    g_assert_cmpstr(evr->release, ==, NULL);
    g_free(evr);

    evr = cr_str_to_evr(":-", chunk);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "");
    g_assert_cmpstr(evr->release, ==, NULL);
    g_free(evr);

    // Really bad values

    evr = cr_str_to_evr(NULL, chunk);
    g_assert_cmpstr(evr->epoch, ==, NULL);
    g_assert_cmpstr(evr->version, ==, NULL);
    g_assert_cmpstr(evr->release, ==, NULL);
    g_free(evr);

    evr = cr_str_to_evr("", chunk);
    g_assert_cmpstr(evr->epoch, ==, NULL);
    g_assert_cmpstr(evr->version, ==, NULL);
    g_assert_cmpstr(evr->release, ==, NULL);
    g_free(evr);

    evr = cr_str_to_evr("-", chunk);
    g_assert_cmpstr(evr->epoch, ==, "0");
    g_assert_cmpstr(evr->version, ==, "");
    g_assert_cmpstr(evr->release, ==, NULL);
    g_free(evr);

    evr = cr_str_to_evr("-:", chunk);
    g_assert_cmpstr(evr->epoch, ==, NULL);
    g_assert_cmpstr(evr->version, ==, "");
    g_assert_cmpstr(evr->release, ==, NULL);
    g_free(evr);

    evr = cr_str_to_evr("foo:bar", chunk);
    g_assert_cmpstr(evr->epoch, ==, NULL);
    g_assert_cmpstr(evr->version, ==, "bar");
    g_assert_cmpstr(evr->release, ==, NULL);
    g_free(evr);

    g_string_chunk_free(chunk);
}


static void
test_cr_is_primary(void)
{
    g_assert(cr_is_primary("/etc/foobar"));
    g_assert(cr_is_primary("/etc/"));
    g_assert(!cr_is_primary("/foo/etc/foobar"));
    g_assert(!cr_is_primary("/tmp/etc/"));

    g_assert(cr_is_primary("/sbin/foobar"));
    g_assert(cr_is_primary("/bin/bash"));
    g_assert(cr_is_primary("/usr/sbin/foobar"));
    g_assert(cr_is_primary("/usr/bin/foobar"));
    g_assert(cr_is_primary("/usr/share/locale/bin/LC_MESSAGES"));  // Sad, but we have to reflect yum behavior
    g_assert(cr_is_primary("/usr/share/man/bin/man0p"));           // my heart is bleeding
    g_assert(!cr_is_primary("/foo/bindir"));
    g_assert(!cr_is_primary("/foo/sbindir"));

    g_assert(cr_is_primary("/usr/lib/sendmail"));
    g_assert(!cr_is_primary("/tmp/usr/lib/sendmail"));

    g_assert(!cr_is_primary(""));
}


static void
test_cr_get_header_byte_range(void)
{
    struct cr_HeaderRangeStruct hdr_range;
    GError *tmp_err = NULL;

    hdr_range = cr_get_header_byte_range(PACKAGE_01, NULL);
    g_assert_cmpuint(hdr_range.start, ==, PACKAGE_01_HEADER_START);
    g_assert_cmpuint(hdr_range.end, ==, PACKAGE_01_HEADER_END);

    hdr_range = cr_get_header_byte_range(PACKAGE_02, &tmp_err);
    g_assert(!tmp_err);
    g_assert_cmpuint(hdr_range.start, ==, PACKAGE_02_HEADER_START);
    g_assert_cmpuint(hdr_range.end, ==, PACKAGE_02_HEADER_END);

    hdr_range = cr_get_header_byte_range(NON_EXIST_FILE, &tmp_err);
    g_assert(tmp_err);
    g_error_free(tmp_err);
    tmp_err = NULL;
    g_assert_cmpuint(hdr_range.start, ==, 0);
    g_assert_cmpuint(hdr_range.end, ==, 0);
}


static void
test_cr_get_filename(void)
{
    char *filename;

    filename = cr_get_filename("/fooo/bar/file");
    g_assert_cmpstr(filename, ==, "file");

    filename = cr_get_filename("///fooo///bar///file");
    g_assert_cmpstr(filename, ==, "file");

    filename = cr_get_filename("/file");
    g_assert_cmpstr(filename, ==, "file");

    filename = cr_get_filename("///file");
    g_assert_cmpstr(filename, ==, "file");

    filename = cr_get_filename("file");
    g_assert_cmpstr(filename, ==, "file");

    filename = cr_get_filename("./file");
    g_assert_cmpstr(filename, ==, "file");

    filename = cr_get_filename("");
    g_assert_cmpstr(filename, ==, "");

    filename = cr_get_filename(NULL);
    g_assert_cmpstr(filename, ==, NULL);
}


static int
read_file(char *f, cr_CompressionType compression, char* buffer, int amount)
{
    int ret = CRE_OK;
    GError *tmp_err = NULL;
    CR_FILE *orig = NULL;
    orig = cr_open(f, CR_CW_MODE_READ, compression, &tmp_err);
    if (!orig) {
        ret = tmp_err->code;
        return ret;
    }
    cr_read(orig, buffer, amount, &tmp_err);
    if (orig)
        cr_close(orig, NULL);
    return ret;
}


#define DST_FILE        "b"

typedef struct {
    gchar *tmp_dir;
    gchar *dst_file;
} Copyfiletest;


static void
copyfiletest_setup(Copyfiletest *copyfiletest,
                   G_GNUC_UNUSED gconstpointer test_data)
{
    copyfiletest->tmp_dir = g_strdup(TMPDIR_TEMPLATE);
    mkdtemp(copyfiletest->tmp_dir);
    copyfiletest->dst_file = g_strconcat(copyfiletest->tmp_dir, "/", DST_FILE, NULL);
}


static void
copyfiletest_teardown(Copyfiletest *copyfiletest,
                      G_GNUC_UNUSED gconstpointer test_data)
{
    remove(copyfiletest->dst_file);
    rmdir(copyfiletest->tmp_dir);
    g_free(copyfiletest->tmp_dir);
    g_free(copyfiletest->dst_file);
}


static void
copyfiletest_test_empty_file(Copyfiletest *copyfiletest,
                             G_GNUC_UNUSED gconstpointer test_data)
{
    gboolean ret;
    char *checksum;
    GError *tmp_err = NULL;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = cr_copy_file(TEST_EMPTY_FILE, copyfiletest->dst_file, &tmp_err);
    g_assert(ret);
    g_assert(!tmp_err);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_IS_REGULAR));
    checksum = cr_checksum_file(copyfiletest->dst_file, CR_CHECKSUM_SHA256, NULL);
    g_assert_cmpstr(checksum, ==, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    g_free(checksum);
}


static void
copyfiletest_test_text_file(Copyfiletest *copyfiletest,
                            G_GNUC_UNUSED gconstpointer test_data)
{
    gboolean ret;
    char *checksum;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = cr_copy_file(TEST_TEXT_FILE, copyfiletest->dst_file, NULL);
    g_assert(ret);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_IS_REGULAR));
    checksum = cr_checksum_file(copyfiletest->dst_file, CR_CHECKSUM_SHA256, NULL);
    g_assert_cmpstr(checksum, ==, "2f395bdfa2750978965e4781ddf224c89646c7d7a1569b7ebb023b170f7bd8bb");
    g_free(checksum);
}


static void
copyfiletest_test_binary_file(Copyfiletest *copyfiletest,
                              G_GNUC_UNUSED gconstpointer test_data)
{
    gboolean ret;
    char *checksum;
    GError *tmp_err = NULL;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = cr_copy_file(TEST_BINARY_FILE, copyfiletest->dst_file, &tmp_err);
    g_assert(!tmp_err);
    g_assert(ret);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_IS_REGULAR));
    checksum = cr_checksum_file(copyfiletest->dst_file, CR_CHECKSUM_SHA256, NULL);
    g_assert_cmpstr(checksum, ==, "bf68e32ad78cea8287be0f35b74fa3fecd0eaa91770b48f1a7282b015d6d883e");
    g_free(checksum);
}


static void
copyfiletest_test_rewrite(Copyfiletest *copyfiletest,
                          G_GNUC_UNUSED gconstpointer test_data)
{
    gboolean ret;
    char *checksum;
    GError *tmp_err = NULL;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = cr_copy_file(TEST_BINARY_FILE, copyfiletest->dst_file, NULL);
    g_assert(ret);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_IS_REGULAR));
    checksum = cr_checksum_file(copyfiletest->dst_file, CR_CHECKSUM_SHA256, NULL);
    g_assert_cmpstr(checksum, ==, "bf68e32ad78cea8287be0f35b74fa3fecd0eaa91770b48f1a7282b015d6d883e");
    g_free(checksum);

    ret = cr_copy_file(TEST_TEXT_FILE, copyfiletest->dst_file, &tmp_err);
    g_assert(!tmp_err);
    g_assert(ret);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_IS_REGULAR));
    checksum = cr_checksum_file(copyfiletest->dst_file, CR_CHECKSUM_SHA256, NULL);
    g_assert_cmpstr(checksum, ==, "2f395bdfa2750978965e4781ddf224c89646c7d7a1569b7ebb023b170f7bd8bb");
    g_free(checksum);
}


static void
copyfiletest_test_corner_cases(Copyfiletest *copyfiletest,
                               G_GNUC_UNUSED gconstpointer test_data)
{
    gboolean ret;
    GError *tmp_err = NULL;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));

    // Without GError
    ret = cr_copy_file(NON_EXIST_FILE, copyfiletest->dst_file, NULL);
    g_assert(!ret);
    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));

    // With GError
    ret = cr_copy_file(NON_EXIST_FILE, copyfiletest->dst_file, &tmp_err);
    g_assert(tmp_err);
    g_error_free(tmp_err);
    g_assert(!ret);
    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
}


static void
compressfile_test_text_file(Copyfiletest *copyfiletest,
                            G_GNUC_UNUSED gconstpointer test_data)
{
    int ret;
    char *checksum;
    GError *tmp_err = NULL;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = cr_compress_file(TEST_TEXT_FILE, copyfiletest->dst_file,
                           CR_CW_GZ_COMPRESSION, NULL, FALSE, &tmp_err);
    g_assert(!tmp_err);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_IS_REGULAR));
    checksum = cr_checksum_file(copyfiletest->dst_file, CR_CHECKSUM_SHA256, NULL);
    g_assert_cmpstr(checksum, ==, "8909fde88a5747d800fd2562b0f22945f014aa7df64"
                                  "cf1c15c7933ae54b72ab6");
    g_free(checksum);
}


static void
compressfile_with_stat_test_text_file(Copyfiletest *copyfiletest,
                                      G_GNUC_UNUSED gconstpointer test_data)
{
    int ret;
    char *checksum;
    cr_ContentStat *stat;
    GError *tmp_err = NULL;

    stat = cr_contentstat_new(CR_CHECKSUM_SHA256, &tmp_err);
    g_assert(stat);
    g_assert(!tmp_err);

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = cr_compress_file_with_stat(TEST_TEXT_FILE, copyfiletest->dst_file,
                                     CR_CW_GZ_COMPRESSION, stat, NULL, FALSE,
                                     &tmp_err);
    g_assert(!tmp_err);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_IS_REGULAR));
    checksum = cr_checksum_file(TEST_TEXT_FILE, CR_CHECKSUM_SHA256, NULL);
    g_assert_cmpstr(stat->checksum, ==, checksum);
    cr_contentstat_free(stat, &tmp_err);
    g_assert(!tmp_err);
}


static void
compressfile_with_stat_test_gz_file_gz_output(Copyfiletest *copyfiletest,
                                      G_GNUC_UNUSED gconstpointer test_data)
{
    int ret;
    char *checksum;
    cr_ContentStat *stat;
    GError *tmp_err = NULL;

    stat = cr_contentstat_new(CR_CHECKSUM_SHA256, &tmp_err);
    g_assert(stat);
    g_assert(!tmp_err);

    char * dst_full_name = g_strconcat(copyfiletest->dst_file, ".gz", NULL);

    g_assert(!g_file_test(dst_full_name, G_FILE_TEST_EXISTS));
    ret = cr_compress_file_with_stat(TEST_TEXT_FILE_GZ, dst_full_name,
                                     CR_CW_GZ_COMPRESSION, stat, NULL, FALSE,
                                     &tmp_err);
    g_assert(!tmp_err);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert(g_file_test(dst_full_name, G_FILE_TEST_IS_REGULAR));
    checksum = cr_checksum_file(TEST_TEXT_FILE, CR_CHECKSUM_SHA256, NULL);
    g_assert_cmpstr(stat->checksum, ==, checksum);

    //assert content is readable after decompression and recompression
    char buf[30];
    read_file(dst_full_name, CR_CW_GZ_COMPRESSION, buf, 30);
    g_assert(g_strrstr(buf, "Lorem ipsum dolor sit amet"));

    cr_contentstat_free(stat, &tmp_err);
    g_assert(!tmp_err);
    free(dst_full_name);
}


static void
compressfile_test_gz_file_xz_output(Copyfiletest *copyfiletest,
                                      G_GNUC_UNUSED gconstpointer test_data)
{
    int ret;
    GError *tmp_err = NULL;

    char * dst_full_name = g_strconcat(copyfiletest->dst_file, ".xz", NULL);

    g_assert(!g_file_test(dst_full_name, G_FILE_TEST_EXISTS));
    ret = cr_compress_file(TEST_TEXT_FILE_GZ, dst_full_name,
                                     CR_CW_XZ_COMPRESSION, NULL, FALSE,
                                     &tmp_err);
    g_assert(!tmp_err);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert(g_file_test(dst_full_name, G_FILE_TEST_IS_REGULAR));

    //assert content is readable after decompression and recompression
    char buf[30];
    read_file(dst_full_name, CR_CW_XZ_COMPRESSION, buf, 30);
    g_assert(g_strrstr(buf, "Lorem ipsum dolor sit amet"));

    g_assert(!tmp_err);
    free(dst_full_name);
}


static void
compressfile_test_xz_file_gz_output(Copyfiletest *copyfiletest,
                                      G_GNUC_UNUSED gconstpointer test_data)
{
    int ret;
    GError *tmp_err = NULL;

    char * dst_full_name = g_strconcat(copyfiletest->dst_file, ".gz", NULL);

    g_assert(!g_file_test(dst_full_name, G_FILE_TEST_EXISTS));
    ret = cr_compress_file(TEST_TEXT_FILE_XZ, dst_full_name,
                                     CR_CW_GZ_COMPRESSION, NULL, FALSE,
                                     &tmp_err);
    g_assert(!tmp_err);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert(g_file_test(dst_full_name, G_FILE_TEST_IS_REGULAR));

    //assert content is readable after decompression and recompression
    char buf[30];
    read_file(dst_full_name, CR_CW_GZ_COMPRESSION, buf, 30);
    g_assert(g_strrstr(buf, "Lorem ipsum dolor sit amet"));

    g_assert(!tmp_err);
    free(dst_full_name);
}


static void
compressfile_test_sqlite_file_gz_output(Copyfiletest *copyfiletest,
                                        G_GNUC_UNUSED gconstpointer test_data)
{
    int ret;
    GError *tmp_err = NULL;

    char * dst_full_name = g_strconcat(copyfiletest->dst_file, ".gz", NULL);

    g_assert(!g_file_test(dst_full_name, G_FILE_TEST_EXISTS));
    ret = cr_compress_file(TEST_SQLITE_FILE, dst_full_name,
                                     CR_CW_GZ_COMPRESSION, NULL, FALSE,
                                     &tmp_err);
    g_assert(!tmp_err);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert(g_file_test(dst_full_name, G_FILE_TEST_EXISTS));

    g_assert(!tmp_err);
}


static void
decompressfile_with_stat_test_text_file(Copyfiletest *copyfiletest,
                                        G_GNUC_UNUSED gconstpointer test_data)
{
    int ret;
    cr_ContentStat *stat;
    GError *tmp_err = NULL;

    stat = cr_contentstat_new(CR_CHECKSUM_SHA256, &tmp_err);
    g_assert(stat);
    g_assert(!tmp_err);

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = cr_decompress_file_with_stat(TEST_TEXT_FILE_GZ, copyfiletest->dst_file,
                                       CR_CW_GZ_COMPRESSION, stat, &tmp_err);
    g_assert(!tmp_err);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_IS_REGULAR));
    g_assert_cmpstr(stat->checksum, ==, TEST_TEXT_FILE_SHA256SUM);
    cr_contentstat_free(stat, &tmp_err);
    g_assert(!tmp_err);
}


static void
test_cr_better_copy_file_local(Copyfiletest *copyfiletest,
                               G_GNUC_UNUSED gconstpointer test_data)
{
    gboolean ret;
    char *checksum;
    GError *tmp_err = NULL;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = cr_better_copy_file(TEST_BINARY_FILE, copyfiletest->dst_file, &tmp_err);
    g_assert(!tmp_err);
    g_assert(ret);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_IS_REGULAR));
    checksum = cr_checksum_file(copyfiletest->dst_file, CR_CHECKSUM_SHA256, NULL);
    g_assert_cmpstr(checksum, ==, "bf68e32ad78cea8287be0f35b74fa3fecd0eaa91770b48f1a7282b015d6d883e");
    g_free(checksum);
}


static void
test_cr_remove_dir(void)
{
    char *tmp_dir;
    char *subdir01, *subdir02, *subsubdir011, *subsubsubdir0111;
    gchar *tmp_file_1, *tmp_file_2, *tmp_file_3;

    tmp_dir = g_strdup(TMPDIR_TEMPLATE);
    g_assert(mkdtemp(tmp_dir));

    subdir01 = g_strconcat(tmp_dir, "/subdir01", NULL);
    subdir02 = g_strconcat(tmp_dir, "/subdir02", NULL);
    subsubdir011 = g_strconcat(subdir01, "/subsubdir011", NULL);
    subsubsubdir0111 = g_strconcat(subsubdir011, "/subsubsubdir0111", NULL);

    g_assert_cmpint(g_mkdir_with_parents(subdir02, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH), ==, 0);
    g_assert_cmpint(g_mkdir_with_parents(subsubsubdir0111, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH), ==, 0);

    tmp_file_1 = g_strconcat(subsubsubdir0111, "/file_0111", NULL);
    tmp_file_2 = g_strconcat(subsubdir011, "/file_011", NULL);
    tmp_file_3 = g_strconcat(subdir02, "/file_02", NULL);

    g_assert(g_file_test(tmp_dir, G_FILE_TEST_EXISTS));

    g_assert(!g_file_test(tmp_file_1, G_FILE_TEST_EXISTS));
    g_assert(!g_file_test(tmp_file_2, G_FILE_TEST_EXISTS));
    g_assert(!g_file_test(tmp_file_3, G_FILE_TEST_EXISTS));

    FILE *f;
    f = fopen(tmp_file_1, "w");
    fputs("foo\n", f);
    fclose(f);

    f = fopen(tmp_file_2, "w");
    fputs("bar\n", f);
    fclose(f);

    f = fopen(tmp_file_3, "w");
    fputs("foobar\n", f);
    fclose(f);

    g_assert(g_file_test(tmp_file_1, G_FILE_TEST_EXISTS));
    g_assert(g_file_test(tmp_file_2, G_FILE_TEST_EXISTS));
    g_assert(g_file_test(tmp_file_3, G_FILE_TEST_EXISTS));

    cr_remove_dir(tmp_dir, NULL);

    g_assert(!g_file_test(tmp_file_1, G_FILE_TEST_EXISTS));
    g_assert(!g_file_test(tmp_file_2, G_FILE_TEST_EXISTS));
    g_assert(!g_file_test(tmp_file_3, G_FILE_TEST_EXISTS));

    g_assert(!g_file_test(tmp_dir, G_FILE_TEST_EXISTS));

    g_free(tmp_dir);
    g_free(subdir01);
    g_free(subdir02);
    g_free(subsubdir011);
    g_free(subsubsubdir0111);
    g_free(tmp_file_1);
    g_free(tmp_file_2);
    g_free(tmp_file_3);
}


static void
test_cr_normalize_dir_path(void)
{
    char *normalized;

    normalized = cr_normalize_dir_path("/////////");
    g_assert_cmpstr(normalized, ==, "/");
    g_free(normalized);

    normalized = cr_normalize_dir_path("///foo///bar///");
    g_assert_cmpstr(normalized, ==, "///foo///bar/");
    g_free(normalized);

    normalized = cr_normalize_dir_path("bar");
    g_assert_cmpstr(normalized, ==, "bar/");
    g_free(normalized);

    normalized = cr_normalize_dir_path(".////////////bar");
    g_assert_cmpstr(normalized, ==, ".////////////bar/");
    g_free(normalized);

    normalized = cr_normalize_dir_path("////////////bar");
    g_assert_cmpstr(normalized, ==, "////////////bar/");
    g_free(normalized);

    normalized = cr_normalize_dir_path("bar//////");
    g_assert_cmpstr(normalized, ==, "bar/");
    g_free(normalized);

    normalized = cr_normalize_dir_path("");
    g_assert_cmpstr(normalized, ==, "./");
    g_free(normalized);

    normalized = cr_normalize_dir_path(NULL);
    g_assert_cmpstr(normalized, ==, NULL);
    g_free(normalized);
}


static void
test_cr_str_to_version(void)
{
    struct cr_Version ver;

    ver = cr_str_to_version(NULL);
    g_assert_cmpint(ver.major, ==, 0);
    g_assert_cmpint(ver.minor, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = cr_str_to_version("");
    g_assert_cmpint(ver.major, ==, 0);
    g_assert_cmpint(ver.minor, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = cr_str_to_version("abcd");
    g_assert_cmpint(ver.major, ==, 0);
    g_assert_cmpint(ver.minor, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "abcd");
    g_free(ver.suffix);

    ver = cr_str_to_version("0.0.0");
    g_assert_cmpint(ver.major, ==, 0);
    g_assert_cmpint(ver.minor, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = cr_str_to_version("9");
    g_assert_cmpint(ver.major, ==, 9);
    g_assert_cmpint(ver.minor, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = cr_str_to_version("3beta");
    g_assert_cmpint(ver.major, ==, 3);
    g_assert_cmpint(ver.minor, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "beta");
    g_free(ver.suffix);

    ver = cr_str_to_version("5.2gamma");
    g_assert_cmpint(ver.major, ==, 5);
    g_assert_cmpint(ver.minor, ==, 2);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "gamma");
    g_free(ver.suffix);

    ver = cr_str_to_version("0.0.0b");
    g_assert_cmpint(ver.major, ==, 0);
    g_assert_cmpint(ver.minor, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "b");
    g_free(ver.suffix);

    ver = cr_str_to_version("2.3.4");
    g_assert_cmpint(ver.major, ==, 2);
    g_assert_cmpint(ver.minor, ==, 3);
    g_assert_cmpint(ver.patch, ==, 4);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = cr_str_to_version("11.33.123");
    g_assert_cmpint(ver.major, ==, 11);
    g_assert_cmpint(ver.minor, ==, 33);
    g_assert_cmpint(ver.patch, ==, 123);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = cr_str_to_version("1234567.0987654.45678");
    g_assert_cmpint(ver.major, ==, 1234567);
    g_assert_cmpint(ver.minor, ==, 987654);
    g_assert_cmpint(ver.patch, ==, 45678);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = cr_str_to_version("1.0.2i");
    g_assert_cmpint(ver.major, ==, 1);
    g_assert_cmpint(ver.minor, ==, 0);
    g_assert_cmpint(ver.patch, ==, 2);
    g_assert_cmpstr(ver.suffix, ==, "i");
    g_free(ver.suffix);

    ver = cr_str_to_version("1..3");
    g_assert_cmpint(ver.major, ==, 1);
    g_assert_cmpint(ver.minor, ==, 0);
    g_assert_cmpint(ver.patch, ==, 3);
    g_assert_cmpstr(ver.suffix, ==, NULL);
    g_free(ver.suffix);

    ver = cr_str_to_version("..alpha");
    g_assert_cmpint(ver.major, ==, 0);
    g_assert_cmpint(ver.minor, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "alpha");
    g_free(ver.suffix);

    ver = cr_str_to_version("alpha");
    g_assert_cmpint(ver.major, ==, 0);
    g_assert_cmpint(ver.minor, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "alpha");
    g_free(ver.suffix);

    ver = cr_str_to_version("1-2-3");
    g_assert_cmpint(ver.major, ==, 1);
    g_assert_cmpint(ver.minor, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "-2-3");
    g_free(ver.suffix);
}


static void
test_cr_cmp_version_str(void)
{
    int ret;

    ret = cr_cmp_version_str(NULL, NULL);
    g_assert_cmpint(ret, ==, 0);

    ret = cr_cmp_version_str("", "");
    g_assert_cmpint(ret, ==, 0);

    ret = cr_cmp_version_str(NULL, "");
    g_assert_cmpint(ret, ==, 0);

    ret = cr_cmp_version_str("", NULL);
    g_assert_cmpint(ret, ==, 0);

    ret = cr_cmp_version_str("3", "3");
    g_assert_cmpint(ret, ==, 0);

    ret = cr_cmp_version_str("1", "2");
    g_assert_cmpint(ret, ==, 2);

    ret = cr_cmp_version_str("99", "8");
    g_assert_cmpint(ret, ==, 1);

    ret = cr_cmp_version_str("5.4.3", "5.4.3");
    g_assert_cmpint(ret, ==, 0);

    ret = cr_cmp_version_str("5.3.2", "5.3.1");
    g_assert_cmpint(ret, ==, 1);

    ret = cr_cmp_version_str("5.3.5", "5.3.6");
    g_assert_cmpint(ret, ==, 2);

    ret = cr_cmp_version_str("6.3.2a", "6.3.2b");
    g_assert_cmpint(ret, ==, 2);

    ret = cr_cmp_version_str("6.3.2azb", "6.3.2abc");
    g_assert_cmpint(ret, ==, 1);

    ret = cr_cmp_version_str("1.2beta", "1.2beta");
    g_assert_cmpint(ret, ==, 0);

    ret = cr_cmp_version_str("n", "n");
    g_assert_cmpint(ret, ==, 0);

    ret = cr_cmp_version_str("c", "b");
    g_assert_cmpint(ret, ==,  1);

    ret = cr_cmp_version_str("c", "f");
    g_assert_cmpint(ret, ==, 2);

    ret = cr_cmp_version_str("2.1", "2.1.3");
    g_assert_cmpint(ret, ==, 2);
}


static void
test_cr_split_rpm_filename(void)
{
    cr_NEVRA *res;

    res = cr_split_rpm_filename(NULL);
    g_assert(!res);

    res = cr_split_rpm_filename("foo-1.0-1.i386");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "foo");
    g_assert_cmpstr(res->version, ==, "1.0");
    g_assert_cmpstr(res->release, ==, "1");
    g_assert(!res->epoch);
    g_assert_cmpstr(res->arch, ==, "i386");
    cr_nevra_free(res);

    res = cr_split_rpm_filename("1:bar-9-123a.ia64.rpm");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "bar");
    g_assert_cmpstr(res->version, ==, "9");
    g_assert_cmpstr(res->release, ==, "123a");
    g_assert_cmpstr(res->epoch, ==, "1");
    g_assert_cmpstr(res->arch, ==, "ia64");
    cr_nevra_free(res);

    res = cr_split_rpm_filename("bar-2:9-123a.ia64.rpm");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "bar");
    g_assert_cmpstr(res->version, ==, "9");
    g_assert_cmpstr(res->release, ==, "123a");
    g_assert_cmpstr(res->epoch, ==, "2");
    g_assert_cmpstr(res->arch, ==, "ia64");
    cr_nevra_free(res);

    res = cr_split_rpm_filename("bar-9-123a:3.ia64.rpm");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "bar");
    g_assert_cmpstr(res->version, ==, "9");
    g_assert_cmpstr(res->release, ==, "123a");
    g_assert_cmpstr(res->epoch, ==, "3");
    g_assert_cmpstr(res->arch, ==, "ia64");
    cr_nevra_free(res);

    res = cr_split_rpm_filename("bar-9-123a.ia64.rpm:4");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "bar");
    g_assert_cmpstr(res->version, ==, "9");
    g_assert_cmpstr(res->release, ==, "123a");
    g_assert_cmpstr(res->epoch, ==, "4");
    g_assert_cmpstr(res->arch, ==, "ia64");
    cr_nevra_free(res);

    res = cr_split_rpm_filename("bar-9-123a.ia64:5");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "bar");
    g_assert_cmpstr(res->version, ==, "9");
    g_assert_cmpstr(res->release, ==, "123a");
    g_assert_cmpstr(res->epoch, ==, "5");
    g_assert_cmpstr(res->arch, ==, "ia64");
    cr_nevra_free(res);

    res = cr_split_rpm_filename("b");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "b");
    g_assert(!res->version);
    g_assert(!res->release);
    g_assert(!res->epoch);
    g_assert(!res->arch);
    cr_nevra_free(res);
}


static void
test_cr_str_to_nevr(void)
{
    cr_NEVR *res;

    res = cr_str_to_nevr(NULL);
    g_assert(!res);

    res = cr_str_to_nevr("createrepo-0.9.9-22.fc20");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "createrepo");
    g_assert_cmpstr(res->version, ==, "0.9.9");
    g_assert_cmpstr(res->release, ==, "22.fc20");
    g_assert(!res->epoch);
    cr_nevr_free(res);

    res = cr_str_to_nevr("bar-4:9-123a");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "bar");
    g_assert_cmpstr(res->version, ==, "9");
    g_assert_cmpstr(res->release, ==, "123a");
    g_assert_cmpstr(res->epoch, ==, "4");
    cr_nevr_free(res);

    res = cr_str_to_nevr("3:foo-2-el.6");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "foo");
    g_assert_cmpstr(res->version, ==, "2");
    g_assert_cmpstr(res->release, ==, "el.6");
    g_assert_cmpstr(res->epoch, ==, "3");
    cr_nevr_free(res);

    res = cr_str_to_nevr("foo-2-el.6:3");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "foo");
    g_assert_cmpstr(res->version, ==, "2");
    g_assert_cmpstr(res->release, ==, "el.6");
    g_assert_cmpstr(res->epoch, ==, "3");
    cr_nevr_free(res);

    res = cr_str_to_nevr("b-1-2");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "b");
    g_assert_cmpstr(res->version, ==, "1");
    g_assert_cmpstr(res->release, ==, "2");
    g_assert(!res->epoch);
    cr_nevr_free(res);

    res = cr_str_to_nevr("b");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "b");
    g_assert(!res->version);
    g_assert(!res->release);
    g_assert(!res->epoch);
    cr_nevr_free(res);
}


static void
test_cr_str_to_nevra(void)
{
    cr_NEVRA *res;

    res = cr_str_to_nevra(NULL);
    g_assert(!res);

    res = cr_str_to_nevra("crypto-utils-2.4.1-52.fc20.x86_64");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "crypto-utils");
    g_assert_cmpstr(res->version, ==, "2.4.1");
    g_assert_cmpstr(res->release, ==, "52.fc20");
    g_assert(!res->epoch);
    g_assert_cmpstr(res->arch, ==, "x86_64");
    cr_nevra_free(res);

    res = cr_str_to_nevra("crypto-utils-1:2.4.1-52.fc20.x86_64");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "crypto-utils");
    g_assert_cmpstr(res->version, ==, "2.4.1");
    g_assert_cmpstr(res->release, ==, "52.fc20");
    g_assert_cmpstr(res->epoch, ==, "1");
    g_assert_cmpstr(res->arch, ==, "x86_64");
    cr_nevra_free(res);

    res = cr_str_to_nevra("2:crypto-utils-2.4.1-52.fc20.x86_64");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "crypto-utils");
    g_assert_cmpstr(res->version, ==, "2.4.1");
    g_assert_cmpstr(res->release, ==, "52.fc20");
    g_assert_cmpstr(res->epoch, ==, "2");
    g_assert_cmpstr(res->arch, ==, "x86_64");
    cr_nevra_free(res);

    res = cr_str_to_nevra("crypto-utils-2.4.1-52.fc20:3.x86_64");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "crypto-utils");
    g_assert_cmpstr(res->version, ==, "2.4.1");
    g_assert_cmpstr(res->release, ==, "52.fc20");
    g_assert_cmpstr(res->epoch, ==, "3");
    g_assert_cmpstr(res->arch, ==, "x86_64");
    cr_nevra_free(res);

    res = cr_str_to_nevra("crypto-utils-2.4.1-52.fc20.x86_64:4");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "crypto-utils");
    g_assert_cmpstr(res->version, ==, "2.4.1");
    g_assert_cmpstr(res->release, ==, "52.fc20");
    g_assert_cmpstr(res->epoch, ==, "4");
    g_assert_cmpstr(res->arch, ==, "x86_64");
    cr_nevra_free(res);

    res = cr_str_to_nevra("a");
    g_assert(res);
    g_assert_cmpstr(res->name, ==, "a");
    g_assert(!res->version);
    g_assert(!res->release);
    g_assert(!res->epoch);
    g_assert(!res->arch);
    cr_nevra_free(res);
}


static void
test_cr_cmp_evr(void)
{
    int res;

    res = cr_cmp_evr(NULL, "2", "1",
                     "0", "2", "1");
    g_assert_cmpint(res, ==, 0);

    res = cr_cmp_evr(NULL, "2", "2",
                     "0", "2", "1");
    g_assert_cmpint(res, ==, 1);

    res = cr_cmp_evr("0", "2", "2",
                     "1", "2", "1");
    g_assert_cmpint(res, ==, -1);

    res = cr_cmp_evr(NULL, "22", "2",
                     "0", "2", "2");
    g_assert_cmpint(res, ==, 1);

    res = cr_cmp_evr(NULL, "13", "2",
                     "0", "2", "2");
    g_assert_cmpint(res, ==, 1);

    res = cr_cmp_evr(NULL, "55", "2",
                     NULL, "55", "2");
    g_assert_cmpint(res, ==, 0);

    res = cr_cmp_evr(NULL, "0", "2a",
                     "0", "0", "2b");
    g_assert_cmpint(res, ==, -1);

    res = cr_cmp_evr(NULL, "0", "2",
                     "0", NULL, "3");
    g_assert_cmpint(res, ==, 1);
}


static void
test_cr_cut_dirs(void)
{
    char *res;

    res = cr_cut_dirs(NULL, 1);
    g_assert_cmpstr(res, ==, NULL);

    res = cr_cut_dirs("", 1);
    g_assert_cmpstr(res, ==, "");

    res = cr_cut_dirs("foo.rpm", 1);
    g_assert_cmpstr(res, ==, "foo.rpm");

    res = cr_cut_dirs("/foo.rpm", 1);
    g_assert_cmpstr(res, ==, "foo.rpm");

    res = cr_cut_dirs("//foo.rpm", 1);
    g_assert_cmpstr(res, ==, "foo.rpm");

    res = cr_cut_dirs("///foo.rpm", 1);
    g_assert_cmpstr(res, ==, "foo.rpm");

    res = cr_cut_dirs("bar/foo.rpm", 1);
    g_assert_cmpstr(res, ==, "foo.rpm");

    res = cr_cut_dirs("/bar/foo.rpm", 1);
    g_assert_cmpstr(res, ==, "foo.rpm");

    res = cr_cut_dirs("bar//foo.rpm", 1);
    g_assert_cmpstr(res, ==, "foo.rpm");

    res = cr_cut_dirs("//bar//foo.rpm", 1);
    g_assert_cmpstr(res, ==, "foo.rpm");

    res = cr_cut_dirs("///a///b/foo.rpm", 1);
    g_assert_cmpstr(res, ==, "b/foo.rpm");

    res = cr_cut_dirs("a/b/c/foo.rpm", 1);
    g_assert_cmpstr(res, ==, "b/c/foo.rpm");

    res = cr_cut_dirs("a/b/c/foo.rpm", 2);
    g_assert_cmpstr(res, ==, "c/foo.rpm");

    res = cr_cut_dirs("a/b/c/foo.rpm", 3);
    g_assert_cmpstr(res, ==, "foo.rpm");

    res = cr_cut_dirs("a///b///c///foo.rpm", 3);
    g_assert_cmpstr(res, ==, "foo.rpm");
}


int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/misc/test_cr_str_to_evr",
            test_cr_str_to_evr);
    g_test_add_func("/misc/test_cr_str_to_evr_with_chunk",
            test_cr_str_to_evr_with_chunk);
    g_test_add_func("/misc/test_cr_is_primary",
            test_cr_is_primary);
    g_test_add_func("/misc/test_cr_get_header_byte_range",
            test_cr_get_header_byte_range);
    g_test_add_func("/misc/test_cr_get_filename",
            test_cr_get_filename);
    g_test_add("/misc/copyfiletest_test_empty_file",
            Copyfiletest, NULL, copyfiletest_setup,
            copyfiletest_test_empty_file, copyfiletest_teardown);
    g_test_add("/misc/copyfiletest_test_text_file",
            Copyfiletest, NULL, copyfiletest_setup,
            copyfiletest_test_text_file, copyfiletest_teardown);
    g_test_add("/misc/copyfiletest_test_binary_file",
            Copyfiletest, NULL, copyfiletest_setup,
            copyfiletest_test_binary_file, copyfiletest_teardown);
    g_test_add("/misc/copyfiletest_test_rewrite",
            Copyfiletest, NULL, copyfiletest_setup,
            copyfiletest_test_rewrite, copyfiletest_teardown);
    g_test_add("/misc/copyfiletest_test_corner_cases",
            Copyfiletest, NULL, copyfiletest_setup,
            copyfiletest_test_corner_cases, copyfiletest_teardown);
    g_test_add("/misc/compressfile_test_text_file",
            Copyfiletest, NULL, copyfiletest_setup,
            compressfile_test_text_file, copyfiletest_teardown);
    g_test_add("/misc/compressfile_with_stat_test_text_file",
            Copyfiletest, NULL, copyfiletest_setup,
            compressfile_with_stat_test_text_file, copyfiletest_teardown);
    g_test_add("/misc/compressfile_with_stat_test_gz_file_gz_output",
            Copyfiletest, NULL, copyfiletest_setup,
            compressfile_with_stat_test_gz_file_gz_output, copyfiletest_teardown);
    g_test_add("/misc/compressfile_test_gz_file_xz_output",
            Copyfiletest, NULL, copyfiletest_setup,
            compressfile_test_gz_file_xz_output, copyfiletest_teardown);
    g_test_add("/misc/compressfile_test_xz_file_gz_output",
            Copyfiletest, NULL, copyfiletest_setup,
            compressfile_test_xz_file_gz_output, copyfiletest_teardown);
    g_test_add("/misc/compressfile_test_sqlite_file_gz_output",
            Copyfiletest, NULL, copyfiletest_setup,
            compressfile_test_sqlite_file_gz_output, copyfiletest_teardown);
    g_test_add("/misc/decompressfile_with_stat_test_text_file",
            Copyfiletest, NULL, copyfiletest_setup,
            decompressfile_with_stat_test_text_file, copyfiletest_teardown);
    g_test_add("/misc/test_cr_better_copy_file_local",
            Copyfiletest, NULL, copyfiletest_setup,
            test_cr_better_copy_file_local, copyfiletest_teardown);
    g_test_add_func("/misc/test_cr_normalize_dir_path",
            test_cr_normalize_dir_path);
    g_test_add_func("/misc/test_cr_remove_dir",
            test_cr_remove_dir);
    g_test_add_func("/misc/test_cr_str_to_version",
            test_cr_str_to_version);
    g_test_add_func("/misc/test_cr_cmp_version_str",
            test_cr_cmp_version_str);
    g_test_add_func("/misc/test_cr_split_rpm_filename",
            test_cr_split_rpm_filename);
    g_test_add_func("/misc/test_cr_str_to_nevr",
            test_cr_str_to_nevr);
    g_test_add_func("/misc/test_cr_str_to_nevra",
            test_cr_str_to_nevra);
    g_test_add_func("/misc/test_cr_cmp_evr",
            test_cr_cmp_evr);
    g_test_add_func("/misc/test_cr_cut_dirs",
            test_cr_cut_dirs);

    return g_test_run();
}
