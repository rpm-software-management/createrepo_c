
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include "fixtures.h"
#include "createrepo/error.h"
#include "createrepo/package.h"
#include "createrepo/misc.h"
#include "createrepo/xml_dump.h"

// Allow g_warning() to not abort during tests that intentionally trigger warnings
static gboolean
log_not_fatal(const gchar *log_domain G_GNUC_UNUSED,
              GLogLevelFlags log_level G_GNUC_UNUSED,
              const gchar *message G_GNUC_UNUSED,
              gpointer user_data G_GNUC_UNUSED)
{
    return FALSE;
}

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

// Test: xml dump succeeds for package with control chars (previously it would fail)
static void
test_cr_xml_dump_with_control_chars(void)
{
    g_test_log_set_fatal_handler(log_not_fatal, NULL);
    cr_Package *p = get_package();

    // Inject a control char into a changelog entry via the string chunk
    cr_ChangelogEntry *changelog = cr_changelog_entry_new();
    changelog->author = cr_safe_string_chunk_insert(p->chunk, "John Doe <john@example.com>");
    changelog->date = 1234567890;
    changelog->changelog = cr_safe_string_chunk_insert(p->chunk,
        "- added patch \x1b""9cd568066b5ff8d6");
    p->changelogs = g_slist_prepend(p->changelogs, changelog);

    // The control char should have been stripped at insert time
    g_assert_cmpstr(changelog->changelog, ==, "- added patch 9cd568066b5ff8d6");

    // XML dump should succeed (not return an error)
    GError *err = NULL;
    struct cr_XmlStruct xml = cr_xml_dump(p, &err);
    g_assert_no_error(err);
    g_assert_nonnull(xml.primary);
    g_assert_nonnull(xml.filelists);
    g_assert_nonnull(xml.other);

    g_free(xml.primary);
    g_free(xml.filelists);
    g_free(xml.filelists_ext);
    g_free(xml.other);
    cr_package_free(p);
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/xml_dump/test_cr_prepend_protocol_00",
                    test_cr_prepend_protocol_00);
    g_test_add_func("/xml_dump/test_cr_prepend_protocol_01",
                    test_cr_prepend_protocol_01);
    g_test_add_func("/xml_dump/test_cr_xml_dump_with_control_chars",
                    test_cr_xml_dump_with_control_chars);

    return g_test_run();
}
