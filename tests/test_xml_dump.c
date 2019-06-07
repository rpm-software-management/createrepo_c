
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include "fixtures.h"
#include "createrepo/error.h"
#include "createrepo/package.h"
#include "createrepo/misc.h"
#include "createrepo/xml_dump.h"

// Tests

static void
test_cr_prepend_protocol_00(void)
{
    const gchar *url_to_be_prepended = "/path/to/package.noarch.rpm";
    gchar *prepended_url = cr_prepend_protocol(url_to_be_prepended);
    g_assert_cmpstr(prepended_url, ==, "file:///path/to/package.noarch.rpm");
    g_free(prepended_url);
}

static void
test_cr_prepend_protocol_01(void)
{
    const gchar *url_to_be_prepended = "http://url/to/package.noarch.rpm";
    gchar *prepended_url = cr_prepend_protocol(url_to_be_prepended);
    g_assert_cmpstr(prepended_url, ==, "http://url/to/package.noarch.rpm");
    g_free(prepended_url);
}

static void
test_cr_Package_contains_forbidden_control_chars_01(void)
{
    cr_Package *p = get_package();
    g_assert(!cr_Package_contains_forbidden_control_chars(p));
}

static void
test_cr_Package_contains_forbidden_control_chars_02(void)
{
    cr_Package *p = get_package();
    p->name = "foo";

    g_assert(cr_Package_contains_forbidden_control_chars(p));
}

static void
test_cr_Package_contains_forbidden_control_chars_03(void)
{
    cr_Package *p = get_package();
    p->summary = "foo";

    g_assert(cr_Package_contains_forbidden_control_chars(p));
}

static void
test_cr_Package_contains_forbidden_control_chars_04(void)
{
    cr_Package *p = get_package();
    cr_Dependency *dep = p->requires->data;
    dep->name = "foobar_dep";

    g_assert(cr_Package_contains_forbidden_control_chars(p));
}

static void
test_cr_Package_contains_forbidden_control_chars_05(void)
{
    cr_Package *p = get_package();
    cr_PackageFile *file = p->files->data;
    file->name = "obar_dep";

    g_assert(cr_Package_contains_forbidden_control_chars(p));
}

static void
test_cr_GSList_of_cr_Dependency_contains_forbidden_control_chars_01(void)
{
    cr_Package *p = get_package();
    cr_Dependency *dep = p->requires->data;
    dep->name = "foobar_dep";

    g_assert(cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(p->requires));
}

static void
test_cr_GSList_of_cr_Dependency_contains_forbidden_control_chars_02(void)
{
    cr_Package *p = get_package();
    cr_Dependency *dep = p->requires->data;
    dep->name = "fo	badep";

    g_assert(!cr_GSList_of_cr_Dependency_contains_forbidden_control_chars(p->requires));
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/xml_dump/test_cr_prepend_protocol_00",
                    test_cr_prepend_protocol_00);
    g_test_add_func("/xml_dump/test_cr_prepend_protocol_01",
                    test_cr_prepend_protocol_01);

    g_test_add_func("/xml_dump/test_cr_Package_contains_forbidden_control_chars_01",
                    test_cr_Package_contains_forbidden_control_chars_01);
    g_test_add_func("/xml_dump/test_cr_Package_contains_forbidden_control_chars_02",
                    test_cr_Package_contains_forbidden_control_chars_02);
    g_test_add_func("/xml_dump/test_cr_Package_contains_forbidden_control_chars_03",
                    test_cr_Package_contains_forbidden_control_chars_03);
    g_test_add_func("/xml_dump/test_cr_Package_contains_forbidden_control_chars_04",
                    test_cr_Package_contains_forbidden_control_chars_04);
    g_test_add_func("/xml_dump/test_cr_Package_contains_forbidden_control_chars_05",
                    test_cr_Package_contains_forbidden_control_chars_05);
    g_test_add_func("/xml_dump/test_cr_GSList_of_cr_Dependency_contains_forbidden_control_chars_01",
                    test_cr_GSList_of_cr_Dependency_contains_forbidden_control_chars_01);
    g_test_add_func("/xml_dump/test_cr_GSList_of_cr_Dependency_contains_forbidden_control_chars_02",
                    test_cr_GSList_of_cr_Dependency_contains_forbidden_control_chars_02);
    return g_test_run();
}
