#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <stdio.h>
#include "createrepo/misc.h"
#include "createrepo/compression_wrapper.h"

#define TMP_FILE_PATTERN                        "test_XXXXXX.txt"

#define COMPRESSED_BUFFER_LEN                   512

#define FILE_COMPRESSED_0_CONTENT               ""
#define FILE_COMPRESSED_0_CONTENT_LEN           0
#define FILE_COMPRESSED_0_PLAIN                 "test_data/compressed_files/00_plain.txt"
#define FILE_COMPRESSED_0_GZ                    "test_data/compressed_files/00_plain.txt.gz"
#define FILE_COMPRESSED_0_BZ2                   "test_data/compressed_files/00_plain.txt.bz2"
#define FILE_COMPRESSED_0_PLAIN_BAD_SUFFIX      "test_data/compressed_files/00_plain.foo0"
#define FILE_COMPRESSED_0_GZ_BAD_SUFFIX         "test_data/compressed_files/00_plain.foo1"
#define FILE_COMPRESSED_0_BZ2_BAD_SUFFIX        "test_data/compressed_files/00_plain.foo2"

#define FILE_COMPRESSED_1_CONTENT               "foobar foobar foobar foobar test test\nfolkjsaflkjsadokf\n"
#define FILE_COMPRESSED_1_CONTENT_LEN           56
#define FILE_COMPRESSED_1_PLAIN                 "test_data/compressed_files/01_plain.txt"
#define FILE_COMPRESSED_1_GZ                    "test_data/compressed_files/01_plain.txt.gz"
#define FILE_COMPRESSED_1_BZ2                   "test_data/compressed_files/01_plain.txt.bz2"
#define FILE_COMPRESSED_1_PLAIN_BAD_SUFFIX      "test_data/compressed_files/01_plain.foo0"
#define FILE_COMPRESSED_1_GZ_BAD_SUFFIX         "test_data/compressed_files/01_plain.foo1"
#define FILE_COMPRESSED_1_BZ2_BAD_SUFFIX        "test_data/compressed_files/01_plain.foo2"



static void test_get_suffix(void)
{
    const char *suffix;

    suffix = get_suffix(AUTO_DETECT_COMPRESSION);
    g_assert(!suffix);

    suffix = get_suffix(UNKNOWN_COMPRESSION);
    g_assert(!suffix);

    suffix = get_suffix(NO_COMPRESSION);
    g_assert(!suffix);

    suffix = get_suffix(GZ_COMPRESSION);
    g_assert_cmpstr(suffix, ==, ".gz");

    suffix = get_suffix(BZ2_COMPRESSION);
    g_assert_cmpstr(suffix, ==, ".bz2");
}


static void test_detect_compression(void)
{
    CompressionType ret;

    // Plain

    ret = detect_compression(FILE_COMPRESSED_0_PLAIN);
    g_assert_cmpint(ret, ==, NO_COMPRESSION);
    ret = detect_compression(FILE_COMPRESSED_1_PLAIN);
    g_assert_cmpint(ret, ==, NO_COMPRESSION);

    // Gz

    ret = detect_compression(FILE_COMPRESSED_0_GZ);
    g_assert_cmpint(ret, ==, GZ_COMPRESSION);
    ret = detect_compression(FILE_COMPRESSED_1_GZ);
    g_assert_cmpint(ret, ==, GZ_COMPRESSION);

    // Bz2

    ret = detect_compression(FILE_COMPRESSED_0_BZ2);
    g_assert_cmpint(ret, ==, BZ2_COMPRESSION);
    ret = detect_compression(FILE_COMPRESSED_1_BZ2);
    g_assert_cmpint(ret, ==, BZ2_COMPRESSION);
}


static void test_detect_compression_bad_suffix(void)
{
    CompressionType ret;

    // Plain

    ret = detect_compression(FILE_COMPRESSED_0_PLAIN_BAD_SUFFIX);
    printf("%s: (%d)\n", FILE_COMPRESSED_0_PLAIN_BAD_SUFFIX, ret);
    g_assert_cmpint(ret, ==, NO_COMPRESSION);
    ret = detect_compression(FILE_COMPRESSED_1_PLAIN_BAD_SUFFIX);
    g_assert_cmpint(ret, ==, NO_COMPRESSION);

    // Gz

    ret = detect_compression(FILE_COMPRESSED_0_GZ_BAD_SUFFIX);
    g_assert_cmpint(ret, ==, GZ_COMPRESSION);
    ret = detect_compression(FILE_COMPRESSED_1_GZ_BAD_SUFFIX);
    g_assert_cmpint(ret, ==, GZ_COMPRESSION);

    // Bz2

    ret = detect_compression(FILE_COMPRESSED_0_BZ2_BAD_SUFFIX);
    g_assert_cmpint(ret, ==, BZ2_COMPRESSION);
    ret = detect_compression(FILE_COMPRESSED_1_BZ2_BAD_SUFFIX);
    g_assert_cmpint(ret, ==, BZ2_COMPRESSION);
}


void test_helper_cw_input(const char *filename, CompressionType ctype, const char *content, int len)
{
    int ret;
    CW_FILE *file;
    char buffer[COMPRESSED_BUFFER_LEN+1];

    file = cw_open(filename, CW_MODE_READ, ctype);
    g_assert(file != NULL);
    ret = cw_read(file, buffer, COMPRESSED_BUFFER_LEN);
    g_assert_cmpint(ret, ==, len);
    buffer[ret] = '\0';
    g_assert_cmpstr(buffer, ==, content);
    ret = cw_close(file);
    g_assert_cmpint(ret, ==, CW_OK);
}


static void test_cw_read_with_autodetection(void)
{
    // Plain

    test_helper_cw_input(FILE_COMPRESSED_0_PLAIN, AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_input(FILE_COMPRESSED_1_PLAIN, AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);

    // Gz

    test_helper_cw_input(FILE_COMPRESSED_0_GZ, AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_input(FILE_COMPRESSED_1_GZ, AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);

    // Bzip2

    test_helper_cw_input(FILE_COMPRESSED_0_BZ2, AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_input(FILE_COMPRESSED_1_BZ2, AUTO_DETECT_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
}


typedef struct {
    gchar *tmp_filename;
} Outputtest;


static void outputtest_setup(Outputtest *outputtest, gconstpointer test_data)
{
    UNUSED(test_data);
    int fd;

    fd = g_file_open_tmp(TMP_FILE_PATTERN, &(outputtest->tmp_filename), NULL);
    close(fd);
}


static void outputtest_teardown(Outputtest *outputtest, gconstpointer test_data)
{
    UNUSED(test_data);

    if (outputtest->tmp_filename) {
        remove(outputtest->tmp_filename);
        g_free(outputtest->tmp_filename);
    }
}


#define OUTPUT_TYPE_WRITE       0
#define OUTPUT_TYPE_PUTS        1
#define OUTPUT_TYPE_PRINTF      2

void test_helper_cw_output(int type, const char *filename, CompressionType ctype, const char *content, int len) {
    int ret;
    CW_FILE *file;

    file = cw_open(filename, CW_MODE_WRITE, ctype);
    g_assert(file);

    switch(type) {
        case OUTPUT_TYPE_WRITE:
            ret = cw_write(file, content, len);
            g_assert_cmpint(ret, ==, len);
            break;

        case OUTPUT_TYPE_PUTS:
            ret = cw_puts(file, content);
            g_assert_cmpint(ret, ==, CW_OK);
            break;

        case OUTPUT_TYPE_PRINTF:
            ret = cw_printf(file, "%s", content);
            g_assert_cmpint(ret, ==, len);
            break;

        default:
            break;
    }

    ret = cw_close(file);
    g_assert_cmpint(ret, ==, CW_OK);

    // Read and compare

    test_helper_cw_input(filename, ctype, content, len);
}


static void outputtest_cw_output(Outputtest *outputtest, gconstpointer test_data)
{
    UNUSED(test_data);

    CW_FILE *file;

    file = cw_open(outputtest->tmp_filename, CW_MODE_WRITE, AUTO_DETECT_COMPRESSION);
    g_assert(!file);

    file = cw_open(outputtest->tmp_filename, CW_MODE_WRITE, UNKNOWN_COMPRESSION);
    g_assert(!file);

    // Plain

    test_helper_cw_output(OUTPUT_TYPE_WRITE, outputtest->tmp_filename, NO_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_WRITE, outputtest->tmp_filename, NO_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PUTS, outputtest->tmp_filename, NO_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PUTS, outputtest->tmp_filename, NO_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);

    // Gz

    test_helper_cw_output(OUTPUT_TYPE_WRITE, outputtest->tmp_filename, GZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_WRITE, outputtest->tmp_filename, GZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PUTS, outputtest->tmp_filename, GZ_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PUTS, outputtest->tmp_filename, GZ_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);

    // Bz2

    test_helper_cw_output(OUTPUT_TYPE_WRITE, outputtest->tmp_filename, BZ2_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_WRITE, outputtest->tmp_filename, BZ2_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename, BZ2_COMPRESSION, FILE_COMPRESSED_0_CONTENT, FILE_COMPRESSED_0_CONTENT_LEN);
    test_helper_cw_output(OUTPUT_TYPE_PRINTF, outputtest->tmp_filename, BZ2_COMPRESSION, FILE_COMPRESSED_1_CONTENT, FILE_COMPRESSED_1_CONTENT_LEN);
}



int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/compression_wrapper/test_get_suffix", test_get_suffix);
    g_test_add_func("/compression_wrapper/test_detect_compression", test_detect_compression);
    g_test_add_func("/compression_wrapper/test_detect_compression_bad_suffix", test_detect_compression_bad_suffix);
    g_test_add_func("/compression_wrapper/test_cw_read_with_autodetection", test_cw_read_with_autodetection);
    g_test_add("/compression_wrapper/outputtest_cw_output", Outputtest, NULL, outputtest_setup, outputtest_cw_output, outputtest_teardown);

    return g_test_run();
}
