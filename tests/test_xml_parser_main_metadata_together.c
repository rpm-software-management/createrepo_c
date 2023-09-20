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

static void
test_cr_xml_package_iterator_00(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    cr_Package *package = NULL;

    cr_PkgIterator *pkg_iterator = cr_PkgIterator_new(
        TEST_REPO_02_PRIMARY, TEST_REPO_02_FILELISTS, TEST_REPO_02_OTHER, NULL, NULL, NULL, NULL, &tmp_err);

    while ((package = cr_PkgIterator_parse_next(pkg_iterator, &tmp_err))) {
        parsed++;
        cr_package_free(package);
    }

    g_assert(cr_PkgIterator_is_finished(pkg_iterator));
    cr_PkgIterator_free(pkg_iterator, &tmp_err);

    g_assert(tmp_err == NULL);
    g_assert_cmpint(parsed, ==, 2);
}

static void
test_cr_xml_package_iterator_filelists_ext_00(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    cr_Package *package = NULL;

    cr_PkgIterator *pkg_iterator = cr_PkgIterator_new(
        TEST_REPO_04_PRIMARY, TEST_REPO_04_FILELISTS_EXT, TEST_REPO_04_OTHER, NULL, NULL, NULL, NULL, &tmp_err);

    while ((package = cr_PkgIterator_parse_next(pkg_iterator, &tmp_err))) {
        parsed++;
        cr_package_free(package);
    }

    g_assert(cr_PkgIterator_is_finished(pkg_iterator));
    cr_PkgIterator_free(pkg_iterator, &tmp_err);

    g_assert(tmp_err == NULL);
    g_assert_cmpint(parsed, ==, 2);
}


static void
test_cr_xml_package_iterator_01_warningcb_interrupt(void)
{
    int parsed = 0;
    int numofwarnings = 0;
    GError *tmp_err = NULL;
    cr_Package *package = NULL;

    cr_PkgIterator *pkg_iterator = cr_PkgIterator_new(
        TEST_REPO_02_PRIMARY, TEST_MRF_BAD_TYPE_FIL, TEST_REPO_02_OTHER, NULL, NULL, warningcb_interrupt, &numofwarnings, &tmp_err);

    while ((package = cr_PkgIterator_parse_next(pkg_iterator, &tmp_err))) {
        parsed++;
        cr_package_free(package);
    }

    cr_PkgIterator_free(pkg_iterator, &tmp_err);

    g_assert(tmp_err != NULL);
    g_assert_cmpint(parsed, ==, 0);
    g_assert_cmpint(tmp_err->code, ==, CRE_CBINTERRUPTED);
    g_assert_cmpint(numofwarnings, ==, 1);
    g_clear_error(&tmp_err);
}

static void
test_cr_xml_package_iterator_02_long_primary(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    cr_Package *package = NULL;

    cr_PkgIterator *pkg_iterator = cr_PkgIterator_new(
        TEST_LONG_PRIMARY, TEST_REPO_02_FILELISTS, TEST_REPO_02_OTHER, NULL, NULL, NULL, NULL, &tmp_err);

    while ((package = cr_PkgIterator_parse_next(pkg_iterator, &tmp_err))) {
        parsed++;
        cr_package_free(package);
    }

    g_assert(cr_PkgIterator_is_finished(pkg_iterator));
    cr_PkgIterator_free(pkg_iterator, &tmp_err);

    g_assert(tmp_err == NULL);
    g_assert_cmpint(parsed, ==, 2);
}

static void
test_cr_xml_package_iterator_03_out_of_order_pkgs(void)
{
    GError *tmp_err = NULL;
    cr_Package *package = NULL;

    cr_PkgIterator *pkg_iterator = cr_PkgIterator_new(
        TEST_REPO_02_PRIMARY, TEST_DIFF_ORDER_FILELISTS, TEST_REPO_02_OTHER, NULL, NULL, NULL, NULL, &tmp_err);

    package = cr_PkgIterator_parse_next(pkg_iterator, &tmp_err);

    g_assert(package == NULL);
    g_assert(tmp_err != NULL);
    g_clear_error(&tmp_err);

    cr_PkgIterator_free(pkg_iterator, &tmp_err);
}

static void
test_cr_xml_package_iterator_04_invalid_path(void)
{
    GError *tmp_err = NULL;

    cr_PkgIterator *pkg_iterator = cr_PkgIterator_new(
        "/non/existing/file.xml", TEST_REPO_02_FILELISTS, TEST_REPO_02_OTHER, NULL, NULL, NULL, NULL, &tmp_err);

    g_assert(pkg_iterator == NULL);
    g_assert(tmp_err != NULL);
    g_clear_error(&tmp_err);
}

static void
test_cr_xml_package_iterator_05_newpkgcb_returns_null(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    cr_Package *package = NULL;

    cr_PkgIterator *pkg_iterator = cr_PkgIterator_new(
        TEST_REPO_02_PRIMARY, TEST_REPO_02_FILELISTS, TEST_REPO_02_OTHER, newpkgcb_skip_fake_bash, NULL, NULL, NULL, &tmp_err);

    while ((package = cr_PkgIterator_parse_next(pkg_iterator, &tmp_err))) {
        parsed++;
        cr_package_free(package);
    }

    cr_PkgIterator_free(pkg_iterator, &tmp_err);

    g_assert(tmp_err == NULL);
    g_assert_cmpint(parsed, ==, 1);
}

static void
test_cr_xml_package_iterator_06_newpkgcb_interrupt(void)
{
    GError *tmp_err = NULL;
    cr_Package *package = NULL;
    int new_cb_count = 0;

    cr_PkgIterator *pkg_iterator = cr_PkgIterator_new(
        TEST_REPO_02_PRIMARY, TEST_REPO_02_FILELISTS, TEST_REPO_02_OTHER, newpkgcb_interrupt, &new_cb_count, NULL, NULL, &tmp_err);

    package = cr_PkgIterator_parse_next(pkg_iterator, &tmp_err);

    g_assert(package == NULL);
    g_assert(tmp_err != NULL);
    g_clear_error(&tmp_err);
    g_assert_cmpint(new_cb_count, ==, 1);

    cr_PkgIterator_free(pkg_iterator, &tmp_err);
}

static void
test_cr_xml_package_iterator_07_warnings_bad_file_type(void)
{
    GError *tmp_err = NULL;
    cr_Package *package = NULL;
    GString *warn_strings = g_string_new(0);
    int parsed = 0;

    cr_PkgIterator *pkg_iterator = cr_PkgIterator_new(
        TEST_REPO_02_PRIMARY, TEST_MRF_BAD_TYPE_FIL, TEST_REPO_02_OTHER, NULL, NULL, warningcb, warn_strings, &tmp_err);

    while ((package = cr_PkgIterator_parse_next(pkg_iterator, &tmp_err))) {
        parsed++;
        cr_package_free(package);
    }

    g_assert(tmp_err == NULL);
    g_assert_cmpint(parsed, ==, 2);
    char *warnmsgs = g_string_free(warn_strings, FALSE);
    g_assert_cmpstr(warnmsgs, ==, "Unknown file type \"foo\";");
    g_free(warnmsgs);

    cr_PkgIterator_free(pkg_iterator, &tmp_err);
}

static void
test_cr_xml_package_iterator_08_multiple_warningscb(void)
{
    GError *tmp_err = NULL;
    cr_Package *package = NULL;
    GString *warn_strings = g_string_new(0);
    int parsed = 0;

    cr_PkgIterator *pkg_iterator = cr_PkgIterator_new(
        TEST_PRIMARY_MULTI_WARN_00, TEST_FILELISTS_MULTI_WARN_00, TEST_OTHER_MULTI_WARN_00, NULL, NULL, warningcb, warn_strings, &tmp_err);

    while ((package = cr_PkgIterator_parse_next(pkg_iterator, &tmp_err))) {
        parsed++;
        cr_package_free(package);
    }

    g_assert(tmp_err == NULL);
    g_assert_cmpint(parsed, ==, 2);
    char *warnmsgs = g_string_free(warn_strings, FALSE);
    g_assert_cmpstr(warnmsgs, ==, "Unknown element \"fooelement\";Missing attribute \"type\" of a package element;Unknown element \"foo\";Conversion of \"foobar\" to integer failed;Unknown element \"bar\";Missing attribute \"arch\" of a package element;Unknown file type \"xxx\";Unknown element \"bar\";Missing attribute \"name\" of a package element;Unknown element \"bar\";Conversion of \"xxx\" to integer failed;");
    g_free(warnmsgs);

    cr_PkgIterator_free(pkg_iterator, &tmp_err);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/xml_parser_main_metadata/test_cr_xml_package_iterator_00",
                    test_cr_xml_package_iterator_00);

    g_test_add_func("/xml_parser_main_metadata/test_cr_xml_package_iterator_filelists_ext_00",
                    test_cr_xml_package_iterator_filelists_ext_00);

    g_test_add_func("/xml_parser_main_metadata/test_cr_xml_package_iterator_01_warningcb_interrupt",
                    test_cr_xml_package_iterator_01_warningcb_interrupt);

    g_test_add_func("/xml_parser_main_metadata/test_cr_xml_package_iterator_02_long_primary",
                    test_cr_xml_package_iterator_02_long_primary);

    g_test_add_func("/xml_parser_main_metadata/test_cr_xml_package_iterator_03_out_of_order_pkgs",
                    test_cr_xml_package_iterator_03_out_of_order_pkgs);

    g_test_add_func("/xml_parser_main_metadata/test_cr_xml_package_iterator_04_invalid_path",
                    test_cr_xml_package_iterator_04_invalid_path);

    g_test_add_func("/xml_parser_main_metadata/test_cr_xml_package_iterator_05_newpkgcb_returns_null",
                    test_cr_xml_package_iterator_05_newpkgcb_returns_null);

    g_test_add_func("/xml_parser_main_metadata/test_cr_xml_package_iterator_06_newpkgcb_interrupt",
                    test_cr_xml_package_iterator_06_newpkgcb_interrupt);

    g_test_add_func("/xml_parser_main_metadata/test_cr_xml_package_iterator_07_warnings_bad_file_type",
                    test_cr_xml_package_iterator_07_warnings_bad_file_type);

    g_test_add_func("/xml_parser_main_metadata/test_cr_xml_package_iterator_08_multiple_warningscb",
                    test_cr_xml_package_iterator_08_multiple_warningscb);

    return g_test_run();
}
