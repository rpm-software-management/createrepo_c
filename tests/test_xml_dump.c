
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

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/xml_dump/test_cr_prepend_protocol_00",
                    test_cr_prepend_protocol_00);

    g_test_add_func("/xml_dump/test_cr_prepend_protocol_01",
                    test_cr_prepend_protocol_01);
    return g_test_run();
}
