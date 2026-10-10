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
    g_assert_true(headerPutString(hdr, RPMTAG_URL, "https://example.test/\x01"));
    g_assert_true(headerPutString(hdr, RPMTAG_LICENSE, "license\x02"));
    g_assert_true(headerPutString(hdr, RPMTAG_VENDOR, "vendor\x03"));
    g_assert_true(headerPutString(hdr, RPMTAG_GROUP, "group\x04"));
    g_assert_true(headerPutString(hdr, RPMTAG_BUILDHOST, "host\x05"));
    g_assert_true(headerPutString(hdr, RPMTAG_PACKAGER, "packager\x06"));
    g_assert_true(headerPutStringArray(hdr, RPMTAG_REQUIRENAME, require_names, 1));
    g_assert_true(headerPutUint32(hdr, RPMTAG_REQUIREFLAGS, require_flags, 1));
    g_assert_true(headerPutStringArray(hdr, RPMTAG_REQUIREVERSION, require_versions, 1));
    g_assert_true(headerPutUint32(hdr, RPMTAG_CHANGELOGTIME, changelog_times, 1));
    g_assert_true(headerPutStringArray(hdr, RPMTAG_CHANGELOGNAME, changelog_names, 1));
    g_assert_true(headerPutStringArray(hdr, RPMTAG_CHANGELOGTEXT, changelog_texts, 1));

    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_SUMMARY*U+FFFD*");
    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_DESCRIPTION*U+FFFD*");
    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_URL*U+FFFD*");
    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_LICENSE*U+FFFD*");
    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_VENDOR*U+FFFD*");
    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_GROUP*U+FFFD*");
    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_BUILDHOST*U+FFFD*");
    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_PACKAGER*U+FFFD*");
    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_CHANGELOGNAME*U+FFFD*");
    g_test_expect_message("C_CREATEREPOLIB", G_LOG_LEVEL_WARNING, "*RPMTAG_CHANGELOGTEXT*U+FFFD*");

    GError *err = NULL;
    cr_Package *pkg = cr_package_from_header(hdr, -1, CR_HDRR_NONE, &err);
    g_test_assert_expected_messages();
    g_assert_no_error(err);
    g_assert_nonnull(pkg);
    g_assert_cmpstr(pkg->summary, ==, "short\xEF\xBF\xBD" " summary");
    g_assert_cmpstr(pkg->description, ==, "line one\nline\xEF\xBF\xBD" "two");
    g_assert_cmpstr(pkg->url, ==, "https://example.test/\xEF\xBF\xBD");
    g_assert_cmpstr(pkg->rpm_license, ==, "license\xEF\xBF\xBD");
    g_assert_cmpstr(pkg->rpm_vendor, ==, "vendor\xEF\xBF\xBD");
    g_assert_cmpstr(pkg->rpm_group, ==, "group\xEF\xBF\xBD");
    g_assert_cmpstr(pkg->rpm_buildhost, ==, "host\xEF\xBF\xBD");
    g_assert_cmpstr(pkg->rpm_packager, ==, "packager\xEF\xBF\xBD");

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


int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    cr_package_parser_init();

    g_test_add_func("/parsepkg/header_replaces_control_chars_in_user_text",
                    test_parsepkg_header_replaces_control_chars_in_user_text);

    int ret = g_test_run();
    cr_package_parser_cleanup();
    return ret;
}
