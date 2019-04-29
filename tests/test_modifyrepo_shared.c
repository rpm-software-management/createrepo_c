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

#include <glib/gstdio.h>
#include "fixtures.h"
#include "createrepo/misc.h"
#include "createrepo/modifyrepo_shared.h"

static void 
copy_repo_TEST_REPO_00(const gchar *target_path, const gchar *tmp){
    
    g_assert(!g_mkdir_with_parents(target_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH));

    gchar *md = g_strconcat(tmp, "/", TEST_REPO_00_REPOMD, NULL);
    gchar *prim = g_strconcat(tmp, "/", TEST_REPO_00_PRIMARY, NULL);
    gchar *file = g_strconcat(tmp, "/", TEST_REPO_00_FILELISTS, NULL);
    gchar *oth = g_strconcat(tmp, "/", TEST_REPO_00_OTHER, NULL);

    g_assert(cr_copy_file(TEST_REPO_00_REPOMD, md, NULL));
    g_assert(cr_copy_file(TEST_REPO_00_PRIMARY, prim, NULL));
    g_assert(cr_copy_file(TEST_REPO_00_FILELISTS, file, NULL));
    g_assert(cr_copy_file(TEST_REPO_00_OTHER, oth, NULL));

    g_free(md);
    g_free(prim);
    g_free(file);
    g_free(oth);
}

static void 
test_cr_remove_compression_suffix_with_none(void)
{
    GError **err = NULL;
    gchar *out = cr_remove_compression_suffix_if_present(TEST_TEXT_FILE, err);
    g_assert_cmpstr(out, ==, "testdata/test_files/text_file");
    g_free(out);
}

static void 
test_cr_remove_compression_suffix(void)
{
    GError **err = NULL;
    gchar *out = cr_remove_compression_suffix_if_present(TEST_TEXT_FILE_GZ, err);
    g_assert_cmpstr(out, ==, "testdata/test_files/text_file");
    g_free(out);

    out = cr_remove_compression_suffix_if_present(TEST_TEXT_FILE_XZ, err);
    g_assert_cmpstr(out, ==, "testdata/test_files/text_file");
    g_free(out);

    out = cr_remove_compression_suffix_if_present(TEST_SQLITE_FILE, err);
    g_assert_cmpstr(out, ==, "testdata/test_files/sqlite_file.sqlite");
    g_free(out);
}

static void 
test_cr_write_file(void)
{
    char *tmp_dir;
    tmp_dir = g_strdup(TMPDIR_TEMPLATE);
    g_assert(mkdtemp(tmp_dir));

    gchar *repopath = g_strconcat(tmp_dir, "/", TEST_REPO_00, "repodata", NULL);
    copy_repo_TEST_REPO_00(repopath, tmp_dir);

    cr_ModifyRepoTask *task = cr_modifyrepotask_new();

    task->path = TEST_TEXT_FILE;
    task->compress = 1;

    GError **err = NULL;
    cr_write_file(repopath, task, CR_CW_GZ_COMPRESSION, err);
    
    //bz1639287 file should not be named text_file.gz.gz
    gchar *dst = g_strconcat(repopath, "/", "text_file.gz" , NULL);
    g_assert(g_file_test(dst, G_FILE_TEST_EXISTS));

    cr_modifyrepotask_free(task);
    g_free(repopath);
    g_free(tmp_dir);
    g_free(dst);
}

static void 
test_cr_write_file_with_gz_file(void)
{
    char *tmp_dir;
    tmp_dir = g_strdup(TMPDIR_TEMPLATE);
    g_assert(mkdtemp(tmp_dir));

    gchar *repopath = g_strconcat(tmp_dir, "/", TEST_REPO_00, "repodata", NULL);
    copy_repo_TEST_REPO_00(repopath, tmp_dir);

    cr_ModifyRepoTask *task = cr_modifyrepotask_new();

    task->path = TEST_TEXT_FILE_GZ;
    task->compress = 1;

    GError **err = NULL;
    char * out = cr_write_file(repopath, task, CR_CW_GZ_COMPRESSION, err);
    
    //bz1639287 file should not be named text_file.gz.gz
    gchar *dst = g_strconcat(repopath, "/", "text_file.gz" , NULL);
    g_assert_cmpstr(out, ==, dst);
    g_assert(g_file_test(dst, G_FILE_TEST_EXISTS));

    cr_modifyrepotask_free(task);
    g_free(repopath);
    g_free(tmp_dir);
    g_free(dst);
    g_free(out);
}

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/modifyrepo_shared/test_cr_remove_compression_suffix", test_cr_remove_compression_suffix);
    g_test_add_func("/modifyrepo_shared/test_cr_remove_compression_suffix_with_none", test_cr_remove_compression_suffix_with_none);

    g_test_add_func("/modifyrepo_shared/test_cr_write_file", test_cr_write_file);
    g_test_add_func("/modifyrepo_shared/test_cr_write_file_with_gz_file", test_cr_write_file_with_gz_file);

    return g_test_run();
}
