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
#include <stdlib.h>
#include <stdio.h>
#include "fixtures.h"
#include "createrepo/error.h"
#include "createrepo/package.h"
#include "createrepo/misc.h"
#include "createrepo/load_metadata.h"
#include "createrepo/locate_metadata.h"
#include "createrepo/metadata_internal.h"

#define REPO_SIZE_00    0

#define REPO_SIZE_01    1
static const char *REPO_HASH_KEYS_01[] = {"152824bff2aa6d54f429d43e87a3ff3a0286505c6d93ec87692b5e3a9e3b97bf"};
static const char *REPO_NAME_KEYS_01[] = {"super_kernel"};
static const char *REPO_FILENAME_KEYS_01[] = {"super_kernel-6.0.1-2.x86_64.rpm"};

#define REPO_SIZE_02    2
static const char *REPO_HASH_KEYS_02[] = {"6d43a638af70ef899933b1fd86a866f18f65b0e0e17dcbf2e42bfd0cdd7c63c3",
                                          "90f61e546938a11449b710160ad294618a5bd3062e46f8cf851fd0088af184b7"};
static const char *REPO_NAME_KEYS_02[] = {"super_kernel",
                                          "fake_bash"};
static const char *REPO_FILENAME_KEYS_02[] = {"super_kernel-6.0.1-2.x86_64.rpm",
                                              "fake_bash-1.1.1-1.x86_64.rpm"};

#define REPO_SIZE_03    0



static void test_cr_metadata_new(void)
{
    guint len;
    cr_Metadata *metadata = NULL;

    // Get new metadata object
    metadata = cr_metadata_new(CR_HT_KEY_DEFAULT, 0, NULL);
    g_assert(metadata);

    // Check if it is empty
    len = g_hash_table_size(cr_metadata_hashtable(metadata));
    g_assert_cmpint(len, ==, 0);

    cr_metadata_free(metadata);
}


void test_helper_check_keys(const char *repopath, cr_HashTableKey key, guint repo_size, const char *keys[])
{
    int ret;
    guint i;
    guint size;
    gpointer value;
    cr_Metadata *metadata;

    metadata = cr_metadata_new(key, 0, NULL);
    g_assert(metadata);
    g_assert(cr_metadata_hashtable(metadata));
    ret = cr_metadata_locate_and_load_xml(metadata, repopath, NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    size = g_hash_table_size(cr_metadata_hashtable(metadata));
    g_assert_cmpuint(size, ==, repo_size);
    for (i=0; i < repo_size; i++) {
        value = g_hash_table_lookup(cr_metadata_hashtable(metadata),
                                    (gconstpointer) keys[i]);
        if (!value)
            g_critical("Key \"%s\" not present!", keys[i]);
    }
    cr_metadata_free(metadata);
}


static void test_cr_metadata_locate_and_load_xml(void)
{
    test_helper_check_keys(TEST_REPO_00, CR_HT_KEY_HASH, REPO_SIZE_00, NULL);
    test_helper_check_keys(TEST_REPO_00, CR_HT_KEY_NAME, REPO_SIZE_00, NULL);
    test_helper_check_keys(TEST_REPO_00, CR_HT_KEY_FILENAME, REPO_SIZE_00, NULL);

    test_helper_check_keys(TEST_REPO_01, CR_HT_KEY_HASH, REPO_SIZE_01, REPO_HASH_KEYS_01);
    test_helper_check_keys(TEST_REPO_01, CR_HT_KEY_NAME, REPO_SIZE_01, REPO_NAME_KEYS_01);
    test_helper_check_keys(TEST_REPO_01, CR_HT_KEY_FILENAME, REPO_SIZE_01, REPO_FILENAME_KEYS_01);

    test_helper_check_keys(TEST_REPO_02, CR_HT_KEY_HASH, REPO_SIZE_02, REPO_HASH_KEYS_02);
    test_helper_check_keys(TEST_REPO_02, CR_HT_KEY_NAME, REPO_SIZE_02, REPO_NAME_KEYS_02);
    test_helper_check_keys(TEST_REPO_02, CR_HT_KEY_FILENAME, REPO_SIZE_02, REPO_FILENAME_KEYS_02);

#ifdef WITH_LIBMODULEMD
    test_helper_check_keys(TEST_REPO_03, CR_HT_KEY_HASH, REPO_SIZE_03, NULL);
    test_helper_check_keys(TEST_REPO_03, CR_HT_KEY_NAME, REPO_SIZE_03, NULL);
    test_helper_check_keys(TEST_REPO_03, CR_HT_KEY_FILENAME, REPO_SIZE_03, NULL);
#else
    /* If we don't have libmodulemd support, this should fail to locate and
     * return CRE_MODULEMD
     */
    struct cr_MetadataLocation *ml;
    g_autoptr (GError) err = NULL;

    ml = cr_locate_metadata(TEST_REPO_03, TRUE, &err);
    g_assert_error (err, CREATEREPO_C_ERROR, CRE_MODULEMD);
#endif /* WITH_LIBMODULEMD */
}


static void test_cr_metadata_locate_and_load_xml_detailed(void)
{
    int ret;
    guint size;
    cr_Package *pkg;
    cr_Metadata *metadata;

    metadata = cr_metadata_new(CR_HT_KEY_NAME, 0, NULL);
    g_assert(metadata);
    ret = cr_metadata_locate_and_load_xml(metadata, TEST_REPO_01, NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    size = g_hash_table_size(cr_metadata_hashtable(metadata));
    g_assert_cmpuint(size, ==, REPO_SIZE_01);
    pkg = (cr_Package *) g_hash_table_lookup(cr_metadata_hashtable(metadata),
                                             "super_kernel");
    g_assert(pkg);

    g_assert_cmpstr(pkg->pkgId, ==, "152824bff2aa6d54f429d43e87a3ff3a0286505c6d93ec87692b5e3a9e3b97bf");
    g_assert_cmpstr(pkg->name, ==, "super_kernel");
    g_assert_cmpstr(pkg->arch, ==, "x86_64");
    g_assert_cmpstr(pkg->version, ==, "6.0.1");
    g_assert_cmpstr(pkg->epoch, ==, "0");
    g_assert_cmpstr(pkg->release, ==, "2");
    g_assert_cmpstr(pkg->summary, ==, "Test package");
    g_assert_cmpstr(pkg->description, ==, "This package has provides, requires, obsoletes, conflicts options.");
    g_assert_cmpstr(pkg->url, ==, "http://so_super_kernel.com/it_is_awesome/yep_it_really_is");
    g_assert_cmpint(pkg->time_file, ==, 1334667003);
    g_assert_cmpint(pkg->time_build, ==, 1334667003);
    g_assert_cmpstr(pkg->rpm_license, ==, "LGPLv2");
    g_assert_cmpstr(pkg->rpm_vendor, ==, NULL);
    g_assert_cmpstr(pkg->rpm_group, ==, "Applications/System");
    g_assert_cmpstr(pkg->rpm_buildhost, ==, "localhost.localdomain");
    g_assert_cmpstr(pkg->rpm_sourcerpm, ==, "super_kernel-6.0.1-2.src.rpm");
    g_assert_cmpint(pkg->rpm_header_start, ==, 280);
    g_assert_cmpint(pkg->rpm_header_end, ==, 2637);
    g_assert_cmpstr(pkg->rpm_packager, ==, NULL);
    g_assert_cmpint(pkg->size_package, ==, 2845);
    g_assert_cmpint(pkg->size_installed, ==, 0);
    g_assert_cmpint(pkg->size_archive, ==, 404);
    g_assert_cmpstr(pkg->location_href, ==, "super_kernel-6.0.1-2.x86_64.rpm");
    g_assert(!pkg->location_base);
    g_assert_cmpstr(pkg->checksum_type, ==, "sha256");

    cr_metadata_free(metadata);
}


#ifdef WITH_LIBMODULEMD
static void test_cr_metadata_locate_and_load_modulemd(void)
{
    int ret;
    guint size;
    cr_Metadata *metadata;

    metadata = cr_metadata_new(CR_HT_KEY_NAME, 0, NULL);
    g_assert(metadata);
    ret = cr_metadata_locate_and_load_xml(metadata, TEST_REPO_03, NULL);
    g_assert_cmpint(ret, ==, CRE_OK);
    size = g_hash_table_size(cr_metadata_hashtable(metadata));
    g_assert_cmpuint(size, ==, REPO_SIZE_03);

    /* If loaded successfully, the index should contain a module named
     * testmodule
     */
    g_assert_nonnull (modulemd_module_index_get_module (
                      cr_metadata_modulemd(metadata),
                      "testmodule"));
}
#endif /* WITH_LIBMODULEMD */


int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/load_metadata/test_cr_metadata_new", test_cr_metadata_new);
    g_test_add_func("/load_metadata/test_cr_metadata_locate_and_load_xml", test_cr_metadata_locate_and_load_xml);
    g_test_add_func("/load_metadata/test_cr_metadata_locate_and_load_xml_detailed", test_cr_metadata_locate_and_load_xml_detailed);

#ifdef WITH_LIBMODULEMD
    g_test_add_func("/load_metadata/test_cr_metadata_locate_and_load_modulemd", test_cr_metadata_locate_and_load_modulemd);
#endif /* WITH_LIBMODULEMD */

    return g_test_run();
}
