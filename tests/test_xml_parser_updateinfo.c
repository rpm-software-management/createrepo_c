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

#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include "fixtures.h"
#include "createrepo/error.h"
#include "createrepo/misc.h"
#include "createrepo/xml_parser.h"
#include "createrepo/updateinfo.h"

// Tests

static void
test_cr_xml_parse_updateinfo_00(void)
{
    GError *tmp_err = NULL;
    cr_UpdateInfo *ui = cr_updateinfo_new();

    int ret = cr_xml_parse_updateinfo(TEST_UPDATEINFO_00, ui,
                                      NULL, NULL, &tmp_err);
    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);

    cr_updateinfo_free(ui);
}

static void
test_cr_xml_parse_updateinfo_01(void)
{
    GError *tmp_err = NULL;
    cr_UpdateInfo *ui = cr_updateinfo_new();
    cr_UpdateRecord *update;
    cr_UpdateReference *ref;
    cr_UpdateCollection *col;
    cr_UpdateCollectionPackage *pkg;

    int ret = cr_xml_parse_updateinfo(TEST_UPDATEINFO_01, ui,
                                      NULL, NULL, &tmp_err);

    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);

    g_assert_cmpint(g_slist_length(ui->updates), ==, 1);
    update = ui->updates->data;

    g_assert_cmpstr(update->from, ==, "secresponseteam@foo.bar");
    g_assert_cmpstr(update->status, ==, "final");
    g_assert_cmpstr(update->type, ==, "enhancement");
    g_assert_cmpstr(update->version, ==, "3");
    g_assert_cmpstr(update->id, ==, "foobarupdate_1");
    g_assert_cmpstr(update->title, ==, "title_1");
    g_assert_cmpstr(update->issued_date, ==, "2012-12-12 00:00:00");
    g_assert_cmpstr(update->updated_date, ==, "2012-12-12 00:00:00");
    g_assert_cmpstr(update->rights, ==, "rights_1");
    g_assert_cmpstr(update->release, ==, "release_1");
    g_assert_cmpstr(update->pushcount, ==, "pushcount_1");
    g_assert_cmpstr(update->severity, ==, "severity_1");
    g_assert_cmpstr(update->summary, ==, "summary_1");
    g_assert_cmpstr(update->description, ==, "description_1");
    g_assert_cmpstr(update->solution, ==, "solution_1");
    g_assert(update->reboot_suggested);

    g_assert_cmpint(g_slist_length(update->references), ==, 1);
    ref = update->references->data;
    g_assert_cmpstr(ref->href, ==, "https://foobar/foobarupdate_1");
    g_assert_cmpstr(ref->id, ==, "1");
    g_assert_cmpstr(ref->type, ==, "self");
    g_assert_cmpstr(ref->title, ==, "update_1");

    g_assert_cmpint(g_slist_length(update->collections), ==, 1);
    col = update->collections->data;
    g_assert_cmpstr(col->shortname, ==, "foo.component");
    g_assert_cmpstr(col->name, ==, "Foo component");

    g_assert_cmpint(g_slist_length(col->packages), ==, 1);
    pkg = col->packages->data;
    g_assert_cmpstr(pkg->name, ==, "bar");
    g_assert_cmpstr(pkg->version, ==, "2.0.1");
    g_assert_cmpstr(pkg->release, ==, "3");
    g_assert_cmpstr(pkg->epoch, ==, "0");
    g_assert_cmpstr(pkg->arch, ==, "noarch");
    g_assert_cmpstr(pkg->src, ==, "bar-2.0.1-3.src.rpm");
    g_assert_cmpstr(pkg->filename, ==, "bar-2.0.1-3.noarch.rpm");
    g_assert_cmpstr(pkg->sum, ==, "29be985e1f652cd0a29ceed6a1c49964d3618bddd22f0be3292421c8777d26c8");
    g_assert_cmpint(pkg->sum_type, ==, CR_CHECKSUM_SHA256);
    g_assert(pkg->reboot_suggested);
    g_assert(pkg->restart_suggested);
    g_assert(pkg->relogin_suggested);

    cr_updateinfo_free(ui);
}

static void
test_cr_xml_parse_updateinfo_02(void)
{
    GError *tmp_err = NULL;
    cr_UpdateInfo *ui = cr_updateinfo_new();
    cr_UpdateRecord *update;
    cr_UpdateReference *ref;
    cr_UpdateCollection *col;
    cr_UpdateCollectionPackage *pkg;

    int ret = cr_xml_parse_updateinfo(TEST_UPDATEINFO_02, ui,
                                      NULL, NULL, &tmp_err);

    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);

    g_assert_cmpint(g_slist_length(ui->updates), ==, 1);
    update = ui->updates->data;

    g_assert(!update->from);
    g_assert(!update->status);
    g_assert(!update->type);
    g_assert(!update->version);
    g_assert(!update->id);
    g_assert(!update->title);
    g_assert(!update->issued_date);
    g_assert(!update->updated_date);
    g_assert(!update->rights);
    g_assert(!update->release);
    g_assert(!update->pushcount);
    g_assert(!update->severity);
    g_assert(!update->summary);
    g_assert(!update->reboot_suggested);
    g_assert(!update->description);
    g_assert(!update->solution);

    g_assert_cmpint(g_slist_length(update->references), ==, 1);
    ref = update->references->data;
    g_assert(!ref->href);
    g_assert(!ref->id);
    g_assert(!ref->type);
    g_assert(!ref->title);

    g_assert_cmpint(g_slist_length(update->collections), ==, 1);
    col = update->collections->data;
    g_assert(!col->shortname);
    g_assert(!col->name);

    g_assert_cmpint(g_slist_length(col->packages), ==, 1);
    pkg = col->packages->data;
    g_assert(!pkg->name);
    g_assert(!pkg->version);
    g_assert(!pkg->release);
    g_assert(!pkg->epoch);
    g_assert(!pkg->arch);
    g_assert(!pkg->src);
    g_assert(!pkg->filename);
    g_assert(!pkg->sum);
    g_assert_cmpint(pkg->sum_type, ==, CR_CHECKSUM_UNKNOWN);
    g_assert(!pkg->reboot_suggested);
    g_assert(!pkg->restart_suggested);
    g_assert(!pkg->relogin_suggested);

    cr_updateinfo_free(ui);
}

//Test for module support
static void
test_cr_xml_parse_updateinfo_03(void)
{
    GError *tmp_err = NULL;
    cr_UpdateInfo *ui = cr_updateinfo_new();
    cr_UpdateRecord *update;
    cr_UpdateCollection *col;
    cr_UpdateCollectionModule *module;
    cr_UpdateCollectionPackage *pkg;

    int ret = cr_xml_parse_updateinfo(TEST_UPDATEINFO_03, ui,
                                      NULL, NULL, &tmp_err);

    g_assert(tmp_err == NULL);
    g_assert_cmpint(ret, ==, CRE_OK);

    g_assert_cmpint(g_slist_length(ui->updates), ==, 6);

    update = g_slist_nth_data(ui->updates, 2);
    g_assert(!update->reboot_suggested);

    update = g_slist_nth_data(ui->updates, 3);

    g_assert_cmpstr(update->from, ==, "errata@redhat.com");
    g_assert_cmpstr(update->status, ==, "stable");
    g_assert_cmpstr(update->type, ==, "enhancement");
    g_assert_cmpstr(update->version, ==, "1");
    g_assert_cmpstr(update->id, ==, "RHEA-2012:0058");
    g_assert_cmpstr(update->title, ==, "Gorilla_Erratum");
    g_assert_cmpstr(update->description, ==, "Gorilla_Erratum");
    g_assert(update->reboot_suggested);

    update = g_slist_nth_data(ui->updates, 4);

    g_assert_cmpstr(update->id, ==, "RHEA-2012:0059");
    g_assert_cmpstr(update->title, ==, "Duck_Kangaroo_Erratum");
    g_assert_cmpstr(update->description, ==, "Duck_Kangaro_Erratum description");
    g_assert_cmpstr(update->issued_date, ==, "2018-01-27 16:08:09");
    g_assert_cmpstr(update->updated_date, ==, "2018-07-20 06:00:01 UTC");
    g_assert_cmpstr(update->release, ==, "1");
    g_assert(update->reboot_suggested);

    g_assert_cmpint(g_slist_length(update->references), ==, 0);

    g_assert_cmpint(g_slist_length(update->collections), ==, 2);
    col = g_slist_nth_data(update->collections, 0);
    g_assert_cmpstr(col->shortname, ==, "");
    g_assert_cmpstr(col->name, ==, "coll_name1");

    module = col->module;
    g_assert_cmpstr(module->name, ==, "kangaroo");
    g_assert_cmpstr(module->stream, ==, "0");
    g_assert_cmpuint(module->version, ==, 20180730223407);
    g_assert_cmpstr(module->context, ==, "deadbeef");
    g_assert_cmpstr(module->arch, ==, "noarch");

    g_assert_cmpint(g_slist_length(col->packages), ==, 1);
    pkg = col->packages->data;
    g_assert_cmpstr(pkg->name, ==, "kangaroo");
    g_assert_cmpstr(pkg->version, ==, "0.3");
    g_assert_cmpstr(pkg->release, ==, "1");
    g_assert(!pkg->epoch);
    g_assert_cmpstr(pkg->arch, ==, "noarch");
    g_assert_cmpstr(pkg->src, ==, "http://www.fedoraproject.org");
    g_assert_cmpstr(pkg->filename, ==, "kangaroo-0.3-1.noarch.rpm");
    g_assert(!pkg->sum);
    g_assert(!pkg->sum_type);

    col = g_slist_nth_data(update->collections, 1);
    g_assert_cmpstr(col->shortname, ==, "");
    g_assert_cmpstr(col->name, ==, "coll_name2");

    module = col->module;
    g_assert_cmpstr(module->name, ==, "duck");
    g_assert_cmpstr(module->stream, ==, "0");
    g_assert_cmpuint(module->version, ==, 20180730233102);
    g_assert_cmpstr(module->context, ==, "deadbeef");
    g_assert_cmpstr(module->arch, ==, "noarch");

    g_assert_cmpint(g_slist_length(col->packages), ==, 1);
    pkg = col->packages->data;
    g_assert_cmpstr(pkg->name, ==, "duck");
    g_assert_cmpstr(pkg->version, ==, "0.7");
    g_assert_cmpstr(pkg->filename, ==, "duck-0.7-1.noarch.rpm");

    update = g_slist_nth_data(ui->updates, 5);

    g_assert_cmpstr(update->id, ==, "RHEA-2012:0060");
    g_assert_cmpstr(update->issued_date, ==, "1555429284");
    g_assert_cmpstr(update->updated_date, ==, "2018-07-29 06:00:01 UTC");

    cr_updateinfo_free(ui);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/xml_parser_updateinfo/test_cr_xml_parse_updateinfo_00",
                    test_cr_xml_parse_updateinfo_00);
    g_test_add_func("/xml_parser_updateinfo/test_cr_xml_parse_updateinfo_01",
                    test_cr_xml_parse_updateinfo_01);
    g_test_add_func("/xml_parser_updateinfo/test_cr_xml_parse_updateinfo_02",
                    test_cr_xml_parse_updateinfo_02);
    g_test_add_func("/xml_parser_updateinfo/test_cr_xml_parse_updateinfo_03",
                    test_cr_xml_parse_updateinfo_03);

    return g_test_run();
}
