/*
 * Copyright (C) 2018 Red Hat, Inc.
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

#include <stdio.h>
#include "fixtures.h"
#include "createrepo/koji.h"
#include "createrepo/load_metadata.h"

// Tests

static void
test_koji_stuff_00(void)
{
    gchar *template = g_strdup(TMPDIR_TEMPLATE);
    gchar *tmp = g_strconcat(mkdtemp(template), "/", NULL);
    struct KojiMergedReposStuff *koji_stuff = NULL;
    struct CmdOptions o = {.koji=1, .koji_simple=1, .blocked=NULL, .tmp_out_repo=tmp};

    GSList *local_repos = NULL;
    struct cr_MetadataLocation *loc = cr_locate_metadata((gchar *) TEST_REPO_00, TRUE, NULL);
    local_repos = g_slist_prepend(local_repos, loc);
    loc = cr_locate_metadata((gchar *) TEST_REPO_01, TRUE, NULL);
    local_repos = g_slist_prepend(local_repos, loc);
    loc = cr_locate_metadata((gchar *) TEST_REPO_02, TRUE, NULL);
    local_repos = g_slist_prepend(local_repos, loc);
    loc = cr_locate_metadata((gchar *) TEST_REPO_KOJI_01, TRUE, NULL);
    local_repos = g_slist_prepend(local_repos, loc);

    int ret = koji_stuff_prepare(&koji_stuff, &o, local_repos);
    g_assert_cmpint(ret, ==, 0);

    //we have only 3 uniq srpm names
    g_assert_cmpint(g_hash_table_size(koji_stuff->include_srpms), ==, 3);
    g_assert(g_hash_table_contains(koji_stuff->include_srpms, "dwm"));
    g_assert(g_hash_table_contains(koji_stuff->include_srpms, "fake_bash"));
    g_assert(g_hash_table_contains(koji_stuff->include_srpms, "super_kernel"));

    g_assert(!koji_stuff->blocked_srpms);
    g_assert(koji_stuff->simple);

    gchar *origins_file_path = g_strconcat(tmp, "pkgorigins.gz", NULL);
    g_assert(g_file_test(origins_file_path, G_FILE_TEST_EXISTS));

    g_assert_cmpint(g_hash_table_size(koji_stuff->seen_rpms), ==, 0);

    koji_stuff_destroy(&koji_stuff);

    g_free(tmp);
    g_slist_free_full(local_repos, (GDestroyNotify) cr_metadatalocation_free);
    g_free(origins_file_path);
    g_free(template);
}

static void
test_koji_stuff_01(void)
{
    gchar *template = g_strdup(TMPDIR_TEMPLATE);
    gchar *tmp = g_strconcat(mkdtemp(template), "/", NULL);
    gchar *blocked_files_path = g_strconcat(tmp, "blocked.txt", NULL);
    FILE *blocked = fopen(blocked_files_path, "w");
    fprintf(blocked, "super_kernel\nfake_kernel\nfake_bash");
    fclose(blocked);

    struct KojiMergedReposStuff *koji_stuff = NULL;
    struct CmdOptions o = {.koji=0, .blocked=blocked_files_path, .tmp_out_repo=tmp};

    GSList *local_repos = NULL;
    struct cr_MetadataLocation *loc = cr_locate_metadata((gchar *) TEST_REPO_00, TRUE, NULL);
    local_repos = g_slist_prepend(local_repos, loc);
    loc = cr_locate_metadata((gchar *) TEST_REPO_01, TRUE, NULL);
    local_repos = g_slist_prepend(local_repos, loc);
    loc = cr_locate_metadata((gchar *) TEST_REPO_02, TRUE, NULL);
    local_repos = g_slist_prepend(local_repos, loc);
    loc = cr_locate_metadata((gchar *) TEST_REPO_KOJI_01, TRUE, NULL);
    local_repos = g_slist_prepend(local_repos, loc);

    int ret = koji_stuff_prepare(&koji_stuff, &o, local_repos);
    g_assert_cmpint(ret, ==, 0);

    //we have only 3 uniq srpm names
    g_assert_cmpint(g_hash_table_size(koji_stuff->include_srpms), ==, 3);
    g_assert(g_hash_table_contains(koji_stuff->include_srpms, "dwm"));
    g_assert(g_hash_table_contains(koji_stuff->include_srpms, "fake_bash"));
    g_assert(g_hash_table_contains(koji_stuff->include_srpms, "super_kernel"));

    g_assert(koji_stuff->blocked_srpms);
    g_assert_cmpint(g_hash_table_size(koji_stuff->blocked_srpms), ==, 3);
    g_assert(g_hash_table_contains(koji_stuff->blocked_srpms, "super_kernel"));
    g_assert(g_hash_table_contains(koji_stuff->blocked_srpms, "fake_kernel"));
    g_assert(g_hash_table_contains(koji_stuff->blocked_srpms, "fake_bash"));

    g_assert_cmpint(g_hash_table_size(koji_stuff->seen_rpms), ==, 0);

    gchar *origins_file_path = g_strconcat(tmp, "pkgorigins.gz", NULL);
    g_assert(g_file_test(origins_file_path, G_FILE_TEST_EXISTS));

    g_assert(!koji_stuff->simple);

    koji_stuff_destroy(&koji_stuff);

    g_slist_free_full(local_repos, (GDestroyNotify) cr_metadatalocation_free);
    g_free(origins_file_path);
    g_free(blocked_files_path);
    g_free(tmp);
    g_free(template);
}

static void
test_koji_stuff_02_get_newest_srpm_from_one_repo(void)
{
    gchar *template = g_strdup(TMPDIR_TEMPLATE);
    gchar *tmp = g_strconcat(mkdtemp(template), "/", NULL);

    struct KojiMergedReposStuff *koji_stuff = NULL;
    struct CmdOptions o = {.koji=0, .blocked=NULL, .tmp_out_repo=tmp};

    GSList *local_repos = NULL;
    struct cr_MetadataLocation *loc = cr_locate_metadata((gchar *) TEST_REPO_KOJI_01, TRUE, NULL);
    local_repos = g_slist_prepend(local_repos, loc);

    int ret = koji_stuff_prepare(&koji_stuff, &o, local_repos);
    g_assert_cmpint(ret, ==, 0);

    g_assert_cmpint(g_hash_table_size(koji_stuff->include_srpms), ==, 1);
    g_assert(g_hash_table_contains(koji_stuff->include_srpms, "dwm"));
    struct srpm_val *value = g_hash_table_lookup(koji_stuff->include_srpms, "dwm");
    g_assert_cmpstr(value->sourcerpm, ==, "dwm-6.1-7.fc28.src.rpm");

    koji_stuff_destroy(&koji_stuff);

    g_slist_free_full(local_repos, (GDestroyNotify) cr_metadatalocation_free);
    g_free(tmp);
    g_free(template);
}

static void
test_koji_stuff_03_get_srpm_from_first_repo_even_if_its_older(void)
{
    gchar *template = g_strdup(TMPDIR_TEMPLATE);
    gchar *tmp = g_strconcat(mkdtemp(template), "/", NULL);

    struct KojiMergedReposStuff *koji_stuff = NULL;
    struct CmdOptions o = {.koji=0, .blocked=NULL, .tmp_out_repo=tmp};

    GSList *local_repos = NULL;
    struct cr_MetadataLocation *loc = cr_locate_metadata((gchar *) TEST_REPO_KOJI_01, TRUE, NULL);
    local_repos = g_slist_prepend(local_repos, loc);
    loc = cr_locate_metadata((gchar *) TEST_REPO_KOJI_02, TRUE, NULL);
    local_repos = g_slist_prepend(local_repos, loc);

    int ret = koji_stuff_prepare(&koji_stuff, &o, local_repos);
    g_assert_cmpint(ret, ==, 0);

    g_assert_cmpint(g_hash_table_size(koji_stuff->include_srpms), ==, 1);
    g_assert(g_hash_table_contains(koji_stuff->include_srpms, "dwm"));
    struct srpm_val *value = g_hash_table_lookup(koji_stuff->include_srpms, "dwm");
    g_assert_cmpstr(value->sourcerpm, ==, "dwm-5.8.2-2.src.rpm");

    koji_stuff_destroy(&koji_stuff);

    g_slist_free_full(local_repos, (GDestroyNotify) cr_metadatalocation_free);
    g_free(tmp);
    g_free(template);
}

struct KojiMergedReposStuff *
create_empty_koji_stuff_for_test(gboolean simple)
{
    struct KojiMergedReposStuff *koji_stuff;
    koji_stuff = g_malloc0(sizeof(struct KojiMergedReposStuff));
    koji_stuff->include_srpms = g_hash_table_new_full(g_str_hash,
                                                      g_str_equal,
                                                      g_free,
                                                      cr_srpm_val_destroy);
    koji_stuff->seen_rpms = g_hash_table_new_full(g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  NULL);
    koji_stuff->blocked_srpms = g_hash_table_new_full(g_str_hash,
                                                      g_str_equal,
                                                      g_free,
                                                      NULL);

    koji_stuff->simple = simple;

    return koji_stuff;
}

struct srpm_val *
create_srpm_val(int repoid, gchar* rpm_source)
{
    struct srpm_val *srpm_value_new;
    srpm_value_new = g_malloc0(sizeof(struct srpm_val));
    srpm_value_new->repo_id = repoid;
    srpm_value_new->sourcerpm = g_strdup(rpm_source);
    return srpm_value_new;
}

void
koji_stuff_destroy_after_test(struct KojiMergedReposStuff **koji_stuff_ptr)
{
    struct KojiMergedReposStuff *koji_stuff;

    if (!koji_stuff_ptr || !*koji_stuff_ptr)
        return;

    koji_stuff = *koji_stuff_ptr;

    if (koji_stuff->blocked_srpms)
        g_hash_table_destroy(koji_stuff->blocked_srpms);
    g_hash_table_destroy(koji_stuff->include_srpms);
    g_hash_table_destroy(koji_stuff->seen_rpms);
    cr_close(koji_stuff->pkgorigins, NULL);
    g_free(koji_stuff);
}

static void
test_koji_allowed_pkg_not_included(void)
{
    cr_Package *pkg =  get_package();
    struct KojiMergedReposStuff *koji_stuff = create_empty_koji_stuff_for_test(0);
    g_hash_table_replace(koji_stuff->include_srpms,
                         g_strdup_printf("dwm"),
                         create_srpm_val(0, "dwm-5.8.2-2.src.rpm"));

    g_assert(!koji_allowed(pkg, koji_stuff));

    koji_stuff_destroy_after_test(&koji_stuff);
    cr_package_free(pkg);
}

static void
test_koji_allowed_pkg_included(void)
{
    cr_Package *pkg =  get_package();
    struct KojiMergedReposStuff *koji_stuff = create_empty_koji_stuff_for_test(0);
    g_hash_table_replace(koji_stuff->include_srpms,
                         g_strdup_printf("foo"),
                         create_srpm_val(0, "foo.src.rpm"));

    g_assert(koji_allowed(pkg, koji_stuff));

    g_assert_cmpint(g_hash_table_size(koji_stuff->seen_rpms), ==, 1);
    g_assert(g_hash_table_contains(koji_stuff->seen_rpms, cr_package_nvra(pkg)));

    koji_stuff_destroy(&koji_stuff);
    cr_package_free(pkg);
}

static void
test_koji_allowed_pkg_blocked(void)
{
    cr_Package *pkg =  get_package();
    struct KojiMergedReposStuff *koji_stuff = create_empty_koji_stuff_for_test(0);
    g_hash_table_replace(koji_stuff->include_srpms,
                         g_strdup_printf("foo"),
                         create_srpm_val(0, "foo.src.rpm"));
    g_hash_table_replace(koji_stuff->blocked_srpms,
                         g_strdup_printf("foo"),
                         NULL);

    g_assert(!koji_allowed(pkg, koji_stuff));

    koji_stuff_destroy(&koji_stuff);
    cr_package_free(pkg);
}

static void
test_koji_allowed_pkg_already_seen(void)
{
    cr_Package *pkg =  get_package();
    struct KojiMergedReposStuff *koji_stuff = create_empty_koji_stuff_for_test(0);
    g_hash_table_replace(koji_stuff->include_srpms,
                         g_strdup_printf("foo"),
                         create_srpm_val(0, "foo.src.rpm"));

    g_assert(koji_allowed(pkg, koji_stuff));
    g_assert(!koji_allowed(pkg, koji_stuff));

    koji_stuff_destroy(&koji_stuff);
    cr_package_free(pkg);
}

static void
test_koji_allowed_simple_ignores_include(void)
{
    cr_Package *pkg =  get_package();
    struct KojiMergedReposStuff *koji_stuff = create_empty_koji_stuff_for_test(1);
    g_hash_table_replace(koji_stuff->include_srpms,
                         g_strdup_printf("foo22"),
                         create_srpm_val(0, "foo22.src.rpm"));

    g_assert(koji_allowed(pkg, koji_stuff));

    koji_stuff_destroy(&koji_stuff);
    cr_package_free(pkg);
}

static void
test_koji_allowed_simple_ignores_seen(void)
{
    cr_Package *pkg =  get_package();
    struct KojiMergedReposStuff *koji_stuff = create_empty_koji_stuff_for_test(1);
    g_hash_table_replace(koji_stuff->include_srpms,
                         g_strdup_printf("foo22"),
                         create_srpm_val(0, "foo22.src.rpm"));

    //we can add the same package more than once
    g_assert(koji_allowed(pkg, koji_stuff));
    g_assert(koji_allowed(pkg, koji_stuff));
    g_assert(koji_allowed(pkg, koji_stuff));

    koji_stuff_destroy(&koji_stuff);
    cr_package_free(pkg);
}

static void
test_koji_allowed_simple_respects_blocked(void)
{
    cr_Package *pkg =  get_package();
    struct KojiMergedReposStuff *koji_stuff = create_empty_koji_stuff_for_test(1);
    g_hash_table_replace(koji_stuff->blocked_srpms,
                         g_strdup_printf("foo"),
                         NULL);

    g_assert(!koji_allowed(pkg, koji_stuff));

    koji_stuff_destroy(&koji_stuff);
    cr_package_free(pkg);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/mergerepo_c/test_koji_stuff_00",
                   test_koji_stuff_00);
    g_test_add_func("/mergerepo_c/test_koji_stuff_01",
                   test_koji_stuff_01);
    g_test_add_func("/mergerepo_c/test_koji_stuff_02_get_newest_srpm_from_one_repo",
                   test_koji_stuff_02_get_newest_srpm_from_one_repo);
    g_test_add_func("/mergerepo_c/test_koji_stuff_03_get_srpm_from_first_repo_even_if_its_older",
                   test_koji_stuff_03_get_srpm_from_first_repo_even_if_its_older);

    g_test_add_func("/mergerepo_c/test_koji_allowed_pkg_not_included",
                   test_koji_allowed_pkg_not_included);
    g_test_add_func("/mergerepo_c/test_koji_allowed_pkg_included",
                   test_koji_allowed_pkg_included);
    g_test_add_func("/mergerepo_c/test_koji_allowed_pkg_blocked",
                   test_koji_allowed_pkg_blocked);
    g_test_add_func("/mergerepo_c/test_koji_allowed_pkg_already_seen",
                   test_koji_allowed_pkg_already_seen);

    g_test_add_func("/mergerepo_c/test_koji_allowed_simple_ignores_include",
                   test_koji_allowed_simple_ignores_include);
    g_test_add_func("/mergerepo_c/test_koji_allowed_simple_ignores_seen",
                   test_koji_allowed_simple_ignores_seen);
    g_test_add_func("/mergerepo_c/test_koji_allowed_simple_respects_blocked",
                   test_koji_allowed_simple_respects_blocked);

    return g_test_run();
}
