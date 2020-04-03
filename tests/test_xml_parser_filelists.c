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
#include "createrepo/xml_parser_internal.h"

//This functions assumes there is enough space in the buffer for the read file plus a terminating NULL
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
    int read = cr_read(orig, buffer, amount, &tmp_err);
    buffer[read] = 0;
    if (orig)
        cr_close(orig, NULL);
    return ret;
}

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

    if (!g_strcmp0(name, "fake_bash"))
        return CRE_OK;

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
test_cr_xml_parse_filelists_00(void)
{
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_REPO_00_FILELISTS, NULL, NULL,
                                     pkgcb, NULL, NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
}

static void
test_cr_xml_parse_filelists_01(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_REPO_01_FILELISTS, NULL, NULL,
                                     pkgcb, &parsed, NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 1);
}

static void
test_cr_xml_parse_filelists_02(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_REPO_02_FILELISTS, NULL, NULL,
                                     pkgcb, &parsed, NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 2);
}

static void
test_cr_xml_parse_filelists_unknown_element_00(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_MRF_UE_FIL_00, NULL, NULL,
                                     pkgcb, &parsed, NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 2);
}

static void
test_cr_xml_parse_filelists_unknown_element_01(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_MRF_UE_FIL_01, NULL, NULL,
                                     pkgcb, &parsed, NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 1);
}

static void
test_cr_xml_parse_filelists_unknown_element_02(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_MRF_UE_FIL_02, NULL, NULL,
                                     pkgcb, &parsed, NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 2);
}

static void
test_cr_xml_parse_filelists_no_pkgid(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_MRF_NO_PKGID_FIL, NULL, NULL,
                                     pkgcb, &parsed, NULL, NULL, &tmp_err);
    g_assert(tmp_err != NULL);
    g_error_free(tmp_err);
    g_assert_cmpint(ret, ==, CRE_BADXMLFILELISTS);
}

static void
test_cr_xml_parse_filelists_skip_fake_bash_00(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_MRF_UE_FIL_00,
                                     newpkgcb_skip_fake_bash, NULL,
                                     pkgcb, &parsed, NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 1);
}

static void
test_cr_xml_parse_filelists_skip_fake_bash_01(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_MRF_UE_FIL_01,
                                     newpkgcb_skip_fake_bash, NULL,
                                     pkgcb, &parsed, NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 0);
}

static void
test_cr_xml_parse_filelists_pkgcb_interrupt(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_REPO_02_FILELISTS, NULL, NULL,
                            pkgcb_interrupt, &parsed, NULL, NULL, &tmp_err);
    g_assert(tmp_err != NULL);
    g_error_free(tmp_err);
    g_assert_cmpint(ret, ==, CRE_CBINTERRUPTED);
    g_assert_cmpint(parsed, ==, 1);
}

static void
test_cr_xml_parse_filelists_newpkgcb_interrupt(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_REPO_02_FILELISTS,
                                     newpkgcb_interrupt, NULL,
                                     pkgcb, &parsed,  NULL, NULL, &tmp_err);
    g_assert(tmp_err != NULL);
    g_error_free(tmp_err);
    g_assert_cmpint(ret, ==, CRE_CBINTERRUPTED);
    g_assert_cmpint(parsed, ==, 0);
}

static void
test_cr_xml_parse_filelists_warningcb_interrupt(void)
{
    int parsed = 0, numofwarnings = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_MRF_BAD_TYPE_FIL,
                                     NULL, NULL,
                                     pkgcb, &parsed,  warningcb_interrupt,
                                     &numofwarnings, &tmp_err);
    g_assert(tmp_err != NULL);
    g_error_free(tmp_err);
    g_assert_cmpint(ret, ==, CRE_CBINTERRUPTED);
    g_assert_cmpint(parsed, ==, 1);
    g_assert_cmpint(numofwarnings, ==, 1);
}

static void
test_cr_xml_parse_filelists_bad_file_type_00(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_MRF_BAD_TYPE_FIL, NULL, NULL,
                                     pkgcb, &parsed, NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 2);
}

static void
test_cr_xml_parse_filelists_bad_file_type_01(void)
{
    char *warnmsgs;
    int parsed = 0;
    GString *warn_strings = g_string_new(0);
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_MRF_BAD_TYPE_FIL, NULL, NULL,
                                     pkgcb, &parsed, warningcb,
                                     warn_strings, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 2);
    warnmsgs = g_string_free(warn_strings, FALSE);
    g_assert_cmpstr(warnmsgs, ==, "Unknown file type \"foo\";");
    g_free(warnmsgs);
}

static void
test_cr_xml_parse_different_md_type(void)
{
    char *warnmsgs;
    int parsed = 0;
    GString *warn_strings = g_string_new(0);
    GError *tmp_err = NULL;
    int ret = cr_xml_parse_filelists(TEST_REPO_01_OTHER, NULL, NULL,
                                     pkgcb, &parsed, warningcb,
                                     warn_strings, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 0);
    warnmsgs = g_string_free(warn_strings, FALSE);
    g_assert_cmpstr(warnmsgs, ==, "Unknown element \"otherdata\";"
            "The target doesn't contain the expected element \"<filelists>\" - "
            "The target probably isn't a valid filelists xml;");
    g_free(warnmsgs);
}

static void
test_cr_xml_parse_filelists_snippet_snippet_01(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    char buf[400];
    read_file(TEST_FILELISTS_SNIPPET_01, CR_CW_AUTO_DETECT_COMPRESSION, buf, 400);
    int ret = cr_xml_parse_filelists_snippet(buf, NULL, NULL, pkgcb, &parsed, NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 1);
}

static void
test_cr_xml_parse_filelists_snippet_snippet_02(void)
{
    int parsed = 0;
    GError *tmp_err = NULL;
    char buf[600];
    read_file(TEST_FILELISTS_SNIPPET_02, CR_CW_AUTO_DETECT_COMPRESSION, buf, 600);
    int ret = cr_xml_parse_filelists_snippet(buf, NULL, NULL, pkgcb, &parsed, NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    g_assert_cmpint(parsed, ==, 2);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_00",
                    test_cr_xml_parse_filelists_00);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_01",
                    test_cr_xml_parse_filelists_01);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_02",
                    test_cr_xml_parse_filelists_02);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_unknown_element_00",
                    test_cr_xml_parse_filelists_unknown_element_00);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_unknown_element_01",
                    test_cr_xml_parse_filelists_unknown_element_01);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_unknown_element_02",
                    test_cr_xml_parse_filelists_unknown_element_02);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_no_pgkid",
                    test_cr_xml_parse_filelists_no_pkgid);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_skip_fake_bash_00",
                    test_cr_xml_parse_filelists_skip_fake_bash_00);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_skip_fake_bash_01",
                    test_cr_xml_parse_filelists_skip_fake_bash_01);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_pkgcb_interrupt",
                    test_cr_xml_parse_filelists_pkgcb_interrupt);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_newpkgcb_interrupt",
                    test_cr_xml_parse_filelists_newpkgcb_interrupt);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_warningcb_interrupt",
                    test_cr_xml_parse_filelists_warningcb_interrupt);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_bad_file_type_00",
                    test_cr_xml_parse_filelists_bad_file_type_00);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_bad_file_type_01",
                    test_cr_xml_parse_filelists_bad_file_type_01);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_different_md_type",
                    test_cr_xml_parse_different_md_type);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_snippet_snippet_01",
                    test_cr_xml_parse_filelists_snippet_snippet_01);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_filelists_snippet_snippet_02",
                    test_cr_xml_parse_filelists_snippet_snippet_02);
    return g_test_run();
}
