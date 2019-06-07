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
#include "createrepo/locate_metadata.h"


static void test_cr_cmp_metadatum_type(void)
{
    //compare equal with not allocated strings
    cr_Metadatum *m = g_malloc0(sizeof(cr_Metadatum));
    m->name = "/some/name/somewhere";
    m->type = "type";
    int out = cr_cmp_metadatum_type(m, "type");
    g_assert_cmpint(out, ==, 0);

    //compare equal with allocated strings
    m->name = g_strdup_printf("group");
    m->type = g_strdup_printf("group");
    gchar *type = g_strdup_printf("group");
    out = cr_cmp_metadatum_type(m, type);
    g_assert_cmpint(out, ==, 0);
    cr_metadatum_free(m);
    g_free(type);

    //compare bigger with allocated strings
    m = g_malloc0(sizeof(cr_Metadatum));
    m->name = g_strdup_printf("name");
    m->type = g_strdup_printf("group");
    type = g_strdup_printf("grou");
    out = cr_cmp_metadatum_type(m, type);
    g_assert_cmpint(out, >, 0);
    cr_metadatum_free(m);
    g_free(type);

    //compare smaller with allocated strings
    m = g_malloc0(sizeof(cr_Metadatum));
    m->name = g_strdup_printf("name");
    m->type = g_strdup_printf("group");
    type = g_strdup_printf("groupppppp");
    out = cr_cmp_metadatum_type(m, type);
    g_assert_cmpint(out, <, 0);
    cr_metadatum_free(m);
    g_free(type);
}

static void test_cr_cmp_repomd_record_type(void)
{
    cr_RepomdRecord *r; 
    int out;
    gchar *type;

    //compare equal with not allocated strings
    r = g_malloc0(sizeof(cr_RepomdRecord));
    r->location_real = "/some/name/somewhere";
    r->type = "type";
    out = cr_cmp_repomd_record_type(r, "type");
    g_assert_cmpint(out, ==, 0);
    g_free(r);

    //compare equal with allocated strings
    r = cr_repomd_record_new("group", "/some/path/somewhere");
    type = g_strdup_printf("group");
    out = cr_cmp_repomd_record_type(r, type);
    g_assert_cmpint(out, ==, 0);
    cr_repomd_record_free(r);
    g_free(type);

    //compare bigger with allocated strings
    r = cr_repomd_record_new("group", "/some/path/somewhere");
    type = g_strdup_printf("grou");
    out = cr_cmp_repomd_record_type(r, type);
    g_assert_cmpint(out, >, 0);
    cr_repomd_record_free(r);
    g_free(type);

    //compare smaller with allocated strings
    r = cr_repomd_record_new("group", "/some/path/somewhere");
    type = g_strdup_printf("groupppppp");
    out = cr_cmp_metadatum_type(r, type);
    g_assert_cmpint(out, <, 0);
    cr_repomd_record_free(r);
    g_free(type);
}


static void test_cr_copy_metadatum(void)
{
    //empty tmp_repo path
    char *tmp_dir, *tmp_repo, *out, *new_name;
    tmp_dir = g_strdup(TMPDIR_TEMPLATE);
    g_assert(mkdtemp(tmp_dir));
    tmp_repo = g_strconcat(tmp_dir, "/", NULL);
    g_assert_cmpint(g_mkdir_with_parents(tmp_repo, 0777), ==, 0);

    GError *err = NULL;
    new_name = cr_copy_metadatum(TEST_REPO_00_PRIMARY, tmp_repo, &err);
    out = g_strconcat(tmp_repo, "1cb61ea996355add02b1426ed4c1780ea75ce0c04c5d1107c025c3fbd7d8bcae-primary.xml.gz", NULL);
    g_assert_cmpstr(new_name, ==, out);
    g_assert_true(g_file_test(new_name, G_FILE_TEST_EXISTS));
    cr_remove_dir(tmp_repo, NULL);

    g_free(new_name);
    g_free(out);
    g_free(tmp_repo);

    //tmp_repo is a folder
    tmp_repo = g_strconcat(tmp_dir, "/folder/", NULL);
    err = NULL;
    g_assert_cmpint(g_mkdir_with_parents(tmp_repo, 0777), ==, 0);
    g_assert_true(g_file_test(tmp_repo, G_FILE_TEST_EXISTS));
    new_name = cr_copy_metadatum(TEST_REPO_00_PRIMARY, tmp_repo, &err);
    out = g_strconcat(tmp_repo, "1cb61ea996355add02b1426ed4c1780ea75ce0c04c5d1107c025c3fbd7d8bcae-primary.xml.gz", NULL);
    g_assert_cmpstr(new_name, ==, out);
    g_assert_true(g_file_test(new_name, G_FILE_TEST_EXISTS));
    cr_remove_dir(tmp_repo, NULL);

    g_free(new_name);
    g_free(out);
    g_free(tmp_dir);
    g_free(tmp_repo);
}

static void test_cr_insert_additional_metadatum(void)
{
    //add to not allocated GSList
    GSList *d = NULL;
    cr_Metadatum *m;

    d = cr_insert_additional_metadatum("./test_path.xml", "group", d);
    g_assert_true(d);
    g_assert_cmpstr(((cr_Metadatum *) d->data)->type, ==, "group");
    g_assert_cmpstr(((cr_Metadatum *) d->data)->name, ==, "./test_path.xml");
    g_slist_free_full(d, (GDestroyNotify) cr_metadatum_free);
    d = NULL;

    //replace one in list of one
    m = g_malloc0(sizeof(cr_Metadatum));
    m->name = g_strdup_printf("name");
    m->type = g_strdup_printf("group");
    d = g_slist_prepend(d, m);
    g_assert_cmpstr(((cr_Metadatum *) d->data)->type, ==, "group");
    g_assert_cmpstr(((cr_Metadatum *) d->data)->name, ==, "name");
    d = cr_insert_additional_metadatum("./test_path.xml", "group", d);
    g_assert_true(d);
    g_assert_cmpstr(((cr_Metadatum *) d->data)->type, ==, "group");
    g_assert_cmpstr(((cr_Metadatum *) d->data)->name, ==, "./test_path.xml");
    g_assert_cmpint(g_slist_length(d), ==, 1);
    g_slist_free_full(d, (GDestroyNotify) cr_metadatum_free);
    d = NULL;

    //add new one to list of one
    m = g_malloc0(sizeof(cr_Metadatum));
    m->name = g_strdup_printf("name");
    m->type = g_strdup_printf("primary");
    d = g_slist_prepend(d, m);
    d = cr_insert_additional_metadatum("./test_path.xml", "group", d);
    g_assert_true(d);
    g_assert_cmpstr(((cr_Metadatum *) d->data)->type, ==, "group");
    g_assert_cmpstr(((cr_Metadatum *) d->data)->name, ==, "./test_path.xml");
    g_assert_cmpint(g_slist_length(d), ==, 2);
    m = g_slist_nth_data(d, 1);
    g_assert_cmpstr(m->type, ==, "primary");
    g_assert_cmpstr(m->name, ==, "name");
    g_slist_free_full(d, (GDestroyNotify) cr_metadatum_free);
    d = NULL;
}

static void test_cr_parse_repomd(void)
{
    struct cr_MetadataLocation *ret = NULL;
    ret = cr_parse_repomd(TEST_REPO_00_REPOMD, TEST_REPO_00, 1);
    g_assert_cmpint(0, ==, g_slist_length(ret->additional_metadata));
    g_assert_cmpstr(TEST_REPO_00_REPOMD, ==, ret->repomd);
    g_assert_cmpstr(TEST_REPO_00, ==, ret->local_path);
    g_assert_cmpint(0, ==, ret->tmp);
    g_assert_cmpstr(TEST_REPO_00_PRIMARY, ==, ret->pri_xml_href);
    g_assert_cmpstr(TEST_REPO_00_OTHER, ==, ret->oth_xml_href);
    g_assert_cmpstr(TEST_REPO_00_FILELISTS, ==, ret->fil_xml_href);
}

static void test_cr_parse_repomd_with_additional_metadata(void)
{
    struct cr_MetadataLocation *ret = NULL;
    ret = cr_parse_repomd(TEST_REPO_WITH_ADDITIONAL_METADATA_REPOMD, TEST_REPO_WITH_ADDITIONAL_METADATA, 0);
    g_assert_cmpint(8, ==, g_slist_length(ret->additional_metadata));
    g_assert_cmpstr(TEST_REPO_WITH_ADDITIONAL_METADATA_REPOMD, ==, ret->repomd);
    g_assert_cmpstr(TEST_REPO_WITH_ADDITIONAL_METADATA, ==, ret->local_path);
    g_assert_cmpint(0, ==, ret->tmp);

    g_assert_cmpstr(TEST_REPO_WITH_ADDITIONAL_METADATA_PRIMARY_XML_GZ, ==, ret->pri_xml_href);
    g_assert_cmpstr(TEST_REPO_WITH_ADDITIONAL_METADATA_OTHER_XML_GZ, ==, ret->oth_xml_href);
    g_assert_cmpstr(TEST_REPO_WITH_ADDITIONAL_METADATA_FILELISTS_XML_GZ, ==, ret->fil_xml_href);

    g_assert_cmpstr(TEST_REPO_WITH_ADDITIONAL_METADATA_PRIMARY_SQLITE_BZ2, ==, ret->pri_sqlite_href);
    g_assert_cmpstr(TEST_REPO_WITH_ADDITIONAL_METADATA_OTHER_SQLITE_BZ2, ==, ret->oth_sqlite_href);
    g_assert_cmpstr(TEST_REPO_WITH_ADDITIONAL_METADATA_FILELISTS_SQLITE_BZ2, ==, ret->fil_sqlite_href);

    cr_Metadatum *metadatum = g_slist_find_custom(ret->additional_metadata, "group", cr_cmp_metadatum_type)->data;
    g_assert(metadatum);
    metadatum = g_slist_find_custom(ret->additional_metadata, "group_zck", cr_cmp_metadatum_type)->data;
    g_assert(metadatum);
    metadatum = g_slist_find_custom(ret->additional_metadata, "group_gz", cr_cmp_metadatum_type)->data;
    g_assert(metadatum);
    metadatum = g_slist_find_custom(ret->additional_metadata, "group_gz_zck", cr_cmp_metadatum_type)->data;
    g_assert(metadatum);
    metadatum = g_slist_find_custom(ret->additional_metadata, "modules", cr_cmp_metadatum_type)->data;
    g_assert(metadatum);
    metadatum = g_slist_find_custom(ret->additional_metadata, "modules_zck", cr_cmp_metadatum_type)->data;
    g_assert(metadatum);
    metadatum = g_slist_find_custom(ret->additional_metadata, "updateinfo", cr_cmp_metadatum_type)->data;
    g_assert(metadatum);
    metadatum = g_slist_find_custom(ret->additional_metadata, "updateinfo_zck", cr_cmp_metadatum_type)->data;
    g_assert(metadatum);

    g_slist_free_full(ret->additional_metadata, (GDestroyNotify) cr_metadatum_free);
}

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/locate_metadata/test_cr_cmp_metadatum_type", test_cr_cmp_metadatum_type);
    g_test_add_func("/locate_metadata/test_cr_cmp_repomd_record_type", test_cr_cmp_repomd_record_type);
    g_test_add_func("/locate_metadata/test_cr_copy_metadatum", test_cr_copy_metadatum);
    g_test_add_func("/locate_metadata/test_cr_insert_additional_metadatum", test_cr_insert_additional_metadatum);

    g_test_add_func("/locate_metadata/test_cr_parse_repomd", test_cr_parse_repomd);
    g_test_add_func("/locate_metadata/test_cr_parse_repomd_with_additional_metadata", test_cr_parse_repomd_with_additional_metadata);

    return g_test_run();
}
