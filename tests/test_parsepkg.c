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
#include <string.h>
#include <rpm/header.h>
#include "fixtures.h"
#include "createrepo/error.h"
#include "createrepo/package.h"
#include "createrepo/parsehdr.h"
#include "createrepo/parsepkg.h"
#include "createrepo/xml_dump.h"

#define EMPTY_PKG   TEST_PACKAGES_PATH"empty-0-0.x86_64.rpm"

static void
test_parsepkg_header_replaces_control_chars_in_user_text(void)
{
    Header hdr = headerNew();
    const char *require_names[] = {"bad\x1b" "dependency"};
    const char *require_versions[] = {"0:1-1"};
    uint32_t require_flags[] = {0};
    uint32_t changelog_times[] = {123};
    const char *changelog_names[] = {"author\x02" "name"};
    const char *changelog_texts[] = {"fixed\x03" "bug"};

    g_assert_true(headerPutString(hdr, RPMTAG_NAME, "test-package"));
    g_assert_true(headerPutString(hdr, RPMTAG_VERSION, "1"));
    g_assert_true(headerPutString(hdr, RPMTAG_RELEASE, "1"));
    g_assert_true(headerPutString(hdr, RPMTAG_ARCH, "noarch"));
    g_assert_true(headerPutString(hdr, RPMTAG_SUMMARY, "short\x01" " summary"));
    g_assert_true(headerPutString(hdr, RPMTAG_DESCRIPTION, "line one\nline\x1b" "two"));
    g_assert_true(headerPutStringArray(hdr, RPMTAG_REQUIRENAME, require_names, 1));
    g_assert_true(headerPutUint32(hdr, RPMTAG_REQUIREFLAGS, require_flags, 1));
    g_assert_true(headerPutStringArray(hdr, RPMTAG_REQUIREVERSION, require_versions, 1));
    g_assert_true(headerPutUint32(hdr, RPMTAG_CHANGELOGTIME, changelog_times, 1));
    g_assert_true(headerPutStringArray(hdr, RPMTAG_CHANGELOGNAME, changelog_names, 1));
    g_assert_true(headerPutStringArray(hdr, RPMTAG_CHANGELOGTEXT, changelog_texts, 1));

    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_SUMMARY*U+FFFD*");
    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_DESCRIPTION*U+FFFD*");
    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_CHANGELOGNAME*U+FFFD*");
    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_CHANGELOGTEXT*U+FFFD*");

    GError *err = NULL;
    cr_Package *pkg = cr_package_from_header(hdr, -1, CR_HDRR_NONE, &err);
    g_test_assert_expected_messages();
    g_assert_no_error(err);
    g_assert_nonnull(pkg);
    g_assert_cmpstr(pkg->summary, ==, "short\xEF\xBF\xBD" " summary");
    g_assert_cmpstr(pkg->description, ==, "line one\nline\xEF\xBF\xBD" "two");

    cr_ChangelogEntry *changelog = pkg->changelogs->data;
    g_assert_cmpstr(changelog->author, ==, "author\xEF\xBF\xBD" "name");
    g_assert_cmpstr(changelog->changelog, ==, "fixed\xEF\xBF\xBD" "bug");

    cr_Dependency *require = pkg->requires->data;
    g_assert_cmpstr(require->name, ==, "bad\x1b" "dependency");
    g_assert_true(cr_hascontrollchars((const unsigned char *) require->name));

    cr_xml_dump_init();
    struct cr_XmlStruct xml = cr_xml_dump(pkg, &err);
    g_assert_null(xml.primary);
    g_assert_error(err, CREATEREPO_C_ERROR, CRE_XMLDATA);
    g_clear_error(&err);

    require->name = "valid-dependency";
    xml = cr_xml_dump(pkg, &err);
    g_assert_no_error(err);
    g_assert_nonnull(xml.primary);
    g_assert_nonnull(xml.other);
    g_assert_nonnull(strstr(xml.primary, "short\xEF\xBF\xBD" " summary"));
    g_assert_nonnull(strstr(xml.other, "fixed\xEF\xBF\xBD" "bug"));
    g_free(xml.primary);
    g_free(xml.filelists);
    g_free(xml.filelists_ext);
    g_free(xml.other);
    cr_xml_dump_cleanup();

    cr_package_free(pkg);
    headerFree(hdr);
}


/* Signatures - unsigned package */

static void
test_parsepkg_signatures_not_loaded_without_flag(void)
{
    cr_Package *pkg = cr_package_from_rpm(EMPTY_PKG, CR_CHECKSUM_SHA256,
                                          EMPTY_PKG, NULL, 5, NULL,
                                          CR_HDRR_NONE, NULL);
    g_assert_nonnull(pkg);
    g_assert_null(pkg->signatures);
    cr_package_free(pkg);
}

static void
test_parsepkg_signatures_empty_for_unsigned_package(void)
{
    cr_Package *pkg = cr_package_from_rpm(EMPTY_PKG, CR_CHECKSUM_SHA256,
                                          EMPTY_PKG, NULL, 5, NULL,
                                          CR_HDRR_LOADSIGNATURES, NULL);
    g_assert_nonnull(pkg);
    g_assert_null(pkg->signatures);
    cr_package_free(pkg);
}


/* Signatures - signed packages: flag not set */

static void
test_parsepkg_signatures_not_loaded_without_flag_rsa(void)
{
    cr_Package *pkg = cr_package_from_rpm(PKG_EMPTY_SIGNED_RSA, CR_CHECKSUM_SHA256,
                                          PKG_EMPTY_SIGNED_RSA, NULL, 5, NULL,
                                          CR_HDRR_NONE, NULL);
    g_assert_nonnull(pkg);
    g_assert_null(pkg->signatures);
    cr_package_free(pkg);
}


/* Signatures - signed packages: flag set */

static void
test_parsepkg_signatures_single_eddsa(void)
{
    cr_Package *pkg = cr_package_from_rpm(PKG_EMPTY_SIGNED_EDDSA, CR_CHECKSUM_SHA256,
                                          PKG_EMPTY_SIGNED_EDDSA, NULL, 5, NULL,
                                          CR_HDRR_LOADSIGNATURES, NULL);
    g_assert_nonnull(pkg);
    g_assert_nonnull(pkg->signatures);
    g_assert_cmpuint(g_slist_length(pkg->signatures), ==, 1);
    g_assert_cmpuint(strlen((char *) pkg->signatures->data), >, 0);
    cr_package_free(pkg);
}

static void
test_parsepkg_signatures_single_rsa(void)
{
    cr_Package *pkg = cr_package_from_rpm(PKG_EMPTY_SIGNED_RSA, CR_CHECKSUM_SHA256,
                                          PKG_EMPTY_SIGNED_RSA, NULL, 5, NULL,
                                          CR_HDRR_LOADSIGNATURES, NULL);
    g_assert_nonnull(pkg);
    g_assert_nonnull(pkg->signatures);
    g_assert_cmpuint(g_slist_length(pkg->signatures), ==, 1);
    g_assert_cmpuint(strlen((char *) pkg->signatures->data), >, 0);
    cr_package_free(pkg);
}

static void
test_parsepkg_signatures_v6_multiple(void)
{
    cr_Package *pkg = cr_package_from_rpm(PKG_EMPTY_SIGNED_V6_MULTIPLE, CR_CHECKSUM_SHA256,
                                          PKG_EMPTY_SIGNED_V6_MULTIPLE, NULL, 5, NULL,
                                          CR_HDRR_LOADSIGNATURES, NULL);
    g_assert_nonnull(pkg);
    g_assert_nonnull(pkg->signatures);
    g_assert_cmpuint(g_slist_length(pkg->signatures), ==, 2);
    for (GSList *elem = pkg->signatures; elem; elem = g_slist_next(elem)) {
        g_assert_cmpuint(strlen((char *) elem->data), >, 0);
    }
    cr_package_free(pkg);
}


int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    cr_package_parser_init();

    g_test_add_func("/parsepkg/header_replaces_control_chars_in_user_text",
                    test_parsepkg_header_replaces_control_chars_in_user_text);
    g_test_add_func("/parsepkg/signatures_not_loaded_without_flag",
                    test_parsepkg_signatures_not_loaded_without_flag);
    g_test_add_func("/parsepkg/signatures_empty_for_unsigned_package",
                    test_parsepkg_signatures_empty_for_unsigned_package);
    g_test_add_func("/parsepkg/signatures_not_loaded_without_flag_rsa",
                    test_parsepkg_signatures_not_loaded_without_flag_rsa);
    g_test_add_func("/parsepkg/signatures_single_eddsa",
                    test_parsepkg_signatures_single_eddsa);
    g_test_add_func("/parsepkg/signatures_single_rsa",
                    test_parsepkg_signatures_single_rsa);

    // Disabled until all CI platforms recognize RPMTAG_OPENPGP
    // https://github.com/rpm-software-management/createrepo_c/pull/477#discussion_r2920022135

    // g_test_add_func("/parsepkg/signatures_v6_multiple",
    //                 test_parsepkg_signatures_v6_multiple);

    int ret = g_test_run();
    cr_package_parser_cleanup();
    return ret;
}
