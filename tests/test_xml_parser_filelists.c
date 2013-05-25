/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2013  Tomas Mlcoch
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
#include <stdlib.h>
#include <stdio.h>
#include "fixtures.h"
#include "createrepo/error.h"
#include "createrepo/package.h"
#include "createrepo/misc.h"
#include "createrepo/xml_parser.h"

static int
pkgcb(cr_Package *pkg, void *cbdata, GError **err)
{
    g_assert(!err || *err == NULL);
    if (cbdata) *((int *)cbdata) += 1;
    return CRE_OK;
}

static void test_cr_xml_parse_filelists_00(void)
{
    int ret;
    GError *tmp_err = NULL;

    ret = cr_xml_parse_filelists(TEST_REPO_00_FILELISTS,
                                 NULL,
                                 NULL,
                                 pkgcb,
                                 NULL,
                                 &tmp_err);

    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
}

static void test_cr_xml_parse_filelists_01(void)
{
    int ret;
    int parsed = 0;
    GError *tmp_err = NULL;

    ret = cr_xml_parse_filelists(TEST_REPO_01_FILELISTS,
                                 NULL,
                                 NULL,
                                 pkgcb,
                                 &parsed,
                                 &tmp_err);

    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 1);
}

static void test_cr_xml_parse_filelists_02(void)
{
    int ret;
    int parsed = 0;
    GError *tmp_err = NULL;

    ret = cr_xml_parse_filelists(TEST_REPO_02_FILELISTS,
                                 NULL,
                                 NULL,
                                 pkgcb,
                                 &parsed,
                                 &tmp_err);

    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 2);
}

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_00",
                    test_cr_xml_parse_filelists_00);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_01",
                    test_cr_xml_parse_filelists_01);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_02",
                    test_cr_xml_parse_filelists_02);
    return g_test_run();
}
