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
#include "createrepo/error.h"
#include "createrepo/misc.h"
#include "createrepo/compression_wrapper.h"

#define TMP_FILE_PATTERN                        "test_XXXXXX.txt"

#define COMPRESSED_BUFFER_LEN                   512

#define FILE_COMPRESSED_0_CONTENT               ""
#define FILE_COMPRESSED_0_CONTENT_LEN           0
#define FILE_COMPRESSED_0_PLAIN                 TEST_COMPRESSED_FILES_PATH"/00_plain.txt"
#define FILE_COMPRESSED_0_GZ                    TEST_COMPRESSED_FILES_PATH"/00_plain.txt.gz"
#define FILE_COMPRESSED_0_BZ2                   TEST_COMPRESSED_FILES_PATH"/00_plain.txt.bz2"
#define FILE_COMPRESSED_0_XZ                    TEST_COMPRESSED_FILES_PATH"/00_plain.txt.xz"
#define FILE_COMPRESSED_0_PLAIN_BAD_SUFFIX      TEST_COMPRESSED_FILES_PATH"/00_plain.foo0"
#define FILE_COMPRESSED_0_GZ_BAD_SUFFIX         TEST_COMPRESSED_FILES_PATH"/00_plain.foo1"
#define FILE_COMPRESSED_0_BZ2_BAD_SUFFIX        TEST_COMPRESSED_FILES_PATH"/00_plain.foo2"
#define FILE_COMPRESSED_0_XZ_BAD_SUFFIX         TEST_COMPRESSED_FILES_PATH"/00_plain.foo3"

#define FILE_COMPRESSED_1_CONTENT               "foobar foobar foobar foobar test test\nfolkjsaflkjsadokf\n"
#define FILE_COMPRESSED_1_CONTENT_LEN           56
#define FILE_COMPRESSED_1_PLAIN                 TEST_COMPRESSED_FILES_PATH"/01_plain.txt"
#define FILE_COMPRESSED_1_GZ                    TEST_COMPRESSED_FILES_PATH"/01_plain.txt.gz"
#define FILE_COMPRESSED_1_BZ2                   TEST_COMPRESSED_FILES_PATH"/01_plain.txt.bz2"
#define FILE_COMPRESSED_1_XZ                    TEST_COMPRESSED_FILES_PATH"/01_plain.txt.xz"
#define FILE_COMPRESSED_1_PLAIN_BAD_SUFFIX      TEST_COMPRESSED_FILES_PATH"/01_plain.foo0"
#define FILE_COMPRESSED_1_GZ_BAD_SUFFIX         TEST_COMPRESSED_FILES_PATH"/01_plain.foo1"
#define FILE_COMPRESSED_1_BZ2_BAD_SUFFIX        TEST_COMPRESSED_FILES_PATH"/01_plain.foo2"
#define FILE_COMPRESSED_1_XZ_BAD_SUFFIX         TEST_COMPRESSED_FILES_PATH"/01_plain.foo3"


static void
test_cr_contentstat(void)
{
    char *checksum;
    cr_ContentStat *cs = NULL;
    GError *tmp_err = NULL;

    cs = cr_contentstat_new(CR_CHECKSUM_UNKNOWN, NULL);
    g_assert(cs);
    checksum = cr_contentstat_free(cs, NULL);
    g_assert(!checksum);
    cs = NULL;

    cs = cr_contentstat_new(CR_CHECKSUM_UNKNOWN, &tmp_err);
    g_assert(cs);
    g_assert(!tmp_err);
    checksum = cr_contentstat_free(cs, &tmp_err);
    g_assert(!checksum);
    g_assert(!tmp_err);
    cs = NULL;

    cs = cr_contentstat_new(CR_CHECKSUM_SHA256, NULL);
    g_assert(cs);
    checksum = cr_contentstat_free(cs, NULL);
    g_assert(checksum);
    g_assert_cmpstr(checksum, ==, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649"
                                  "b934ca495991b7852b855");
    g_free(checksum);
    cs = NULL;
    checksum = NULL;

    cs = cr_contentstat_new(CR_CHECKSUM_SHA256, &tmp_err);
    g_assert(cs);
    g_assert(!tmp_err);
    checksum = cr_contentstat_free(cs, &tmp_err);
    g_assert(!tmp_err);
    g_assert(checksum);
    g_assert_cmpstr(checksum, ==, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649"
                                  "b934ca495991b7852b855");
    g_free(checksum);
    cs = NULL;
    checksum = NULL;
}

static void
test_cr_compression_suffix(void)
{
    const char *suffix;

    suffix = cr_compression_suffix(CR_CW_AUTO_DETECT_COMPRESSION);
    g_assert(!suffix);

    suffix = cr_compression_suffix(CR_CW_UNKNOWN_COMPRESSION);
    g_assert(!suffix);

    suffix = cr_compression_suffix(CR_CW_NO_COMPRESSION);
    g_assert(!suffix);

    suffix = cr_compression_suffix(CR_CW_GZ_COMPRESSION);
    g_assert_cmpstr(suffix, ==, ".gz");

    suffix = cr_compression_suffix(CR_CW_BZ2_COMPRESSION);
    g_assert_cmpstr(suffix, ==, ".bz2");

    suffix = cr_compression_suffix(CR_CW_XZ_COMPRESSION);
    g_assert_cmpstr(suffix, ==, ".xz");
}

static void
test_cr_detect_compression(void)
{
    cr_CompressionType ret;
    GError *tmp_err = NULL;

    // Plain

    ret = cr_detect_compression(FILE_COMPRESSED_0_PLAIN, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_NO_COMPRESSION);
    g_assert(!tmp_err);
    ret = cr_detect_compression(FILE_COMPRESSED_1_PLAIN, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_NO_COMPRESSION);
    g_assert(!tmp_err);

    // Gz

    ret = cr_detect_compression(FILE_COMPRESSED_0_GZ, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_GZ_COMPRESSION);
    g_assert(!tmp_err);
    ret = cr_detect_compression(FILE_COMPRESSED_1_GZ, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_GZ_COMPRESSION);
    g_assert(!tmp_err);

    // Bz2

    ret = cr_detect_compression(FILE_COMPRESSED_0_BZ2, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_BZ2_COMPRESSION);
    g_assert(!tmp_err);
    ret = cr_detect_compression(FILE_COMPRESSED_1_BZ2, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_BZ2_COMPRESSION);
    g_assert(!tmp_err);

    // Xz

    ret = cr_detect_compression(FILE_COMPRESSED_0_XZ, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_XZ_COMPRESSION);
    g_assert(!tmp_err);
    ret = cr_detect_compression(FILE_COMPRESSED_1_XZ, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_XZ_COMPRESSION);
    g_assert(!tmp_err);
}


static void
test_cr_detect_compression_bad_suffix(void)
{
    cr_CompressionType ret;
    GError *tmp_err = NULL;

    // Plain

    ret = cr_detect_compression(FILE_COMPRESSED_0_PLAIN_BAD_SUFFIX, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_NO_COMPRESSION);
    g_assert(!tmp_err);
    ret = cr_detect_compression(FILE_COMPRESSED_1_PLAIN_BAD_SUFFIX, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_NO_COMPRESSION);
    g_assert(!tmp_err);

    // Gz

    ret = cr_detect_compression(FILE_COMPRESSED_0_GZ_BAD_SUFFIX, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_GZ_COMPRESSION);
    g_assert(!tmp_err);
    ret = cr_detect_compression(FILE_COMPRESSED_1_GZ_BAD_SUFFIX, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_GZ_COMPRESSION);
    g_assert(!tmp_err);

    // Bz2

    ret = cr_detect_compression(FILE_COMPRESSED_0_BZ2_BAD_SUFFIX, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_BZ2_COMPRESSION);
    g_assert(!tmp_err);
    ret = cr_detect_compression(FILE_COMPRESSED_1_BZ2_BAD_SUFFIX, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_BZ2_COMPRESSION);
    g_assert(!tmp_err);

    // Xz

    ret = cr_detect_compression(FILE_COMPRESSED_0_XZ_BAD_SUFFIX, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_XZ_COMPRESSION);
    g_assert(!tmp_err);
    ret = cr_detect_compression(FILE_COMPRESSED_1_XZ_BAD_SUFFIX, &tmp_err);
    g_assert_cmpint(ret, ==, CR_CW_XZ_COMPRESSION);
    g_assert(!tmp_err);
}


void
test_helper_cw_input(const char *filename,
                     cr_CompressionType ctype,
                     const char *content,
                     int len)
{
    int ret;
    CR_FILE *file;
    char buffer[COMPRESSED_BUFFER_LEN+1];
    GError *tmp_err = NULL;

    file = cr_open(filename, CR_CW_MODE_READ, ctype, &tmp_err);
    g_assert(file);
    g_assert(!tmp_err);

    ret = cr_read(file, buffer, COMPRESSED_BUFFER_LEN, &tmp_err);
    g_assert_cmpint(ret, ==, len);
    g_assert(!tmp_err);

    buffer[ret] = '\0';
    g_assert_cmpstr(buffer, ==, content);
    ret = cr_close(file, &tmp_err);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert(!tmp_err);
}


static void
test_cr_read_with_autodetection(void)
{
    // Plain

    test_helper_cw_input(FILE_COMPRESSED_0_PLAIN, CR_CW_AUTO_DETECT_COMPRESSION,
            FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_input(FILE_COMPRESSED_1_PLAIN, CR_CW_AUTO_DETECT_COMPRESSION,
            FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);

    // Gz

    test_helper_cw_input(FILE_COMPRESSED_0_GZ, CR_CW_AUTO_DETECT_COMPRESSION,
            FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_input(FILE_COMPRESSED_1_GZ, CR_CW_AUTO_DETECT_COMPRESSION,
            FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);

    // Bzip2

    test_helper_cw_input(FILE_COMPRESSED_0_BZ2, CR_CW_AUTO_DETECT_COMPRESSION,
            FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_input(FILE_COMPRESSED_1_BZ2, CR_CW_AUTO_DETECT_COMPRESSION,
            FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);

    // Xz

    test_helper_cw_input(FILE_COMPRESSED_0_XZ, CR_CW_AUTO_DETECT_COMPRESSION,
            FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_input(FILE_COMPRESSED_1_XZ, CR_CW_AUTO_DETECT_COMPRESSION,
            FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
}


typedef struct {
    gchar *tmp_filename;
} Outputtest;


static void
outputtest_setup(Outputtest *outputtest, gconstpointer test_data)
{
    CR_UNUSED(test_data);
    int fd;

    fd = g_file_open_tmp(TMP_FILE_PATTERN, &(outputtest->tmp_filename), NULL);
    close(fd);
}


static void
outputtest_teardown(Outputtest *outputtest, gconstpointer test_data)
{
    CR_UNUSED(test_data);

    if (outputtest->tmp_filename) {
        remove(outputtest->tmp_filename);
        g_free(outputtest->tmp_filename);
    }
}


#define OUTPUT_TYPE_WRITE       0
#define OUTPUT_TYPE_PUTS        1
#define OUTPUT_TYPE_PRINTF      2

void
test_helper_cw_output(int type,
                      const char *filename,
                      cr_CompressionType ctype,
                      const char *content,
                      int len)
{
    int ret;
    CR_FILE *file;
    GError *tmp_err = NULL;
    file = cr_open(filename, CR_CW_MODE_WRITE, ctype, &tmp_err);
    g_assert(file);
    g_assert(!tmp_err);

    switch(type) {
        case OUTPUT_TYPE_WRITE:
            ret = cr_write(file, content, len, &tmp_err);
            g_assert_cmpint(ret, ==, len);
            g_assert(!tmp_err);
            break;

        case OUTPUT_TYPE_PUTS:
            ret = cr_puts(file, content, &tmp_err);
            g_assert_cmpint(ret, ==, CRE_OK);
            g_assert(!tmp_err);
            break;

        case OUTPUT_TYPE_PRINTF:
            ret = cr_printf(&tmp_err, file, "%s", content);
            g_assert_cmpint(ret, ==, len);
            g_assert(!tmp_err);
            break;

        default:
            break;
    }

    ret = cr_close(file, &tmp_err);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert(!tmp_err);

    // Read and compare

    test_helper_cw_input(filename, ctype, content, len);
}


static void
outputtest_cw_output(Outputtest *outputtest, gconstpointer test_data)
{
    CR_UNUSED(test_data);

    // Plain

    printf("Testing - plain\nwrite()\n");
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename,
                          CR_CW_NO_COMPRESSION, FILE_COMPRESSED_0_CONTENT,
                          FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename,
                          CR_CW_NO_COMPRESSION, FILE_COMPRESSED_1_CONTENT,
                          FILE_COMPRESSED_1_CONTENT_LEN);
    printf("puts()\n");
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename,
                          CR_CW_NO_COMPRESSION, FILE_COMPRESSED_0_CONTENT,
                          FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename,
                          CR_CW_NO_COMPRESSION, FILE_COMPRESSED_1_CONTENT,
                          FILE_COMPRESSED_1_CONTENT_LEN);
    printf("printf()\n");
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename,
                          CR_CW_NO_COMPRESSION, FILE_COMPRESSED_0_CONTENT,
                          FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename,
                          CR_CW_NO_COMPRESSION, FILE_COMPRESSED_1_CONTENT,
                          FILE_COMPRESSED_1_CONTENT_LEN);

    // Gz

    printf("Testing - gz\nwrite()\n");
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename,
                          CR_CW_GZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT,
                          FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename,
                          CR_CW_GZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT,
                          FILE_COMPRESSED_1_CONTENT_LEN);
    printf("puts()\n");
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename,
                          CR_CW_GZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT,
                          FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename,
                          CR_CW_GZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT,
                          FILE_COMPRESSED_1_CONTENT_LEN);
    printf("printf()\n");
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename,
                          CR_CW_GZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT,
                          FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename,
                          CR_CW_GZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT,
                          FILE_COMPRESSED_1_CONTENT_LEN);

    // Bz2

    printf("Testing - bz2\nwrite()\n");
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename,
                          CR_CW_BZ2_COMPRESSION, FILE_COMPRESSED_0_CONTENT,
                          FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename,
                          CR_CW_BZ2_COMPRESSION, FILE_COMPRESSED_1_CONTENT,
                          FILE_COMPRESSED_1_CONTENT_LEN);
    printf("puts()\n");
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename,
                          CR_CW_BZ2_COMPRESSION, FILE_COMPRESSED_0_CONTENT,
                          FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename,
                          CR_CW_BZ2_COMPRESSION, FILE_COMPRESSED_1_CONTENT,
                          FILE_COMPRESSED_1_CONTENT_LEN);
    printf("printf()\n");
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename,
                          CR_CW_BZ2_COMPRESSION, FILE_COMPRESSED_0_CONTENT,
                          FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename,
                          CR_CW_BZ2_COMPRESSION, FILE_COMPRESSED_1_CONTENT,
                          FILE_COMPRESSED_1_CONTENT_LEN);

    // Xz

    printf("Testing - xz\nwrite()\n");
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename,
                          CR_CW_XZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT,
                          FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename,
                          CR_CW_XZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT,
                          FILE_COMPRESSED_1_CONTENT_LEN);
    printf("puts()\n");
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename,
                          CR_CW_XZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT,
                          FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename,
                          CR_CW_XZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT,
                          FILE_COMPRESSED_1_CONTENT_LEN);
    printf("printf()\n");
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename,
                          CR_CW_XZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT,
                          FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename,
                          CR_CW_XZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT,
                          FILE_COMPRESSED_1_CONTENT_LEN);
}


static void
test_cr_error_handling(void)
{
    GError *tmp_err = NULL;
    cr_CompressionType type;
    CR_FILE *f;

    type = cr_detect_compression("/filename/that/should/not/exists", &tmp_err);
    g_assert_cmpint(type, ==, CR_CW_UNKNOWN_COMPRESSION);
    g_assert(tmp_err);
    g_assert_cmpint(tmp_err->code, ==, CRE_NOFILE);
    g_error_free(tmp_err);
    tmp_err = NULL;

    type = cr_detect_compression("/", &tmp_err);
    g_assert_cmpint(type, ==, CR_CW_UNKNOWN_COMPRESSION);
    g_assert(tmp_err);
    g_assert_cmpint(tmp_err->code, ==, CRE_NOFILE);
    g_error_free(tmp_err);
    tmp_err = NULL;

    f = cr_open("/", CR_CW_MODE_READ, CR_CW_AUTO_DETECT_COMPRESSION, &tmp_err);
    g_assert(!f);
    g_assert(tmp_err);
    g_assert_cmpint(tmp_err->code, ==, CRE_NOFILE);
    g_error_free(tmp_err);
    tmp_err = NULL;

    // Opening dir for writing

    f = cr_open("/", CR_CW_MODE_WRITE, CR_CW_NO_COMPRESSION, &tmp_err);
    g_assert(!f);
    g_assert(tmp_err);
    g_assert_cmpint(tmp_err->code, ==, CRE_IO);
    g_error_free(tmp_err);
    tmp_err = NULL;

    f = cr_open("/", CR_CW_MODE_WRITE, CR_CW_GZ_COMPRESSION, &tmp_err);
    g_assert(!f);
    g_assert(tmp_err);
    g_assert_cmpint(tmp_err->code, ==, CRE_GZ);
    g_error_free(tmp_err);
    tmp_err = NULL;

    f = cr_open("/", CR_CW_MODE_WRITE, CR_CW_BZ2_COMPRESSION, &tmp_err);
    g_assert(!f);
    g_assert(tmp_err);
    g_assert_cmpint(tmp_err->code, ==, CRE_IO);
    g_error_free(tmp_err);
    tmp_err = NULL;

    f = cr_open("/", CR_CW_MODE_WRITE, CR_CW_XZ_COMPRESSION, &tmp_err);
    g_assert(!f);
    g_assert(tmp_err);
    g_assert_cmpint(tmp_err->code, ==, CRE_XZ);
    g_error_free(tmp_err);
    tmp_err = NULL;

    // Opening plain text file as compressed

    char buf[256];
    int ret;

    // gzread can read compressed as well as uncompressed, so this test
    // is useful.
    f = cr_open(FILE_COMPRESSED_1_PLAIN, CR_CW_MODE_READ,
                CR_CW_GZ_COMPRESSION, &tmp_err);
    g_assert(f);
    ret = cr_read(f, buf, 256, &tmp_err);
    g_assert_cmpint(ret, ==, FILE_COMPRESSED_1_CONTENT_LEN);
    g_assert(!tmp_err);
    ret = cr_close(f, &tmp_err);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert(!tmp_err);

    f = cr_open(FILE_COMPRESSED_1_PLAIN, CR_CW_MODE_READ,
                CR_CW_BZ2_COMPRESSION, &tmp_err);
    g_assert(f);
    ret = cr_read(f, buf, 256, &tmp_err);
    g_assert_cmpint(ret, ==, -1);
    g_assert(tmp_err);
    g_assert_cmpint(tmp_err->code, ==, CRE_BZ2);
    g_error_free(tmp_err);
    tmp_err = NULL;
    ret = cr_close(f, &tmp_err);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert(!tmp_err);

    f = cr_open(FILE_COMPRESSED_1_PLAIN, CR_CW_MODE_READ,
                CR_CW_XZ_COMPRESSION, &tmp_err);
    g_assert(f);
    ret = cr_read(f, buf, 256, &tmp_err);
    g_assert_cmpint(ret, ==, -1);
    g_assert(tmp_err);
    g_assert_cmpint(tmp_err->code, ==, CRE_XZ);
    g_error_free(tmp_err);
    tmp_err = NULL;
    ret = cr_close(f, &tmp_err);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert(!tmp_err);
}


int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/compression_wrapper/test_cr_contentstat",
            test_cr_contentstat);
    g_test_add_func("/compression_wrapper/test_cr_compression_suffix",
            test_cr_compression_suffix);
    g_test_add_func("/compression_wrapper/test_cr_detect_compression",
            test_cr_detect_compression);
    g_test_add_func("/compression_wrapper/test_cr_detect_compression_bad_suffix",
            test_cr_detect_compression_bad_suffix);
    g_test_add_func("/compression_wrapper/test_cr_read_with_autodetection",
            test_cr_read_with_autodetection);
    g_test_add("/compression_wrapper/outputtest_cw_output", Outputtest, NULL,
            outputtest_setup, outputtest_cw_output, outputtest_teardown);
    g_test_add_func("/compression_wrapper/test_cr_error_handling",
            test_cr_error_handling);

    return g_test_run();
}
