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
#include "createrepo/compression_wrapper.h"

#define TMP_FILE_PATTERN                        "test_XXXXXX.txt"

#define COMPRESSED_BUFFER_LEN                   512

#define FILE_COMPRESSED_0_CONTENT               ""
#define FILE_COMPRESSED_0_CONTENT_LEN           0
#define FILE_COMPRESSED_0_PLAIN                 "test_data/compressed_files/00_plain.txt"
#define FILE_COMPRESSED_0_GZ                    "test_data/compressed_files/00_plain.txt.gz"
#define FILE_COMPRESSED_0_BZ2                   "test_data/compressed_files/00_plain.txt.bz2"
#define FILE_COMPRESSED_0_XZ                    "test_data/compressed_files/00_plain.txt.xz"
#define FILE_COMPRESSED_0_PLAIN_BAD_SUFFIX      "test_data/compressed_files/00_plain.foo0"
#define FILE_COMPRESSED_0_GZ_BAD_SUFFIX         "test_data/compressed_files/00_plain.foo1"
#define FILE_COMPRESSED_0_BZ2_BAD_SUFFIX        "test_data/compressed_files/00_plain.foo2"
#define FILE_COMPRESSED_0_XZ_BAD_SUFFIX         "test_data/compressed_files/00_plain.foo3"

#define FILE_COMPRESSED_1_CONTENT               "foobar foobar foobar foobar test test\nfolkjsaflkjsadokf\n"
#define FILE_COMPRESSED_1_CONTENT_LEN           56
#define FILE_COMPRESSED_1_PLAIN                 "test_data/compressed_files/01_plain.txt"
#define FILE_COMPRESSED_1_GZ                    "test_data/compressed_files/01_plain.txt.gz"
#define FILE_COMPRESSED_1_BZ2                   "test_data/compressed_files/01_plain.txt.bz2"
#define FILE_COMPRESSED_1_XZ                    "test_data/compressed_files/01_plain.txt.xz"
#define FILE_COMPRESSED_1_PLAIN_BAD_SUFFIX      "test_data/compressed_files/01_plain.foo0"
#define FILE_COMPRESSED_1_GZ_BAD_SUFFIX         "test_data/compressed_files/01_plain.foo1"
#define FILE_COMPRESSED_1_BZ2_BAD_SUFFIX        "test_data/compressed_files/01_plain.foo2"
#define FILE_COMPRESSED_1_XZ_BAD_SUFFIX         "test_data/compressed_files/01_plain.foo3"



static void test_cr_compression_suffix(void)
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


static void test_cr_detect_compression(void)
{
    cr_CompressionType ret;

    // Plain

    ret = cr_detect_compression(FILE_COMPRESSED_0_PLAIN);
    g_assert_cmpint(ret, ==, CR_CW_NO_COMPRESSION);
    ret = cr_detect_compression(FILE_COMPRESSED_1_PLAIN);
    g_assert_cmpint(ret, ==, CR_CW_NO_COMPRESSION);

    // Gz

    ret = cr_detect_compression(FILE_COMPRESSED_0_GZ);
    g_assert_cmpint(ret, ==, CR_CW_GZ_COMPRESSION);
    ret = cr_detect_compression(FILE_COMPRESSED_1_GZ);
    g_assert_cmpint(ret, ==, CR_CW_GZ_COMPRESSION);

    // Bz2

    ret = cr_detect_compression(FILE_COMPRESSED_0_BZ2);
    g_assert_cmpint(ret, ==, CR_CW_BZ2_COMPRESSION);
    ret = cr_detect_compression(FILE_COMPRESSED_1_BZ2);
    g_assert_cmpint(ret, ==, CR_CW_BZ2_COMPRESSION);

    // Xz

    ret = cr_detect_compression(FILE_COMPRESSED_0_XZ);
    g_assert_cmpint(ret, ==, CR_CW_XZ_COMPRESSION);
    ret = cr_detect_compression(FILE_COMPRESSED_1_XZ);
    g_assert_cmpint(ret, ==, CR_CW_XZ_COMPRESSION);
}


static void test_cr_detect_compression_bad_suffix(void)
{
    cr_CompressionType ret;

    // Plain

    ret = cr_detect_compression(FILE_COMPRESSED_0_PLAIN_BAD_SUFFIX);
    g_assert_cmpint(ret, ==, CR_CW_NO_COMPRESSION);
    ret = cr_detect_compression(FILE_COMPRESSED_1_PLAIN_BAD_SUFFIX);
    g_assert_cmpint(ret, ==, CR_CW_NO_COMPRESSION);

    // Gz

    ret = cr_detect_compression(FILE_COMPRESSED_0_GZ_BAD_SUFFIX);
    g_assert_cmpint(ret, ==, CR_CW_GZ_COMPRESSION);
    ret = cr_detect_compression(FILE_COMPRESSED_1_GZ_BAD_SUFFIX);
    g_assert_cmpint(ret, ==, CR_CW_GZ_COMPRESSION);

    // Bz2

    ret = cr_detect_compression(FILE_COMPRESSED_0_BZ2_BAD_SUFFIX);
    g_assert_cmpint(ret, ==, CR_CW_BZ2_COMPRESSION);
    ret = cr_detect_compression(FILE_COMPRESSED_1_BZ2_BAD_SUFFIX);
    g_assert_cmpint(ret, ==, CR_CW_BZ2_COMPRESSION);

    // Xz

    ret = cr_detect_compression(FILE_COMPRESSED_0_XZ_BAD_SUFFIX);
    g_assert_cmpint(ret, ==, CR_CW_XZ_COMPRESSION);
    ret = cr_detect_compression(FILE_COMPRESSED_1_XZ_BAD_SUFFIX);
    g_assert_cmpint(ret, ==, CR_CW_XZ_COMPRESSION);
}


void test_helper_cw_input(const char *filename, cr_CompressionType ctype, const char *content, int len)
{
    int ret;
    CR_FILE *file;
    char buffer[COMPRESSED_BUFFER_LEN+1];

    file = cr_open(filename, CR_CW_MODE_READ, ctype);
    g_assert(file != NULL);
    ret = cr_read(file, buffer, COMPRESSED_BUFFER_LEN);
    g_assert_cmpint(ret, ==, len);
    buffer[ret] = '\0';
    g_assert_cmpstr(buffer, ==, content);
    ret = cr_close(file);
    g_assert_cmpint(ret, ==, CR_CW_OK);
}


static void test_cr_read_with_autodetection(void)
{
    // Plain

    test_helper_cw_input(FILE_COMPRESSED_0_PLAIN, CR_CW_AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_input(FILE_COMPRESSED_1_PLAIN, CR_CW_AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);

    // Gz

    test_helper_cw_input(FILE_COMPRESSED_0_GZ, CR_CW_AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_input(FILE_COMPRESSED_1_GZ, CR_CW_AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);

    // Bzip2

    test_helper_cw_input(FILE_COMPRESSED_0_BZ2, CR_CW_AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_input(FILE_COMPRESSED_1_BZ2, CR_CW_AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);

    // Xz

    test_helper_cw_input(FILE_COMPRESSED_0_XZ, CR_CW_AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_input(FILE_COMPRESSED_1_XZ, CR_CW_AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
}


typedef struct {
    gchar *tmp_filename;
} Outputtest;


static void outputtest_setup(Outputtest *outputtest, gconstpointer test_data)
{
    CR_UNUSED(test_data);
    int fd;

    fd = g_file_open_tmp(TMP_FILE_PATTERN, &(outputtest->tmp_filename), NULL);
    close(fd);
}


static void outputtest_teardown(Outputtest *outputtest, gconstpointer test_data)
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

void test_helper_cw_output(int type, const char *filename, cr_CompressionType ctype, const char *content, int len) {
    int ret;
    CR_FILE *file;

    file = cr_open(filename, CR_CW_MODE_WRITE, ctype);
    g_assert(file);

    switch(type) {
        case OUTPUT_TYPE_WRITE:
            ret = cr_write(file, content, len);
            g_assert_cmpint(ret, ==, len);
            break;

        case OUTPUT_TYPE_PUTS:
            ret = cr_puts(file, content);
            g_assert_cmpint(ret, ==, CR_CW_OK);
            break;

        case OUTPUT_TYPE_PRINTF:
            ret = cr_printf(file, "%s", content);   // XXX
            g_assert_cmpint(ret, ==, len);
            break;

        default:
            break;
    }

    ret = cr_close(file);
    g_assert_cmpint(ret, ==, CR_CW_OK);

    // Read and compare

    test_helper_cw_input(filename, ctype, content, len);
}


static void outputtest_cw_output(Outputtest *outputtest, gconstpointer test_data)
{
    CR_UNUSED(test_data);

    CR_FILE *file;

    file = cr_open(outputtest->tmp_filename, CR_CW_MODE_WRITE, CR_CW_AUTO_DETECT_COMPRESSION);
    g_assert(!file);

    file = cr_open(outputtest->tmp_filename, CR_CW_MODE_WRITE, CR_CW_UNKNOWN_COMPRESSION);
    g_assert(!file);

    // Plain

    printf("Testing - plain\nwrite()\n");
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename, CR_CW_NO_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename, CR_CW_NO_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
    printf("puts()\n");
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename, CR_CW_NO_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename, CR_CW_NO_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
    printf("printf()\n");
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename, CR_CW_NO_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename, CR_CW_NO_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);

    // Gz

    printf("Testing - gz\nwrite()\n");
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename, CR_CW_GZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename, CR_CW_GZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
    printf("puts()\n");
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename, CR_CW_GZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename, CR_CW_GZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
    printf("printf()\n");
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename, CR_CW_GZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename, CR_CW_GZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);

    // Bz2

    printf("Testing - bz2\nwrite()\n");
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename, CR_CW_BZ2_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename, CR_CW_BZ2_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
    printf("puts()\n");
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename, CR_CW_BZ2_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename, CR_CW_BZ2_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
    printf("printf()\n");
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename, CR_CW_BZ2_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename, CR_CW_BZ2_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);

    // Xz

    printf("Testing - xz\nwrite()\n");
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename, CR_CW_XZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_WRITE,  outputtest->tmp_filename, CR_CW_XZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
    printf("puts()\n");
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename, CR_CW_XZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PUTS,   outputtest->tmp_filename, CR_CW_XZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
    printf("printf()\n");
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename, CR_CW_XZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename, CR_CW_XZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
}



int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/compression_wrapper/test_cr_compression_suffix", test_cr_compression_suffix);
    g_test_add_func("/compression_wrapper/test_cr_detect_compression", test_cr_detect_compression);
    g_test_add_func("/compression_wrapper/test_cr_detect_compression_bad_suffix", test_cr_detect_compression_bad_suffix);
    g_test_add_func("/compression_wrapper/test_cr_read_with_autodetection", test_cr_read_with_autodetection);
    g_test_add("/compression_wrapper/outputtest_cw_output", Outputtest, NULL, outputtest_setup, outputtest_cw_output, outputtest_teardown);

    return g_test_run();
}
