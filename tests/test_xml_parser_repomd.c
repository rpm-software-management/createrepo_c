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
#include "createrepo/repomd.h"
#include "createrepo/misc.h"
#include "createrepo/xml_parser.h"

// Callbacks

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
test_cr_xml_parse_repomd_00(void)
{
    GError *tmp_err = NULL;
    cr_Repomd *repomd = cr_repomd_new();

    int ret = cr_xml_parse_repomd(TEST_REPO_00_REPOMD, repomd,
                                  NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);

    cr_repomd_free(repomd);
}

static void
test_cr_xml_parse_repomd_01(void)
{
    GError *tmp_err = NULL;
    cr_Repomd *repomd = cr_repomd_new();

    int ret = cr_xml_parse_repomd(TEST_REPO_01_REPOMD, repomd,
                                  NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);

    cr_repomd_free(repomd);
}

static void
test_cr_xml_parse_repomd_02(void)
{
    GError *tmp_err = NULL;
    char *warnmsgs;
    cr_Repomd *repomd = cr_repomd_new();
    GString *warn_strings = g_string_new(0);

    int ret = cr_xml_parse_repomd(TEST_REPO_02_REPOMD, repomd,
                                  warningcb, warn_strings, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);

    cr_repomd_free(repomd);

    warnmsgs = g_string_free(warn_strings, FALSE);
    g_assert_cmpstr(warnmsgs, ==, "");
    g_free(warnmsgs);
}

static void
test_cr_xml_parse_repomd_warningcb_interrupt(void)
{
    int numofwarnings = 0;
    GError *tmp_err = NULL;
    cr_Repomd *repomd = cr_repomd_new();

    int ret = cr_xml_parse_repomd(TEST_MRF_MISSING_TYPE_REPOMD, repomd,
                                  warningcb_interrupt,
                                  &numofwarnings, &tmp_err);
    g_assert(tmp_err != NULL);
    g_error_free(tmp_err);
    g_assert_cmpint(ret, ==, CRE_CBINTERRUPTED);
    g_assert_cmpint(numofwarnings, ==, 1);
    cr_repomd_free(repomd);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_repomd_00",
                    test_cr_xml_parse_repomd_00);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_repomd_01",
                    test_cr_xml_parse_repomd_01);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_repomd_02",
                    test_cr_xml_parse_repomd_02);
    g_test_add_func("/xml_parser_filelists/test_cr_xml_parse_repomd_warningcb_interrupt",
                    test_cr_xml_parse_repomd_warningcb_interrupt);

    return g_test_run();
}
