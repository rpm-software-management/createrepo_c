/*
 * Copyright (C) 2021 Red Hat, Inc.
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include "fixtures.h"
#include "createrepo/error.h"
#include "createrepo/package.h"
#include "createrepo/misc.h"
#include "createrepo/xml_parser.h"
#include "createrepo/xml_parser_internal.h"

// Callbacks

static int
pkgcb(cr_Package *pkg, void *cbdata, GError **err)
{
    g_assert(pkg);
    g_assert(!err || *err == NULL);
    if (cbdata) *((int *)cbdata) += 1;
    cr_package_free(pkg);
    return CR_CB_RET_OK;
}

static int
pkgcb_interrupt(cr_Package *pkg, void *cbdata, GError **err)
{
    g_assert(pkg);
    g_assert(!err || *err == NULL);
    if (cbdata) *((int *)cbdata) += 1;
    cr_package_free(pkg);
    return CR_CB_RET_ERR;
}

static int
newpkgcb_skip_fake_bash(cr_Package **pkg,
                        G_GNUC_UNUSED const char *pkgId,
                        const char *name,
                        G_GNUC_UNUSED const char *arch,
                        G_GNUC_UNUSED void *cbdata,
                        GError **err)
{
    g_assert(pkg != NULL);
    g_assert(*pkg == NULL);
    g_assert(pkgId != NULL);
    g_assert(!err || *err == NULL);


    if (!g_strcmp0(name, "fake_bash")) {
        return CRE_OK;
    }

    *pkg = cr_package_new();
    return CR_CB_RET_OK;
}

static int
newpkgcb_interrupt(cr_Package **pkg,
                   G_GNUC_UNUSED const char *pkgId,
                   G_GNUC_UNUSED const char *name,
                   G_GNUC_UNUSED const char *arch,
                   G_GNUC_UNUSED void *cbdata,
                   GError **err)
{
    g_assert(pkg != NULL);
    g_assert(*pkg == NULL);
    g_assert(pkgId != NULL);
    g_assert(!err || *err == NULL);

    if (cbdata) *((int *)cbdata) += 1;

    return CR_CB_RET_ERR;
}

static int
warningcb(G_GNUC_UNUSED cr_XmlParserWarningType type,
          G_GNUC_UNUSED char *msg,
          void *cbdata,
          G_GNUC_UNUSED GError **err)
{
    g_assert(type < CR_XML_WARNING_SENTINEL);
    g_assert(!err || *err == NULL);

    g_string_append((GString *) cbdata, msg);
    g_string_append((GString *) cbdata, ";");

    return CR_CB_RET_OK;
}

static int
warningcb_interrupt(G_GNUC_UNUSED cr_XmlParserWarningType type,
                    G_GNUC_UNUSED char *msg,
                    G_GNUC_UNUSED void *cbdata,
                    G_GNUC_UNUSED GError **err)
{
    g_assert(type < CR_XML_WARNING_SENTINEL);
    g_assert(!err || *err == NULL);

    if (cbdata) *((int *)cbdata) += 1;

    return CR_CB_RET_ERR;
}

// Tests

static void
test_cr_xml_parse_main_metadata_together_00(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_main_metadata_together(TEST_REPO_02_PRIMARY, TEST_REPO_02_FILELISTS, TEST_REPO_02_OTHER,
                                                  NULL, NULL, pkgcb, &parsed, NULL, NULL, TRUE, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 2);
}

static void
test_cr_xml_parse_main_metadata_together_01_out_of_order_pkgs(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_main_metadata_together(TEST_REPO_02_PRIMARY,
                                                  TEST_DIFF_ORDER_FILELISTS,
                                                  TEST_REPO_02_OTHER,
                                                  NULL, NULL, pkgcb, &parsed, NULL, NULL, FALSE, &tmp_err);
    g_assert(tmp_err != NULL);
    g_assert_cmpint(ret, ==, CRE_XMLPARSER);

    g_clear_error(&tmp_err);
    ret = cr_xml_parse_main_metadata_together(TEST_REPO_02_PRIMARY, TEST_DIFF_ORDER_FILELISTS, TEST_REPO_02_OTHER,
                                              NULL, NULL, pkgcb, &parsed, NULL, NULL, TRUE, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 2);
}

static void
test_cr_xml_parse_main_metadata_together_02_invalid_path(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_main_metadata_together("/non/existent/file", TEST_REPO_02_FILELISTS, TEST_REPO_02_OTHER,
                                                  NULL, NULL, pkgcb, &parsed, NULL, NULL, TRUE, &tmp_err);
    g_assert(tmp_err != NULL);
    g_assert_cmpint(ret, ==, CRE_NOFILE);
}

static void
test_cr_xml_parse_main_metadata_together_03_newpkgcb_returns_null(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_main_metadata_together(TEST_REPO_02_PRIMARY, TEST_REPO_02_FILELISTS, TEST_REPO_02_OTHER,
                                                  newpkgcb_skip_fake_bash, NULL, pkgcb, &parsed, NULL, NULL, TRUE,
                                                  &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 1);

    parsed = 0;
    ret = cr_xml_parse_main_metadata_together(TEST_REPO_02_PRIMARY, TEST_DIFF_ORDER_FILELISTS, TEST_REPO_02_OTHER,
                                              newpkgcb_skip_fake_bash, NULL, pkgcb, &parsed, NULL, NULL, TRUE,
                                              &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 1);

    parsed = 0;
    ret = cr_xml_parse_main_metadata_together(TEST_REPO_02_PRIMARY, TEST_DIFF_ORDER_FILELISTS, TEST_REPO_02_OTHER,
                                              newpkgcb_skip_fake_bash, NULL, pkgcb, &parsed, NULL, NULL, FALSE,
                                              &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 1);
}

static void
test_cr_xml_parse_main_metadata_together_04_newpkgcb_interrupt(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_main_metadata_together(TEST_REPO_02_PRIMARY, TEST_REPO_02_FILELISTS, TEST_REPO_02_OTHER,
                                                  newpkgcb_interrupt, &parsed, NULL, NULL, NULL, NULL, TRUE, &tmp_err);
    g_assert(tmp_err != NULL);
    g_error_free(tmp_err);
    g_assert_cmpint(ret, ==, CRE_CBINTERRUPTED);
    g_assert_cmpint(parsed, ==, 1);
}

static void
test_cr_xml_parse_main_metadata_together_05_pkgcb_interrupt(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_main_metadata_together(TEST_REPO_02_PRIMARY, TEST_REPO_02_FILELISTS, TEST_REPO_02_OTHER,
                                                  NULL, NULL, pkgcb_interrupt, &parsed, NULL, NULL, TRUE, &tmp_err);
    g_assert(tmp_err != NULL);
    g_error_free(tmp_err);
    g_assert_cmpint(ret, ==, CRE_CBINTERRUPTED);
    g_assert_cmpint(parsed, ==, 1);
}

static void
test_cr_xml_parse_main_metadata_together_06_warnings_bad_file_type(void)
{
    int parsed = 0;
    char *warnmsgs;
    GError *tmp_err = NULL;
    GString *warn_strings = g_string_new(0);
    int ret = cr_xml_parse_main_metadata_together(TEST_REPO_02_PRIMARY, TEST_MRF_BAD_TYPE_FIL, TEST_REPO_02_OTHER,
                                                  NULL, NULL, pkgcb, &parsed, warningcb, warn_strings, TRUE, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 2);
    warnmsgs = g_string_free(warn_strings, FALSE);
    g_assert_cmpstr(warnmsgs, ==, "Unknown file type \"foo\";");
    g_free(warnmsgs);
}

static void
test_cr_xml_parse_main_metadata_together_07_warningcb_interrupt(void)
{
    int numofwarnings = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_main_metadata_together(TEST_REPO_02_PRIMARY, TEST_MRF_BAD_TYPE_FIL, TEST_REPO_02_OTHER,
                                                  NULL, NULL, pkgcb, NULL, warningcb_interrupt, &numofwarnings,
                                                  TRUE, &tmp_err);
    g_assert(tmp_err != NULL);
    g_error_free(tmp_err);
    g_assert_cmpint(ret, ==, CRE_CBINTERRUPTED);
    g_assert_cmpint(numofwarnings, ==, 1);
}

static void
test_cr_xml_parse_main_metadata_together_08_long_primary(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_main_metadata_together(TEST_LONG_PRIMARY, TEST_REPO_02_FILELISTS, TEST_REPO_02_OTHER,
                                                  NULL, NULL, pkgcb, &parsed, NULL, NULL, TRUE, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 2);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_main_metadata_together_00",
                    test_cr_xml_parse_main_metadata_together_00);

    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_main_metadata_together_01_out_of_order_pkgs",
                    test_cr_xml_parse_main_metadata_together_01_out_of_order_pkgs);

    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_main_metadata_together_02_invalid_path",
                    test_cr_xml_parse_main_metadata_together_02_invalid_path);

    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_main_metadata_together_03_newpkgcb_returns_null",
                    test_cr_xml_parse_main_metadata_together_03_newpkgcb_returns_null);

    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_main_metadata_together_04_newpkgcb_interrupt",
                    test_cr_xml_parse_main_metadata_together_04_newpkgcb_interrupt);

    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_main_metadata_together_05_pkgcb_interrupt",
                    test_cr_xml_parse_main_metadata_together_05_pkgcb_interrupt);

    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_main_metadata_together_06_warnings_bad_file_type",
                    test_cr_xml_parse_main_metadata_together_06_warnings_bad_file_type);

    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_main_metadata_together_07_warningcb_interrupt",
                    test_cr_xml_parse_main_metadata_together_07_warningcb_interrupt);

    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_main_metadata_together_08_long_primary",
                    test_cr_xml_parse_main_metadata_together_08_long_primary);

    return g_test_run();
}
