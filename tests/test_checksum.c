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

static void
test_cr_checksum_file(void)
{
    char *checksum;
    GError *tmp_err = NULL;

    checksum = cr_checksum_file(TEST_EMPTY_FILE, CR_CHECKSUM_MD5, NULL);
    g_assert_cmpstr(checksum, ==, "d41d8cd98f00b204e9800998ecf8427e");
    g_free(checksum);
    checksum = cr_checksum_file(TEST_EMPTY_FILE, CR_CHECKSUM_SHA1, NULL);
    g_assert_cmpstr(checksum, ==, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    g_free(checksum);
    checksum = cr_checksum_file(TEST_EMPTY_FILE, CR_CHECKSUM_SHA256, NULL);
    g_assert_cmpstr(checksum, ==, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649"
            "b934ca495991b7852b855");
    g_free(checksum);
    checksum = cr_checksum_file(TEST_EMPTY_FILE, CR_CHECKSUM_SHA512, NULL);
    g_assert_cmpstr(checksum, ==, "cf83e1357eefb8bdf1542850d66d8007d620e4050b5"
            "715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd4741"
            "7a81a538327af927da3e");
    g_free(checksum);

    checksum = cr_checksum_file(TEST_TEXT_FILE, CR_CHECKSUM_MD5, &tmp_err);
    g_assert_cmpstr(checksum, ==, "d6d4da5c15f8fe7570ce6ab6b3503916");
    g_assert(!tmp_err);
    g_free(checksum);
    checksum = cr_checksum_file(TEST_TEXT_FILE, CR_CHECKSUM_SHA1, &tmp_err);
    g_assert_cmpstr(checksum, ==, "da048ee8fabfbef1b3d6d3f5a4be20029eecec77");
    g_assert(!tmp_err);
    g_free(checksum);
    checksum = cr_checksum_file(TEST_TEXT_FILE, CR_CHECKSUM_SHA256, &tmp_err);
    g_assert_cmpstr(checksum, ==, "2f395bdfa2750978965e4781ddf224c89646c7d7a15"
            "69b7ebb023b170f7bd8bb");
    g_assert(!tmp_err);
    g_free(checksum);
    checksum = cr_checksum_file(TEST_TEXT_FILE, CR_CHECKSUM_SHA512, &tmp_err);
    g_assert_cmpstr(checksum, ==, "6ef7c2fd003614033aab59a65164c897fd150cfa855"
            "1f2dd66828cc7a4d16afc3a35890f342eeaa424c1270fa8bbb4b792875b9deb34"
            "cd78ab9ded1c360de45c");
    g_assert(!tmp_err);
    g_free(checksum);

    checksum = cr_checksum_file(TEST_BINARY_FILE, CR_CHECKSUM_MD5, NULL);
    g_assert_cmpstr(checksum, ==, "4f8b033d7a402927a20c9328fc0e0f46");
    g_free(checksum);
    checksum = cr_checksum_file(TEST_BINARY_FILE, CR_CHECKSUM_SHA1, NULL);
    g_assert_cmpstr(checksum, ==, "3539fb660a41846352ac4fa9076d168a3c77070b");
    g_free(checksum);
    checksum = cr_checksum_file(TEST_BINARY_FILE, CR_CHECKSUM_SHA256, NULL);
    g_assert_cmpstr(checksum, ==, "bf68e32ad78cea8287be0f35b74fa3fecd0eaa91770"
            "b48f1a7282b015d6d883e");
    g_free(checksum);
    checksum = cr_checksum_file(TEST_BINARY_FILE, CR_CHECKSUM_SHA512, NULL);
    g_assert_cmpstr(checksum, ==, "339877a8ce6cdb2df62f3f76c005cac4f50144197bd"
            "095cec21056d6ddde570fe5b16e3f1cd077ece799d5dd23dc6c9c1afed018384d"
            "840bd97233c320e60dfa");
    g_free(checksum);

    // Corner cases

    checksum = cr_checksum_file(TEST_BINARY_FILE, 244, &tmp_err);
    g_assert(!checksum);
    g_assert(tmp_err);
    g_error_free(tmp_err);
    tmp_err = NULL;

    checksum = cr_checksum_file(NON_EXIST_FILE, CR_CHECKSUM_MD5, &tmp_err);
    g_assert(!checksum);
    g_assert(tmp_err);
    g_error_free(tmp_err);
    tmp_err = NULL;
}


static void
test_cr_checksum_name_str(void)
{
    const char *checksum_name;

    checksum_name = cr_checksum_name_str(CR_CHECKSUM_MD5);
    g_assert_cmpstr(checksum_name, ==, "md5");

    checksum_name = cr_checksum_name_str(CR_CHECKSUM_SHA);
    g_assert_cmpstr(checksum_name, ==, "sha");

    checksum_name = cr_checksum_name_str(CR_CHECKSUM_SHA1);
    g_assert_cmpstr(checksum_name, ==, "sha1");

    checksum_name = cr_checksum_name_str(CR_CHECKSUM_SHA224);
    g_assert_cmpstr(checksum_name, ==, "sha224");

    checksum_name = cr_checksum_name_str(CR_CHECKSUM_SHA256);
    g_assert_cmpstr(checksum_name, ==, "sha256");

    checksum_name = cr_checksum_name_str(CR_CHECKSUM_SHA384);
    g_assert_cmpstr(checksum_name, ==, "sha384");

    checksum_name = cr_checksum_name_str(CR_CHECKSUM_SHA512);
    g_assert_cmpstr(checksum_name, ==, "sha512");

    checksum_name = cr_checksum_name_str(244);
    g_assert_cmpstr(checksum_name, ==, NULL);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/checksum/test_cr_checksum_file",
            test_cr_checksum_file);
    g_test_add_func("/checksum/test_cr_checksum_name_str",
            test_cr_checksum_name_str);

    return g_test_run();
}
