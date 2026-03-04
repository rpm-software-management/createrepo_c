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
#include "fixtures.h"
#include "createrepo/package.h"
#include "createrepo/parsepkg.h"

#define EMPTY_PKG   TEST_PACKAGES_PATH"empty-0-0.x86_64.rpm"


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
    g_assert_cmpuint(g_slist_length(pkg->signatures), >=, 2);
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
    g_test_add_func("/parsepkg/signatures_v6_multiple",
                    test_parsepkg_signatures_v6_multiple);

    int ret = g_test_run();
    cr_package_parser_cleanup();
    return ret;
}
