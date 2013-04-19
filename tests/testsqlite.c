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


cr_Package *
get_package()
{
    cr_Package *p;
    cr_Dependency *dep;
    cr_PackageFile *file;

    p = cr_package_new();
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

    dep = cr_dependency_new();
    dep->name = "foobar_dep";
    dep->flags = NULL;
    dep->pre = FALSE;
    p->requires = (g_slist_prepend(p->requires, dep));

    dep = cr_dependency_new();
    dep->name = "foobar_pre_dep";
    dep->flags = "LE";
    dep->pre = TRUE;
    p->requires = g_slist_prepend(p->requires, dep);

    file = cr_package_file_new();
    file->type = "";
    file->path = "/bin/";
    file->name = "foo";
    p->files = g_slist_prepend(p->files, file);

    file = cr_package_file_new();
    file->type = "dir";
    file->path = "/var/foo/";
    file->name = NULL;
    p->files = g_slist_prepend(p->files, file);

    return p;
}



cr_Package *
get_empty_package()
{
    cr_Package *p;
    cr_Dependency *dep;
    cr_PackageFile *file;

    p = cr_package_new();
    p->name = "foo";

    dep = cr_dependency_new();
    dep->name   = NULL;
    dep->flags  = NULL;
    dep->pre    = FALSE;
    p->requires = (g_slist_prepend(p->requires, dep));

    dep = cr_dependency_new();
    dep->name   = NULL;
    dep->flags  = NULL;
    dep->pre    = TRUE;
    p->requires = g_slist_prepend(p->requires, dep);

    file = cr_package_file_new();
    file->type = NULL;
    file->path = NULL;
    file->name = NULL;
    p->files   = g_slist_prepend(p->files, file);

    return p;
}


static void
testdata_setup(TestData *testdata, gconstpointer test_data)
{
    CR_UNUSED(test_data);
    testdata->tmp_dir = g_strdup(TMP_DIR_PATTERN);
    mkdtemp(testdata->tmp_dir);
}


static void
testdata_teardown(TestData *testdata, gconstpointer test_data)
{
    CR_UNUSED(test_data);
    cr_remove_dir(testdata->tmp_dir);
    g_free(testdata->tmp_dir);
}


static void
test_cr_open_db(TestData *testdata, gconstpointer test_data)
{
    CR_UNUSED(test_data);

    GError *err = NULL;
    gchar *path = NULL;
    sqlite3 *db;

    // Create new db

    path = g_strconcat(testdata->tmp_dir, "/", TMP_PRIMARY_NAME, NULL);
    db = cr_db_open_primary(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));
    g_free(path);
    cr_db_close_primary(db, &err);
    g_assert(!err);

    path = g_strconcat(testdata->tmp_dir, "/", TMP_FILELISTS_NAME, NULL);
    db = cr_db_open_filelists(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));
    g_free(path);
    cr_db_close_filelists(db, &err);
    g_assert(!err);

    path = g_strconcat(testdata->tmp_dir, "/", TMP_OTHER_NAME, NULL);
    db = cr_db_open_other(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));
    g_free(path);
    cr_db_close_other(db, &err);
    g_assert(!err);
}


static void
test_cr_db_add_primary_pkg(TestData *testdata, gconstpointer test_data)
{
    CR_UNUSED(test_data);

    GError *err = NULL;
    gchar *path;
    sqlite3 *db;
    cr_Package *pkg;

    GTimer *timer = g_timer_new();
    gdouble topen, tprepare, tadd, tclean, tmp;

    // Create new db

    path = g_strconcat(testdata->tmp_dir, "/", TMP_PRIMARY_NAME, NULL);
    g_timer_start(timer);
    db = cr_db_open_primary(path, &err);
    topen = g_timer_elapsed(timer, NULL);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));

    // Load package

    pkg = get_package();

    // Add package

    cr_DbPrimaryStatements pri_stmts;

    tmp = g_timer_elapsed(timer, NULL);
    pri_stmts = cr_db_prepare_primary_statements(db, &err);
    tprepare = g_timer_elapsed(timer, NULL) - tmp;
    g_assert(!err);

    tmp = g_timer_elapsed(timer, NULL);
    cr_db_add_primary_pkg(pri_stmts, pkg, &err);
    tadd = g_timer_elapsed(timer, NULL) - tmp;
    g_assert(!err);

    tmp = g_timer_elapsed(timer, NULL);
    cr_db_destroy_primary_statements(pri_stmts);
    cr_db_close_primary(db, &err);
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
test_cr_db_dbinfo_update(TestData *testdata, gconstpointer test_data)
{
    CR_UNUSED(test_data);

    GError *err = NULL;
    gchar *path;
    sqlite3 *db;
    cr_Package *pkg;

    // Create new db

    path = g_strconcat(testdata->tmp_dir, "/", TMP_PRIMARY_NAME, NULL);
    db = cr_db_open_primary(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));

    // Try cr_db_dbinfo_update

    cr_db_dbinfo_update(db, "foochecksum", &err);
    g_assert(!err);

    // Load package

    pkg = get_package();

    // Add package

    cr_DbPrimaryStatements pri_stmts;

    pri_stmts = cr_db_prepare_primary_statements(db, &err);
    g_assert(!err);

    cr_db_add_primary_pkg(pri_stmts, pkg, &err);
    g_assert(!err);

    cr_db_destroy_primary_statements(pri_stmts);

    // Try cr_db_dbinfo_update again

    cr_db_dbinfo_update(db, "foochecksum", &err);
    g_assert(!err);

    // Cleanup

    cr_package_free(pkg);
    g_free(path);
    cr_db_close_primary(db, &err);
    g_assert(!err);
}



static void
test_all(TestData *testdata, gconstpointer test_data)
{
    CR_UNUSED(test_data);

    GError *err = NULL;
    gchar *path;
    sqlite3 *db = NULL;
    cr_Package *pkg, *pkg2 = NULL;

    // Create new db

    path = g_strconcat(testdata->tmp_dir, "/", TMP_PRIMARY_NAME, NULL);
    db = cr_db_open_primary(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));

    // Try cr_db_dbinfo_update

    cr_db_dbinfo_update(db, "foochecksum", &err);
    g_assert(!err);

    // Load package

    cr_package_parser_init();
    pkg = cr_package_from_rpm(EMPTY_PKG, CR_CHECKSUM_SHA256, EMPTY_PKG, NULL, 5, NULL);
    g_assert(pkg);
    cr_package_parser_cleanup();

    pkg2 = get_empty_package();

    // Add package

    cr_DbPrimaryStatements pri_stmts;

    pri_stmts = cr_db_prepare_primary_statements(db, &err);
    g_assert(!err);

    cr_db_add_primary_pkg(pri_stmts, pkg, &err);
    g_assert(!err);
    cr_db_add_primary_pkg(pri_stmts, pkg2, &err);
    g_assert(!err);

    cr_db_destroy_primary_statements(pri_stmts);

    // Try cr_db_dbinfo_update again

    cr_db_dbinfo_update(db, "foochecksum", &err);
    g_assert(!err);


    // Cleanup

    cr_package_free(pkg);
    cr_package_free(pkg2);
    cr_db_close_primary(db, &err);
    g_assert(!err);
    g_free(path);
}



int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add("/sqlite/test_cr_open_db", TestData, NULL, testdata_setup, test_cr_open_db, testdata_teardown);
    g_test_add("/sqlite/test_cr_db_add_primary_pkg", TestData, NULL, testdata_setup, test_cr_db_add_primary_pkg, testdata_teardown);
    g_test_add("/sqlite/test_cr_db_dbinfo_update", TestData, NULL, testdata_setup, test_cr_db_dbinfo_update, testdata_teardown);
    g_test_add("/sqlite/test_all", TestData, NULL, testdata_setup, test_all, testdata_teardown);

    return g_test_run();
}
