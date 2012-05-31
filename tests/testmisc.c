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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "createrepo/misc.h"


#define TEST_FILES_PATH         "test_data/test_files/"
#define EMPTY_FILE              TEST_FILES_PATH"empty_file"
#define TEXT_FILE               TEST_FILES_PATH"text_file"
#define BINARY_FILE             TEST_FILES_PATH"binary_file"

#define PACKAGE_01              "test_data/packages/super_kernel-6.0.1-2.x86_64.rpm"
#define PACKAGE_01_HEADER_START 280
#define PACKAGE_01_HEADER_END   2637

#define PACKAGE_02              "test_data/packages/fake_bash-1.1.1-1.x86_64.rpm"
#define PACKAGE_02_HEADER_START 280
#define PACKAGE_02_HEADER_END   2057

#define TMP_DIR_PATTERN         "/tmp/createrepo_test_XXXXXX"
#define NON_EXIST_FILE          "/tmp/foobarfile.which.should.not.exists"

#define VALID_URL_01    "http://google.com/index.html"
#define URL_FILENAME_01 "index.html"
#define INVALID_URL     "htp://foo.bar"


static void test_string_to_version(void)
{
    struct EVR evr;

    // V

    evr = string_to_version("5.0.0", NULL);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "5.0.0");
    g_assert_cmpstr(evr.release, ==, NULL);
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    evr = string_to_version("6.1", NULL);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "6.1");
    g_assert_cmpstr(evr.release, ==, NULL);
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    evr = string_to_version("7", NULL);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "7");
    g_assert_cmpstr(evr.release, ==, NULL);
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    // VR

    evr = string_to_version("5.0.0-2", NULL);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "5.0.0");
    g_assert_cmpstr(evr.release, ==, "2");
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    evr = string_to_version("6.1-3", NULL);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "6.1");
    g_assert_cmpstr(evr.release, ==, "3");
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    evr = string_to_version("7-4", NULL);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "7");
    g_assert_cmpstr(evr.release, ==, "4");
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    // EV

    evr = string_to_version("1:5.0.0", NULL);
    g_assert_cmpstr(evr.epoch, ==, "1");
    g_assert_cmpstr(evr.version, ==, "5.0.0");
    g_assert_cmpstr(evr.release, ==, NULL);
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    evr = string_to_version("2:6.1", NULL);
    g_assert_cmpstr(evr.epoch, ==, "2");
    g_assert_cmpstr(evr.version, ==, "6.1");
    g_assert_cmpstr(evr.release, ==, NULL);
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    evr = string_to_version("3:7", NULL);
    g_assert_cmpstr(evr.epoch, ==, "3");
    g_assert_cmpstr(evr.version, ==, "7");
    g_assert_cmpstr(evr.release, ==, NULL);
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    //EVR

    evr = string_to_version("1:5.0.0-11", NULL);
    g_assert_cmpstr(evr.epoch, ==, "1");
    g_assert_cmpstr(evr.version, ==, "5.0.0");
    g_assert_cmpstr(evr.release, ==, "11");
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    evr = string_to_version("2:6.1-22", NULL);
    g_assert_cmpstr(evr.epoch, ==, "2");
    g_assert_cmpstr(evr.version, ==, "6.1");
    g_assert_cmpstr(evr.release, ==, "22");
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    evr = string_to_version("3:7-33", NULL);
    g_assert_cmpstr(evr.epoch, ==, "3");
    g_assert_cmpstr(evr.version, ==, "7");
    g_assert_cmpstr(evr.release, ==, "33");
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    // Bad strings

    evr = string_to_version(":", NULL);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "");
    g_assert_cmpstr(evr.release, ==, NULL);
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    evr = string_to_version(":-", NULL);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "");
    g_assert_cmpstr(evr.release, ==, NULL);
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    // Really bad values

    evr = string_to_version(NULL, NULL);
    g_assert_cmpstr(evr.epoch, ==, NULL);
    g_assert_cmpstr(evr.version, ==, NULL);
    g_assert_cmpstr(evr.release, ==, NULL);
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    evr = string_to_version("", NULL);
    g_assert_cmpstr(evr.epoch, ==, NULL);
    g_assert_cmpstr(evr.version, ==, NULL);
    g_assert_cmpstr(evr.release, ==, NULL);
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    evr = string_to_version("-", NULL);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "");
    g_assert_cmpstr(evr.release, ==, NULL);
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    evr = string_to_version("-:", NULL);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "");
    g_assert_cmpstr(evr.release, ==, NULL);
    free(evr.epoch);
    free(evr.version);
    free(evr.release);

    evr = string_to_version("foo:bar", NULL);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "bar");
    g_assert_cmpstr(evr.release, ==, NULL);
    free(evr.epoch);
    free(evr.version);
    free(evr.release);
}


static void test_string_to_version_with_chunk(void)
{
    struct EVR evr;
    GStringChunk *chunk;
    chunk = g_string_chunk_new(512);

    // V

    evr = string_to_version("5.0.0", chunk);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "5.0.0");
    g_assert_cmpstr(evr.release, ==, NULL);

    evr = string_to_version("6.1", chunk);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "6.1");
    g_assert_cmpstr(evr.release, ==, NULL);

    evr = string_to_version("7", chunk);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "7");
    g_assert_cmpstr(evr.release, ==, NULL);

    // VR

    evr = string_to_version("5.0.0-2", chunk);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "5.0.0");
    g_assert_cmpstr(evr.release, ==, "2");

    evr = string_to_version("6.1-3", chunk);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "6.1");
    g_assert_cmpstr(evr.release, ==, "3");

    evr = string_to_version("7-4", chunk);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "7");
    g_assert_cmpstr(evr.release, ==, "4");

    // EV

    evr = string_to_version("1:5.0.0", chunk);
    g_assert_cmpstr(evr.epoch, ==, "1");
    g_assert_cmpstr(evr.version, ==, "5.0.0");
    g_assert_cmpstr(evr.release, ==, NULL);

    evr = string_to_version("2:6.1", chunk);
    g_assert_cmpstr(evr.epoch, ==, "2");
    g_assert_cmpstr(evr.version, ==, "6.1");
    g_assert_cmpstr(evr.release, ==, NULL);

    evr = string_to_version("3:7", chunk);
    g_assert_cmpstr(evr.epoch, ==, "3");
    g_assert_cmpstr(evr.version, ==, "7");
    g_assert_cmpstr(evr.release, ==, NULL);

    //EVR

    evr = string_to_version("1:5.0.0-11", chunk);
    g_assert_cmpstr(evr.epoch, ==, "1");
    g_assert_cmpstr(evr.version, ==, "5.0.0");
    g_assert_cmpstr(evr.release, ==, "11");

    evr = string_to_version("2:6.1-22", chunk);
    g_assert_cmpstr(evr.epoch, ==, "2");
    g_assert_cmpstr(evr.version, ==, "6.1");
    g_assert_cmpstr(evr.release, ==, "22");

    evr = string_to_version("3:7-33", chunk);
    g_assert_cmpstr(evr.epoch, ==, "3");
    g_assert_cmpstr(evr.version, ==, "7");
    g_assert_cmpstr(evr.release, ==, "33");

    // Bad strings

    evr = string_to_version(":", chunk);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "");
    g_assert_cmpstr(evr.release, ==, NULL);

    evr = string_to_version(":-", chunk);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "");
    g_assert_cmpstr(evr.release, ==, NULL);

    // Really bad values

    evr = string_to_version(NULL, chunk);
    g_assert_cmpstr(evr.epoch, ==, NULL);
    g_assert_cmpstr(evr.version, ==, NULL);
    g_assert_cmpstr(evr.release, ==, NULL);

    evr = string_to_version("", chunk);
    g_assert_cmpstr(evr.epoch, ==, NULL);
    g_assert_cmpstr(evr.version, ==, NULL);
    g_assert_cmpstr(evr.release, ==, NULL);

    evr = string_to_version("-", chunk);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "");
    g_assert_cmpstr(evr.release, ==, NULL);

    evr = string_to_version("-:", chunk);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "");
    g_assert_cmpstr(evr.release, ==, NULL);

    evr = string_to_version("foo:bar", chunk);
    g_assert_cmpstr(evr.epoch, ==, "0");
    g_assert_cmpstr(evr.version, ==, "bar");
    g_assert_cmpstr(evr.release, ==, NULL);

    g_string_chunk_free(chunk);
}


static void test_is_primary(void)
{
    g_assert(is_primary("/etc/foobar"));
    g_assert(is_primary("/etc/"));
    g_assert(!is_primary("/foo/etc/foobar"));
    g_assert(!is_primary("/tmp/etc/"));

    g_assert(is_primary("/sbin/foobar"));
    g_assert(is_primary("/bin/bash"));
    g_assert(is_primary("/usr/sbin/foobar"));
    g_assert(is_primary("/usr/bin/foobar"));
    g_assert(is_primary("/usr/share/locale/bin/LC_MESSAGES"));  // Sad, but we have to reflect yum behavior
    g_assert(is_primary("/usr/share/man/bin/man0p"));           // my heart is bleeding
    g_assert(!is_primary("/foo/bindir"));
    g_assert(!is_primary("/foo/sbindir"));

    g_assert(is_primary("/usr/lib/sendmail"));
    g_assert(!is_primary("/tmp/usr/lib/sendmail"));

    g_assert(!is_primary(""));
}


static void test_compute_file_checksum(void)
{
    char *checksum;

    checksum = compute_file_checksum(EMPTY_FILE, PKG_CHECKSUM_MD5);
    g_assert_cmpstr(checksum, ==, "d41d8cd98f00b204e9800998ecf8427e");
    g_free(checksum);
    checksum = compute_file_checksum(EMPTY_FILE, PKG_CHECKSUM_SHA1);
    g_assert_cmpstr(checksum, ==, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    g_free(checksum);
    checksum = compute_file_checksum(EMPTY_FILE, PKG_CHECKSUM_SHA256);
    g_assert_cmpstr(checksum, ==, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    g_free(checksum);

    checksum = compute_file_checksum(TEXT_FILE, PKG_CHECKSUM_MD5);
    g_assert_cmpstr(checksum, ==, "d6d4da5c15f8fe7570ce6ab6b3503916");
    g_free(checksum);
    checksum = compute_file_checksum(TEXT_FILE, PKG_CHECKSUM_SHA1);
    g_assert_cmpstr(checksum, ==, "da048ee8fabfbef1b3d6d3f5a4be20029eecec77");
    g_free(checksum);
    checksum = compute_file_checksum(TEXT_FILE, PKG_CHECKSUM_SHA256);
    g_assert_cmpstr(checksum, ==, "2f395bdfa2750978965e4781ddf224c89646c7d7a1569b7ebb023b170f7bd8bb");
    g_free(checksum);

    checksum = compute_file_checksum(BINARY_FILE, PKG_CHECKSUM_MD5);
    g_assert_cmpstr(checksum, ==, "4f8b033d7a402927a20c9328fc0e0f46");
    g_free(checksum);
    checksum = compute_file_checksum(BINARY_FILE, PKG_CHECKSUM_SHA1);
    g_assert_cmpstr(checksum, ==, "3539fb660a41846352ac4fa9076d168a3c77070b");
    g_free(checksum);
    checksum = compute_file_checksum(BINARY_FILE, PKG_CHECKSUM_SHA256);
    g_assert_cmpstr(checksum, ==, "bf68e32ad78cea8287be0f35b74fa3fecd0eaa91770b48f1a7282b015d6d883e");
    g_free(checksum);

    // Corner cases

    checksum = compute_file_checksum(BINARY_FILE, 244);
    g_assert(!checksum);

    checksum = compute_file_checksum(NON_EXIST_FILE, PKG_CHECKSUM_MD5);
    g_assert(!checksum);

    checksum = compute_file_checksum(NULL, PKG_CHECKSUM_MD5);
    g_assert(!checksum);

}


static void test_get_header_byte_range(void)
{
    struct HeaderRangeStruct hdr_range;

    hdr_range = get_header_byte_range(PACKAGE_01);
    g_assert_cmpuint(hdr_range.start, ==, PACKAGE_01_HEADER_START);
    g_assert_cmpuint(hdr_range.end, ==, PACKAGE_01_HEADER_END);

    hdr_range = get_header_byte_range(PACKAGE_02);
    g_assert_cmpuint(hdr_range.start, ==, PACKAGE_02_HEADER_START);
    g_assert_cmpuint(hdr_range.end, ==, PACKAGE_02_HEADER_END);

    hdr_range = get_header_byte_range(NON_EXIST_FILE);
    g_assert_cmpuint(hdr_range.start, ==, 0);
    g_assert_cmpuint(hdr_range.end, ==, 0);
}


static void test_get_checksum_name_str(void)
{
    const char *checksum_name;

    checksum_name = get_checksum_name_str(PKG_CHECKSUM_MD5);
    g_assert_cmpstr(checksum_name, ==, "md5");

    checksum_name = get_checksum_name_str(PKG_CHECKSUM_SHA1);
    g_assert_cmpstr(checksum_name, ==, "sha1");

    checksum_name = get_checksum_name_str(PKG_CHECKSUM_SHA256);
    g_assert_cmpstr(checksum_name, ==, "sha256");

    checksum_name = get_checksum_name_str(244);
    g_assert_cmpstr(checksum_name, ==, NULL);
}


static void test_get_filename(void)
{
    char *filename;

    filename = get_filename("/fooo/bar/file");
    g_assert_cmpstr(filename, ==, "file");

    filename = get_filename("///fooo///bar///file");
    g_assert_cmpstr(filename, ==, "file");

    filename = get_filename("/file");
    g_assert_cmpstr(filename, ==, "file");

    filename = get_filename("///file");
    g_assert_cmpstr(filename, ==, "file");

    filename = get_filename("file");
    g_assert_cmpstr(filename, ==, "file");

    filename = get_filename("./file");
    g_assert_cmpstr(filename, ==, "file");

    filename = get_filename("");
    g_assert_cmpstr(filename, ==, "");

    filename = get_filename(NULL);
    g_assert_cmpstr(filename, ==, NULL);
}


#define DST_FILE        "b"

typedef struct {
    gchar *tmp_dir;
    gchar *dst_file;
} Copyfiletest;


static void copyfiletest_setup(Copyfiletest *copyfiletest, gconstpointer test_data)
{
    UNUSED(test_data);
    copyfiletest->tmp_dir = g_strdup(TMP_DIR_PATTERN);
    mkdtemp(copyfiletest->tmp_dir);
    copyfiletest->dst_file = g_strconcat(copyfiletest->tmp_dir, "/", DST_FILE, NULL);
}


static void copyfiletest_teardown(Copyfiletest *copyfiletest, gconstpointer test_data)
{
    UNUSED(test_data);
    remove(copyfiletest->dst_file);
    rmdir(copyfiletest->tmp_dir);
    g_free(copyfiletest->tmp_dir);
    g_free(copyfiletest->dst_file);
}


static void copyfiletest_test_empty_file(Copyfiletest *copyfiletest, gconstpointer test_data)
{
    UNUSED(test_data);
    int ret;
    char *checksum;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = copy_file(EMPTY_FILE, copyfiletest->dst_file);
    g_assert_cmpint(ret, ==, CR_COPY_OK);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR));
    checksum = compute_file_checksum(copyfiletest->dst_file, PKG_CHECKSUM_SHA256);
    g_assert_cmpstr(checksum, ==, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    g_free(checksum);
}


static void copyfiletest_test_text_file(Copyfiletest *copyfiletest, gconstpointer test_data)
{
    UNUSED(test_data);
    int ret;
    char *checksum;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = copy_file(TEXT_FILE, copyfiletest->dst_file);
    g_assert_cmpint(ret, ==, CR_COPY_OK);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR));
    checksum = compute_file_checksum(copyfiletest->dst_file, PKG_CHECKSUM_SHA256);
    g_assert_cmpstr(checksum, ==, "2f395bdfa2750978965e4781ddf224c89646c7d7a1569b7ebb023b170f7bd8bb");
    g_free(checksum);
}


static void copyfiletest_test_binary_file(Copyfiletest *copyfiletest, gconstpointer test_data)
{
    UNUSED(test_data);
    int ret;
    char *checksum;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = copy_file(BINARY_FILE, copyfiletest->dst_file);
    g_assert_cmpint(ret, ==, CR_COPY_OK);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR));
    checksum = compute_file_checksum(copyfiletest->dst_file, PKG_CHECKSUM_SHA256);
    g_assert_cmpstr(checksum, ==, "bf68e32ad78cea8287be0f35b74fa3fecd0eaa91770b48f1a7282b015d6d883e");
    g_free(checksum);
}


static void copyfiletest_test_rewrite(Copyfiletest *copyfiletest, gconstpointer test_data)
{
    UNUSED(test_data);
    int ret;
    char *checksum;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = copy_file(BINARY_FILE, copyfiletest->dst_file);
    g_assert_cmpint(ret, ==, CR_COPY_OK);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR));
    checksum = compute_file_checksum(copyfiletest->dst_file, PKG_CHECKSUM_SHA256);
    g_assert_cmpstr(checksum, ==, "bf68e32ad78cea8287be0f35b74fa3fecd0eaa91770b48f1a7282b015d6d883e");
    g_free(checksum);

    ret = copy_file(TEXT_FILE, copyfiletest->dst_file);
    g_assert_cmpint(ret, ==, CR_COPY_OK);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR));
    checksum = compute_file_checksum(copyfiletest->dst_file, PKG_CHECKSUM_SHA256);
    g_assert_cmpstr(checksum, ==, "2f395bdfa2750978965e4781ddf224c89646c7d7a1569b7ebb023b170f7bd8bb");
    g_free(checksum);
}


static void copyfiletest_test_corner_cases(Copyfiletest *copyfiletest, gconstpointer test_data)
{
    UNUSED(test_data);
    int ret;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = copy_file(NON_EXIST_FILE, copyfiletest->dst_file);
    g_assert_cmpint(ret, ==, CR_COPY_ERR);
    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
}



static void test_download_valid_url_1(Copyfiletest *copyfiletest, gconstpointer test_data)
{
    UNUSED(test_data);

    char *error = NULL;
    CURL *handle = curl_easy_init();

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    download(handle, VALID_URL_01, copyfiletest->dst_file, &error);
    curl_easy_cleanup(handle);
    g_assert(!error);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
}



static void test_download_valid_url_2(Copyfiletest *copyfiletest, gconstpointer test_data)
{
    UNUSED(test_data);

    char *error = NULL;
    CURL *handle = curl_easy_init();

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    download(handle, VALID_URL_01, copyfiletest->tmp_dir, &error);
    curl_easy_cleanup(handle);
    g_assert(!error);
    char *dst = g_strconcat(copyfiletest->tmp_dir, "/", URL_FILENAME_01, NULL);
    g_assert(g_file_test(dst, G_FILE_TEST_EXISTS));
    g_free(dst);
}


static void test_download_invalid_url(Copyfiletest *copyfiletest, gconstpointer test_data)
{
    UNUSED(test_data);

    char *error = NULL;
    CURL *handle = curl_easy_init();

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    download(handle, INVALID_URL, copyfiletest->dst_file, &error);
    curl_easy_cleanup(handle);
    g_assert(error);
    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
}



static void test_better_copy_file_local(Copyfiletest *copyfiletest, gconstpointer test_data)
{
    UNUSED(test_data);
    int ret;
    char *checksum;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = better_copy_file(BINARY_FILE, copyfiletest->dst_file);
    g_assert_cmpint(ret, ==, CR_COPY_OK);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR));
    checksum = compute_file_checksum(copyfiletest->dst_file, PKG_CHECKSUM_SHA256);
    g_assert_cmpstr(checksum, ==, "bf68e32ad78cea8287be0f35b74fa3fecd0eaa91770b48f1a7282b015d6d883e");
    g_free(checksum);
}



static void test_better_copy_file_url(Copyfiletest *copyfiletest, gconstpointer test_data)
{
    UNUSED(test_data);
    int ret;

    g_assert(!g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS));
    ret = better_copy_file(VALID_URL_01, copyfiletest->dst_file);
    g_assert_cmpint(ret, ==, CR_COPY_OK);
    g_assert(g_file_test(copyfiletest->dst_file, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR));
}



static void test_remove_dir(void)
{
    char *tmp_dir;
    char *subdir01, *subdir02, *subsubdir011, *subsubsubdir0111;
    gchar *tmp_file_1, *tmp_file_2, *tmp_file_3;

    tmp_dir = g_strdup(TMP_DIR_PATTERN);
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

    remove_dir(tmp_dir);

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


static void test_normalize_dir_path(void)
{
    char *normalized;

    normalized = normalize_dir_path("/////////");
    g_assert_cmpstr(normalized, ==, "/");
    g_free(normalized);

    normalized = normalize_dir_path("///foo///bar///");
    g_assert_cmpstr(normalized, ==, "///foo///bar/");
    g_free(normalized);

    normalized = normalize_dir_path("bar");
    g_assert_cmpstr(normalized, ==, "bar/");
    g_free(normalized);

    normalized = normalize_dir_path(".////////////bar");
    g_assert_cmpstr(normalized, ==, ".////////////bar/");
    g_free(normalized);

    normalized = normalize_dir_path("////////////bar");
    g_assert_cmpstr(normalized, ==, "////////////bar/");
    g_free(normalized);

    normalized = normalize_dir_path("bar//////");
    g_assert_cmpstr(normalized, ==, "bar/");
    g_free(normalized);

    normalized = normalize_dir_path("");
    g_assert_cmpstr(normalized, ==, "./");
    g_free(normalized);

    normalized = normalize_dir_path(NULL);
    g_assert_cmpstr(normalized, ==, NULL);
    g_free(normalized);
}


static void test_str_to_version(void)
{
    struct Version ver;

    ver = str_to_version(NULL);
    g_assert_cmpint(ver.version, ==, 0);
    g_assert_cmpint(ver.release, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = str_to_version("");
    g_assert_cmpint(ver.version, ==, 0);
    g_assert_cmpint(ver.release, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = str_to_version("abcd");
    g_assert_cmpint(ver.version, ==, 0);
    g_assert_cmpint(ver.release, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "abcd");
    g_free(ver.suffix);

    ver = str_to_version("0.0.0");
    g_assert_cmpint(ver.version, ==, 0);
    g_assert_cmpint(ver.release, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = str_to_version("9");
    g_assert_cmpint(ver.version, ==, 9);
    g_assert_cmpint(ver.release, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = str_to_version("3beta");
    g_assert_cmpint(ver.version, ==, 3);
    g_assert_cmpint(ver.release, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "beta");
    g_free(ver.suffix);

    ver = str_to_version("5.2gamma");
    g_assert_cmpint(ver.version, ==, 5);
    g_assert_cmpint(ver.release, ==, 2);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "gamma");
    g_free(ver.suffix);

    ver = str_to_version("0.0.0b");
    g_assert_cmpint(ver.version, ==, 0);
    g_assert_cmpint(ver.release, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "b");
    g_free(ver.suffix);

    ver = str_to_version("2.3.4");
    g_assert_cmpint(ver.version, ==, 2);
    g_assert_cmpint(ver.release, ==, 3);
    g_assert_cmpint(ver.patch, ==, 4);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = str_to_version("11.33.123");
    g_assert_cmpint(ver.version, ==, 11);
    g_assert_cmpint(ver.release, ==, 33);
    g_assert_cmpint(ver.patch, ==, 123);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = str_to_version("1234567.0987654.45678");
    g_assert_cmpint(ver.version, ==, 1234567);
    g_assert_cmpint(ver.release, ==, 987654);
    g_assert_cmpint(ver.patch, ==, 45678);
    g_assert_cmpstr(ver.suffix, ==, NULL);

    ver = str_to_version("1.0.2i");
    g_assert_cmpint(ver.version, ==, 1);
    g_assert_cmpint(ver.release, ==, 0);
    g_assert_cmpint(ver.patch, ==, 2);
    g_assert_cmpstr(ver.suffix, ==, "i");
    g_free(ver.suffix);

    ver = str_to_version("1..3");
    g_assert_cmpint(ver.version, ==, 1);
    g_assert_cmpint(ver.release, ==, 0);
    g_assert_cmpint(ver.patch, ==, 3);
    g_assert_cmpstr(ver.suffix, ==, NULL);
    g_free(ver.suffix);

    ver = str_to_version("..alpha");
    g_assert_cmpint(ver.version, ==, 0);
    g_assert_cmpint(ver.release, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "alpha");
    g_free(ver.suffix);

    ver = str_to_version("alpha");
    g_assert_cmpint(ver.version, ==, 0);
    g_assert_cmpint(ver.release, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "alpha");
    g_free(ver.suffix);

    ver = str_to_version("1-2-3");
    g_assert_cmpint(ver.version, ==, 1);
    g_assert_cmpint(ver.release, ==, 0);
    g_assert_cmpint(ver.patch, ==, 0);
    g_assert_cmpstr(ver.suffix, ==, "-2-3");
    g_free(ver.suffix);
}


static void test_cmp_version_string(void)
{
    int ret;

    ret = cmp_version_string(NULL, NULL);
    g_assert_cmpint(ret, ==, 0);

    ret = cmp_version_string("", "");
    g_assert_cmpint(ret, ==, 0);

    ret = cmp_version_string(NULL, "");
    g_assert_cmpint(ret, ==, 0);

    ret = cmp_version_string("", NULL);
    g_assert_cmpint(ret, ==, 0);

    ret = cmp_version_string("3", "3");
    g_assert_cmpint(ret, ==, 0);

    ret = cmp_version_string("1", "2");
    g_assert_cmpint(ret, ==, 2);

    ret = cmp_version_string("99", "8");
    g_assert_cmpint(ret, ==, 1);

    ret = cmp_version_string("5.4.3", "5.4.3");
    g_assert_cmpint(ret, ==, 0);

    ret = cmp_version_string("5.3.2", "5.3.1");
    g_assert_cmpint(ret, ==, 1);

    ret = cmp_version_string("5.3.5", "5.3.6");
    g_assert_cmpint(ret, ==, 2);

    ret = cmp_version_string("6.3.2a", "6.3.2b");
    g_assert_cmpint(ret, ==, 2);

    ret = cmp_version_string("6.3.2azb", "6.3.2abc");
    g_assert_cmpint(ret, ==, 1);

    ret = cmp_version_string("1.2beta", "1.2beta");
    g_assert_cmpint(ret, ==, 0);

    ret = cmp_version_string("n", "n");
    g_assert_cmpint(ret, ==, 0);

    ret = cmp_version_string("c", "b");
    g_assert_cmpint(ret, ==,  1);

    ret = cmp_version_string("c", "f");
    g_assert_cmpint(ret, ==, 2);
}



int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/misc/test_string_to_version", test_string_to_version);
    g_test_add_func("/misc/test_string_to_version_with_chunk", test_string_to_version_with_chunk);
    g_test_add_func("/misc/test_is_primary", test_is_primary);
    g_test_add_func("/misc/test_compute_file_checksum", test_compute_file_checksum);
    g_test_add_func("/misc/test_get_header_byte_range", test_get_header_byte_range);
    g_test_add_func("/misc/test_get_checksum_name_str", test_get_checksum_name_str);
    g_test_add_func("/misc/test_get_filename", test_get_filename);
    g_test_add("/misc/copyfiletest_test_empty_file", Copyfiletest, NULL, copyfiletest_setup, copyfiletest_test_empty_file, copyfiletest_teardown);
    g_test_add("/misc/copyfiletest_test_text_file", Copyfiletest, NULL, copyfiletest_setup, copyfiletest_test_text_file, copyfiletest_teardown);
    g_test_add("/misc/copyfiletest_test_binary_file", Copyfiletest, NULL, copyfiletest_setup, copyfiletest_test_binary_file, copyfiletest_teardown);
    g_test_add("/misc/copyfiletest_test_rewrite", Copyfiletest, NULL, copyfiletest_setup, copyfiletest_test_rewrite, copyfiletest_teardown);
    g_test_add("/misc/copyfiletest_test_corner_cases", Copyfiletest, NULL, copyfiletest_setup, copyfiletest_test_corner_cases, copyfiletest_teardown);
    g_test_add("/misc/test_download_valid_url_1", Copyfiletest, NULL, copyfiletest_setup, test_download_valid_url_1, copyfiletest_teardown);
    g_test_add("/misc/test_download_valid_url_2", Copyfiletest, NULL, copyfiletest_setup, test_download_valid_url_2, copyfiletest_teardown);
    g_test_add("/misc/test_download_invalid_url", Copyfiletest, NULL, copyfiletest_setup, test_download_invalid_url, copyfiletest_teardown);
    g_test_add("/misc/test_better_copy_file_local", Copyfiletest, NULL, copyfiletest_setup, test_better_copy_file_local, copyfiletest_teardown);
    g_test_add("/misc/test_better_copy_file_url", Copyfiletest, NULL, copyfiletest_setup, test_better_copy_file_url, copyfiletest_teardown);
    g_test_add_func("/misc/test_normalize_dir_path", test_normalize_dir_path);
    g_test_add_func("/misc/test_remove_dir", test_remove_dir);
    g_test_add_func("/misc/test_str_to_version", test_str_to_version);
    g_test_add_func("/misc/test_cmp_version_string", test_cmp_version_string);

    return g_test_run();
}
