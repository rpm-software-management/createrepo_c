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
#include "fixtures.h"
#include "createrepo/misc.h"
#include "createrepo/package.h"
#include "createrepo/sqlite.h"
#include "createrepo/parsepkg.h"
#include "createrepo/constants.h"

#define TMP_DIR_PATTERN         "/tmp/createrepo_test_XXXXXX"
#define TMP_PRIMARY_NAME        "primary.sqlite"
#define TMP_FILELISTS_NAME      "filelists.sqlite"
#define TMP_OTHER_NAME          "other.sqlite"

#define EMPTY_PKG               TEST_PACKAGES_PATH"empty-0-0.x86_64.rpm"
#define EMPTY_PKG_SRC           TEST_PACKAGES_PATH"empty-0-0.src.rpm"


typedef struct {
    gchar *tmp_dir;
} TestData;


static void
testdata_setup(TestData *testdata,
               G_GNUC_UNUSED gconstpointer test_data)
{
    testdata->tmp_dir = g_strdup(TMP_DIR_PATTERN);
    mkdtemp(testdata->tmp_dir);
}


static void
testdata_teardown(TestData *testdata,
                  G_GNUC_UNUSED gconstpointer test_data)
{
    cr_remove_dir(testdata->tmp_dir, NULL);
    g_free(testdata->tmp_dir);
}


static void
test_cr_open_db(TestData *testdata,
                G_GNUC_UNUSED gconstpointer test_data)
{
    GError *err = NULL;
    gchar *path = NULL;
    cr_SqliteDb *db;

    // Create new db

    path = g_strconcat(testdata->tmp_dir, "/", TMP_PRIMARY_NAME, NULL);
    db = cr_db_open_primary(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));
    g_free(path);
    cr_db_close(db, &err);
    g_assert(!err);

    path = g_strconcat(testdata->tmp_dir, "/", TMP_FILELISTS_NAME, NULL);
    db = cr_db_open_filelists(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));
    g_free(path);
    cr_db_close(db, &err);
    g_assert(!err);

    path = g_strconcat(testdata->tmp_dir, "/", TMP_OTHER_NAME, NULL);
    db = cr_db_open_other(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));
    g_free(path);
    cr_db_close(db, &err);
    g_assert(!err);
}


static void
test_cr_db_add_primary_pkg(TestData *testdata,
                           G_GNUC_UNUSED gconstpointer test_data)
{
    GError *err = NULL;
    gchar *path;
    cr_SqliteDb *db;
    cr_Package *pkg;

    GTimer *timer = g_timer_new();

    // Create new db

    path = g_strconcat(testdata->tmp_dir, "/", TMP_PRIMARY_NAME, NULL);

    db = cr_db_open_primary(path, &err);
    g_assert(db);
    g_assert(!err);
    g_assert(g_file_test(path, G_FILE_TEST_EXISTS));

    // Load package

    pkg = get_package();

    // Add package

    cr_db_add_pkg(db, pkg, &err);
    g_assert(!err);

    cr_db_close(db, &err);

    // Cleanup

    g_timer_stop(timer);
    g_timer_destroy(timer);
    g_free(path);
    g_assert(!err);
}


static void
test_cr_db_dbinfo_update(TestData *testdata,
                         G_GNUC_UNUSED gconstpointer test_data)
{
    GError *err = NULL;
    gchar *path;
    cr_SqliteDb *db;
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

    cr_db_add_pkg(db, pkg, &err);
    g_assert(!err);

    // Try cr_db_dbinfo_update again

    cr_db_dbinfo_update(db, "foochecksum", &err);
    g_assert(!err);

    // Cleanup

    cr_package_free(pkg);
    g_free(path);
    cr_db_close(db, &err);
    g_assert(!err);
}



static void
test_all(TestData *testdata,
         G_GNUC_UNUSED gconstpointer test_data)
{
    GError *err = NULL;
    gchar *path;
    cr_SqliteDb *db = NULL;
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
    pkg = cr_package_from_rpm(EMPTY_PKG, CR_CHECKSUM_SHA256, EMPTY_PKG, NULL,
                              5, NULL, CR_HDRR_NONE, NULL);
    g_assert(pkg);
    cr_package_parser_cleanup();

    pkg2 = get_empty_package();

    // Add package

    cr_db_add_pkg(db, pkg, &err);
    g_assert(!err);
    cr_db_add_pkg(db, pkg2, &err);
    g_assert(!err);

    // Try cr_db_dbinfo_update again

    cr_db_dbinfo_update(db, "foochecksum", &err);
    g_assert(!err);


    // Cleanup

    cr_package_free(pkg);
    cr_package_free(pkg2);
    cr_db_close(db, &err);
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
