/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sqlite3.h>
#include "createrepo/misc.h"
#include "createrepo/package.h"
#include "createrepo/sqlite.h"
#include "createrepo/parsepkg.h"
#include "createrepo/constants.h"

#define TMP_DIR_PATTERN         "/tmp/createrepo_test_XXXXXX"
#define TMP_PRIMARY_NAME        "primary.sqlite"
#define TMP_FILELISTS_NAME      "filelists.sqlite"
#define TMP_OTHER_NAME          "other.sqlite"

#define EMPTY_PKG               "test_data/packages/empty-0-0.x86_64.rpm"
#define EMPTY_PKG_SRC           "test_data/packages/empty-0-0.src.rpm"


typedef struct {
    gchar *tmp_dir;
} TestData;


Package *
get_package()
{
    Package *p;
    Dependency *dep;
    PackageFile *file;

    p = package_new();
    p->pkgId = "123456";
    p->name = "foo";
    p->arch = "x86_64";
    p->version = "1.2.3";
    p->epoch = "1";
    p->release = "2";
    p->summary = "foo package";
    p->description = "super cool package";
    p->url = "http://package.com";
    p->time_file = 123456;
    p->time_build = 234567;
    p->rpm_license = "GPL";
    p->rpm_vendor = NULL;
    p->rpm_group = NULL;
    p->rpm_buildhost = NULL;
    p->rpm_sourcerpm = "foo.src.rpm";
    p->rpm_header_start = 20;
    p->rpm_header_end = 120;
    p->rpm_packager = NULL;
    p->size_package = 123;
    p->size_installed = 20;
    p->size_archive = 30;
    p->location_href = "foo.rpm";
    p->location_base = NULL;
    p->checksum_type = "sha256";

    dep = dependency_new();
    dep->name = "foobar_dep";
    dep->flags = NULL;
    dep->pre = FALSE;
    p->requires = (g_slist_prepend(p->requires, dep));

    dep = dependency_new();
    dep->name = "foobar_pre_dep";
    dep->flags = "LE";
    dep->pre = TRUE;
    p->requires = g_slist_prepend(p->requires, dep);

    file = package_file_new();
    file->type = "";
    file->path = "/bin/";
    file->name = "foo";
    p->files = g_slist_prepend(p->files, file);

    file = package_file_new();
    file->type = "dir";
    file->path = "/var/foo/";
    file->name = NULL;
    p->files = g_slist_prepend(p->files, file);

    return p;
}



Package *
get_empty_package()
{
    Package *p;
    Dependency *dep;
    PackageFile *file;

    p = package_new();
    p->name = "foo";

    dep = dependency_new();
    dep->name   = NULL;
    dep->flags  = NULL;
    dep->pre    = FALSE;
    p->requires = (g_slist_prepend(p->requires, dep));

    dep = dependency_new();
    dep->name   = NULL;
    dep->flags  = NULL;
    dep->pre    = TRUE;
    p->requires = g_slist_prepend(p->requires, dep);

    file = package_file_new();
    file->type = NULL;
    file->path = NULL;
    file->name = NULL;
    p->files   = g_slist_prepend(p->files, file);

    return p;
}


static void
testdata_setup(TestData *testdata, gconstpointer test_data)
{
    UNUSED(test_data);
    testdata->tmp_dir = g_strdup(TMP_DIR_PATTERN);
    mkdtemp(testdata->tmp_dir);
}


static void
testdata_teardown(TestData *testdata, gconstpointer test_data)
{
    UNUSED(test_data);
    remove_dir(testdata->tmp_dir);
    g_free(testdata->tmp_dir);
}


static void
test_open_db(TestData *testdata, gconstpointer test_data)
{
    UNUSED(test_data);

    GError *err = NULL;
    gchar *path = NULL;
    sqlite3 *db;

    // Create new db

    path = g_strconcat(testdata->tmp_dir, "/", TMP_PRIMARY_NAME, NULL);
    db = open_primary_db(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));
    g_free(path);
    close_primary_db(db, &err);
    g_assert(!err);

    path = g_strconcat(testdata->tmp_dir, "/", TMP_FILELISTS_NAME, NULL);
    db = open_filelists_db(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));
    g_free(path);
    close_filelists_db(db, &err);
    g_assert(!err);

    path = g_strconcat(testdata->tmp_dir, "/", TMP_OTHER_NAME, NULL);
    db = open_other_db(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));
    g_free(path);
    close_other_db(db, &err);
    g_assert(!err);
}


static void
test_add_primary_pkg_db(TestData *testdata, gconstpointer test_data)
{
    UNUSED(test_data);

    GError *err = NULL;
    gchar *path;
    sqlite3 *db;
    Package *pkg;

    GTimer *timer = g_timer_new();
    gdouble topen, tprepare, tadd, tclean, tmp;

    // Create new db

    path = g_strconcat(testdata->tmp_dir, "/", TMP_PRIMARY_NAME, NULL);
    g_timer_start(timer);
    db = open_primary_db(path, &err);
    topen = g_timer_elapsed(timer, NULL);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));

    // Load package

    pkg = get_package();

    // Add package

    DbPrimaryStatements pri_stmts;

    tmp = g_timer_elapsed(timer, NULL);
    pri_stmts = prepare_primary_db_statements(db, &err);
    tprepare = g_timer_elapsed(timer, NULL) - tmp;
    g_assert(!err);

    tmp = g_timer_elapsed(timer, NULL);
    add_primary_pkg_db(pri_stmts, pkg);
    tadd = g_timer_elapsed(timer, NULL) - tmp;

    tmp = g_timer_elapsed(timer, NULL);
    destroy_primary_db_statements(pri_stmts);
    close_primary_db(db, &err);
    tclean = g_timer_elapsed(timer, NULL) - tmp;

    printf("Stats:\nOpen:    %f\nPrepare: %f\nAdd:     %f\nCleanup: %f\nSum:     %f\n",
            topen, tprepare, tadd, tclean, (topen + tprepare + tadd + tclean));

    // Cleanup

    g_timer_stop(timer);
    g_timer_destroy(timer);
    g_free(path);
    g_assert(!err);
}


static void
test_dbinfo_update(TestData *testdata, gconstpointer test_data)
{
    UNUSED(test_data);

    GError *err = NULL;
    gchar *path;
    sqlite3 *db;
    Package *pkg;

    // Create new db

    path = g_strconcat(testdata->tmp_dir, "/", TMP_PRIMARY_NAME, NULL);
    db = open_primary_db(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));

    // Try dbinfo_update

    dbinfo_update(db, "foochecksum", &err);
    g_assert(!err);

    // Load package

    pkg = get_package();

    // Add package

    DbPrimaryStatements pri_stmts;

    pri_stmts = prepare_primary_db_statements(db, &err);
    g_assert(!err);

    add_primary_pkg_db(pri_stmts, pkg);

    destroy_primary_db_statements(pri_stmts);

    // Try dbinfo_update again

    dbinfo_update(db, "foochecksum", &err);
    g_assert(!err);

    // Cleanup

    package_free(pkg);
    g_free(path);
    close_primary_db(db, &err);
    g_assert(!err);
}



static void
test_all(TestData *testdata, gconstpointer test_data)
{
    UNUSED(test_data);

    GError *err = NULL;
    gchar *path;
    sqlite3 *db = NULL;
    Package *pkg, *pkg2 = NULL;

    // Create new db

    path = g_strconcat(testdata->tmp_dir, "/", TMP_PRIMARY_NAME, NULL);
    db = open_primary_db(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));

    // Try dbinfo_update

    dbinfo_update(db, "foochecksum", &err);
    g_assert(!err);

    // Load package

    init_package_parser();
    pkg = package_from_file(EMPTY_PKG, PKG_CHECKSUM_SHA256, EMPTY_PKG, NULL, 5, NULL);
    g_assert(pkg);
    free_package_parser();

    pkg2 = get_empty_package();

    // Add package

    DbPrimaryStatements pri_stmts;

    pri_stmts = prepare_primary_db_statements(db, &err);
    g_assert(!err);

    add_primary_pkg_db(pri_stmts, pkg);
    add_primary_pkg_db(pri_stmts, pkg2);

    destroy_primary_db_statements(pri_stmts);

    // Try dbinfo_update again

    dbinfo_update(db, "foochecksum", &err);
    g_assert(!err);


    // Cleanup

    package_free(pkg);
    package_free(pkg2);
    close_primary_db(db, &err);
    g_assert(!err);
    g_free(path);
}



int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add("/sqlite/test_open_db", TestData, NULL, testdata_setup, test_open_db, testdata_teardown);
    g_test_add("/sqlite/test_add_primary_pkg_db", TestData, NULL, testdata_setup, test_add_primary_pkg_db, testdata_teardown);
    g_test_add("/sqlite/test_dbinfo_update", TestData, NULL, testdata_setup, test_dbinfo_update, testdata_teardown);
    g_test_add("/sqlite/test_all", TestData, NULL, testdata_setup, test_all, testdata_teardown);

    return g_test_run();
}
