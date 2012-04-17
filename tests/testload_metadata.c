#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include "createrepo/package.h"
#include "createrepo/misc.h"
#include "createrepo/load_metadata.h"

#define REPO_PATH_00    "test_data/repo_00/"
#define REPO_SIZE_00    0
static const char *REPO_HASH_KEYS_00[] = {};
static const char *REPO_NAME_KEYS_00[] = {};
static const char *REPO_FILENAME_KEYS_00[] = {};

#define REPO_PATH_01    "test_data/repo_01/"
#define REPO_SIZE_01    1
static const char *REPO_HASH_KEYS_01[] = {"152824bff2aa6d54f429d43e87a3ff3a0286505c6d93ec87692b5e3a9e3b97bf"};
static const char *REPO_NAME_KEYS_01[] = {"super_kernel"};
static const char *REPO_FILENAME_KEYS_01[] = {"super_kernel-6.0.1-2.x86_64.rpm"};

#define REPO_PATH_02    "test_data/repo_02/"
#define REPO_SIZE_02    2
static const char *REPO_HASH_KEYS_02[] = {"6d43a638af70ef899933b1fd86a866f18f65b0e0e17dcbf2e42bfd0cdd7c63c3",
                                          "90f61e546938a11449b710160ad294618a5bd3062e46f8cf851fd0088af184b7"};
static const char *REPO_NAME_KEYS_02[] = {"super_kernel",
                                          "fake_bash"};
static const char *REPO_FILENAME_KEYS_02[] = {"super_kernel-6.0.1-2.x86_64.rpm",
                                              "fake_bash-1.1.1-1.x86_64.rpm"};



static void test_new_metadata_hashtable(void)
{
    guint len;
    GHashTable *hashtable = NULL;

    // Get new hashtable
    hashtable = new_metadata_hashtable();
    g_assert(hashtable);

    // Check if it is empty
    len = g_hash_table_size(hashtable);
    g_assert_cmpint(len, ==, 0);

    destroy_metadata_hashtable(hashtable);
}


static void test_destroy_metadata_hashtable(void)
{
    GHashTable *hashtable = new_metadata_hashtable();
    g_assert(hashtable);
    destroy_metadata_hashtable(hashtable);

    destroy_metadata_hashtable(NULL);
}


void test_helper_check_keys(const char *repopath, HashTableKey key, guint repo_size, const char *keys[])
{
    int ret;
    guint i;
    guint size;
    gpointer value;
    GHashTable *hashtable;

    hashtable = new_metadata_hashtable();
    g_assert(hashtable);
    ret = locate_and_load_xml_metadata(hashtable, repopath, key);
    g_assert_cmpint(ret, ==, LOAD_METADATA_OK);
    size = g_hash_table_size(hashtable);
    g_assert_cmpuint(size, ==, repo_size);
    for (i=0; i < repo_size; i++) {
        value = g_hash_table_lookup(hashtable, (gconstpointer) keys[i]);
        if (!value)
            g_critical("Key \"%s\" not present!", keys[i]);
    }
    destroy_metadata_hashtable(hashtable);
}


static void test_locate_and_load_xml_metadata(void)
{
    test_helper_check_keys(REPO_PATH_00, HT_KEY_HASH, REPO_SIZE_00, REPO_HASH_KEYS_00);
    test_helper_check_keys(REPO_PATH_00, HT_KEY_NAME, REPO_SIZE_00, REPO_NAME_KEYS_00);
    test_helper_check_keys(REPO_PATH_00, HT_KEY_FILENAME, REPO_SIZE_00, REPO_FILENAME_KEYS_00);

    test_helper_check_keys(REPO_PATH_01, HT_KEY_HASH, REPO_SIZE_01, REPO_HASH_KEYS_01);
    test_helper_check_keys(REPO_PATH_01, HT_KEY_NAME, REPO_SIZE_01, REPO_NAME_KEYS_01);
    test_helper_check_keys(REPO_PATH_01, HT_KEY_FILENAME, REPO_SIZE_01, REPO_FILENAME_KEYS_01);

    test_helper_check_keys(REPO_PATH_02, HT_KEY_HASH, REPO_SIZE_02, REPO_HASH_KEYS_02);
    test_helper_check_keys(REPO_PATH_02, HT_KEY_NAME, REPO_SIZE_02, REPO_NAME_KEYS_02);
    test_helper_check_keys(REPO_PATH_02, HT_KEY_FILENAME, REPO_SIZE_02, REPO_FILENAME_KEYS_02);
}


static void test_locate_and_load_xml_metadata_detailed(void)
{
    int ret;
    guint size;
    Package *pkg;
    GHashTable *hashtable;

    hashtable = new_metadata_hashtable();
    g_assert(hashtable);
    ret = locate_and_load_xml_metadata(hashtable, REPO_PATH_01, HT_KEY_NAME);
    g_assert_cmpint(ret, ==, LOAD_METADATA_OK);
    size = g_hash_table_size(hashtable);
    g_assert_cmpuint(size, ==, REPO_SIZE_01);
    pkg = (Package *) g_hash_table_lookup(hashtable, "super_kernel");
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
    g_assert_cmpstr(pkg->rpm_vendor, ==, "");
    g_assert_cmpstr(pkg->rpm_group, ==, "Applications/System");
    g_assert_cmpstr(pkg->rpm_buildhost, ==, "localhost.localdomain");
    g_assert_cmpstr(pkg->rpm_sourcerpm, ==, "super_kernel-6.0.1-2.src.rpm");
    g_assert_cmpint(pkg->rpm_header_start, ==, 280);
    g_assert_cmpint(pkg->rpm_header_end, ==, 2637);
    g_assert_cmpstr(pkg->rpm_packager, ==, "");
    g_assert_cmpint(pkg->size_package, ==, 2845);
    g_assert_cmpint(pkg->size_installed, ==, 0);
    g_assert_cmpint(pkg->size_archive, ==, 404);
    g_assert_cmpstr(pkg->location_href, ==, "super_kernel-6.0.1-2.x86_64.rpm");
    g_assert(!pkg->location_base);
    g_assert_cmpstr(pkg->checksum_type, ==, "sha256");

    destroy_metadata_hashtable(hashtable);
}


int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/load_metadata/test_new_metadata_hashtable", test_new_metadata_hashtable);
    g_test_add_func("/load_metadata/test_destroy_metadata_hashtable", test_destroy_metadata_hashtable);
    g_test_add_func("/load_metadata/test_locate_and_load_xml_metadata", test_locate_and_load_xml_metadata);
    g_test_add_func("/load_metadata/test_locate_and_load_xml_metadata_detailed", test_locate_and_load_xml_metadata_detailed);

    return g_test_run();
}
