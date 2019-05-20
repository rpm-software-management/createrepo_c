/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2013  Tomas Mlcoch
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

#define _XOPEN_SOURCE 700

#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "fixtures.h"
#include "createrepo/misc.h"
#include "createrepo/xml_file.h"
#include "createrepo/compression_wrapper.h"

typedef struct {
    gchar *tmpdir;
} TestFixtures;


static void
fixtures_setup(TestFixtures *fixtures,
               G_GNUC_UNUSED gconstpointer test_data)
{
    gchar *template = g_strdup(TMPDIR_TEMPLATE);
    fixtures->tmpdir = mkdtemp(template);
    g_assert(fixtures->tmpdir);
}


static void
fixtures_teardown(TestFixtures *fixtures,
                  G_GNUC_UNUSED gconstpointer test_data)
{
    if (!fixtures->tmpdir)
        return;

    cr_remove_dir(fixtures->tmpdir, NULL);
    g_free(fixtures->tmpdir);
}


static void
test_no_packages(TestFixtures *fixtures,
                 G_GNUC_UNUSED gconstpointer test_data)
{
    cr_XmlFile *f;
    gchar *path;
    gchar contents[2048];
    int ret;
    GError *err = NULL;

    g_assert(g_file_test(fixtures->tmpdir, G_FILE_TEST_IS_DIR));

    // Try primary.xml

    path = g_build_filename(fixtures->tmpdir, "primary.xml.gz", NULL);
    f = cr_xmlfile_open_primary(path, CR_CW_GZ_COMPRESSION, &err);
    g_assert(f);
    g_assert(err == NULL);
    cr_xmlfile_close(f, &err);

    CR_FILE *crf = cr_open(path,
                           CR_CW_MODE_READ,
                           CR_CW_AUTO_DETECT_COMPRESSION,
                           NULL);
    g_assert(crf);
    ret = cr_read(crf, &contents, 2047, NULL);
    g_assert(ret != CR_CW_ERR);
    contents[ret] = '\0';
    cr_close(crf, NULL);
    g_assert_cmpstr(contents, ==, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<metadata xmlns=\"http://linux.duke.edu/metadata/common\" "
            "xmlns:rpm=\"http://linux.duke.edu/metadata/rpm\" "
            "packages=\"0\">\n</metadata>");

    g_free(path);
}

static void
test_rewrite_header_pacakge_count(TestFixtures *fixtures,
                           G_GNUC_UNUSED gconstpointer test_data)
{

    cr_XmlFile *f;
    gchar *path;
    gchar contents[2048];
    int ret;
    GError *err = NULL;

    g_assert(g_file_test(fixtures->tmpdir, G_FILE_TEST_IS_DIR));

    // Try primary.xml

    path = g_build_filename(fixtures->tmpdir, "primary.xml.gz", NULL);
    f = cr_xmlfile_open_primary(path, CR_CW_GZ_COMPRESSION, &err);
    g_assert(f);
    g_assert(err == NULL);
    cr_xmlfile_close(f, &err);

    cr_ContentStat *stat;
    stat = cr_contentstat_new(CR_CHECKSUM_SHA256, &err);
    cr_rewrite_header_package_count(path, CR_CW_GZ_COMPRESSION, 9, 0,
                                    stat, NULL, &err);
    g_assert(!err);
    g_assert_cmpint(stat->size, >=, 100);
    cr_contentstat_free(stat, &err);

    CR_FILE *crf = cr_open(path,
                           CR_CW_MODE_READ,
                           CR_CW_AUTO_DETECT_COMPRESSION,
                           NULL);
    g_assert(crf);
    ret = cr_read(crf, &contents, 2047, NULL);
    g_assert(ret != CR_CW_ERR);
    contents[ret] = '\0';
    cr_close(crf, NULL);
    g_assert_cmpstr(contents, ==, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<metadata xmlns=\"http://linux.duke.edu/metadata/common\" "
            "xmlns:rpm=\"http://linux.duke.edu/metadata/rpm\" "
            "packages=\"9\">\n</metadata>");

    g_free(path);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add("/xml_file/test_no_packages", TestFixtures, NULL, fixtures_setup, test_no_packages, fixtures_teardown);
    g_test_add("/xml_file/test_write_modified_header", TestFixtures, NULL,
            fixtures_setup, test_rewrite_header_pacakge_count, fixtures_teardown);

    return g_test_run();
}
