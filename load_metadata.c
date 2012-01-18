#include <glib.h>
#include <stdio.h>
#include "load_metadata.h"


void free_values(gpointer data)
{
    struct package_metadata *md = (struct package_metadata *) data;
    g_free(md->checksum_type);
    g_free(md->primary_xml);
    g_free(md->filelists_xml);
    g_free(md->other_xml);
    g_free(md);
}


GHashTable *load_metadata(const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path)
{
    GFileTest flags = G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR;
    if (!g_file_test(primary_xml_path, flags) ||
        !g_file_test(filelists_xml_path, flags) ||
        !g_file_test(other_xml_path, flags))
    {
        return NULL;
    }

    GHashTable *metadata = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_values);

    // Map file into memmory
    char *pri_cont, *fil_cont, *oth_cont;
    int pri_len, fil_len, oth_len;

    g_file_get_contents(primary_xml_path, &pri_cont, &pri_len, NULL);
    g_file_get_contents(filelists_xml_path, &fil_cont, &fil_len, NULL);
    g_file_get_contents(other_xml_path, &oth_cont, &oth_len, NULL);

    GRegexCompileFlags c_flags = G_REGEX_DOTALL | G_REGEX_OPTIMIZE;
    GRegexMatchFlags   m_flags = 0;

    GRegex *package_re       = g_regex_new("<package.*?</package>", c_flags, m_flags, NULL);
    GRegex *filename_re      = g_regex_new("<location href=\"([^\"]*)", c_flags, m_flags, NULL);
    GRegex *checksum_type_re = g_regex_new("<checksum .*?type=\"([a-zA-Z0-9]+)\"", c_flags, m_flags, NULL);
    GRegex *filetime_re      = g_regex_new("<time .*?file=\"([0-9]+)\"", c_flags, m_flags, NULL);
    GRegex *size_re          = g_regex_new("<size .*?package=\"([0-9]+)\"", c_flags, m_flags, NULL);

    GMatchInfo *pri_pkg_match_info = NULL;
    GMatchInfo *fil_pkg_match_info = NULL;
    GMatchInfo *oth_pkg_match_info = NULL;

    g_regex_match(package_re, pri_cont, m_flags, &pri_pkg_match_info);
    g_regex_match(package_re, fil_cont, m_flags, &fil_pkg_match_info);
    g_regex_match(package_re, oth_cont, m_flags, &oth_pkg_match_info);

    // Iterate over all packages
    while (g_match_info_matches (pri_pkg_match_info) &&
           g_match_info_matches (fil_pkg_match_info) &&
           g_match_info_matches (oth_pkg_match_info))
    {
        char *pkg_pri = g_match_info_fetch (pri_pkg_match_info, 0);
        char *pkg_fil = g_match_info_fetch (fil_pkg_match_info, 0);
        char *pkg_oth = g_match_info_fetch (oth_pkg_match_info, 0);

        // Get Location
        GMatchInfo *mi_location;
        g_regex_match(filename_re, pkg_pri, m_flags, &mi_location);
        char *location = g_match_info_fetch(mi_location, 1);

        // Get Checksum type
        GMatchInfo *mi_checksum;
        g_regex_match(checksum_type_re, pkg_pri, m_flags, &mi_checksum);
        char *checksum_type = g_match_info_fetch(mi_checksum, 1);

        // Get Filetime
        GMatchInfo *mi_filetime;
        g_regex_match(filetime_re, pkg_pri, m_flags, &mi_filetime);
        char *filetime_str = g_match_info_fetch(mi_filetime, 1);
        gint64 filetime = g_ascii_strtoll(filetime_str, NULL, 10);

        // Get Size
        GMatchInfo *mi_size;
        g_regex_match(size_re, pkg_pri, m_flags, &mi_size);
        char *size_str = g_match_info_fetch(mi_size, 1);
        gint64 size = g_ascii_strtoll(size_str, NULL, 10);

        // Insert into metadata hash table
        struct package_metadata *pkg_md = g_malloc(sizeof(struct package_metadata));
        pkg_md->time_file = filetime;
        pkg_md->size_package = size;
        pkg_md->checksum_type = checksum_type;
        pkg_md->primary_xml = pkg_pri;
        pkg_md->filelists_xml = pkg_fil;
        pkg_md->other_xml = pkg_oth;
        g_hash_table_insert(metadata, location, pkg_md);

        // Clean up
        g_free(filetime_str);
        g_free(size_str);

        g_match_info_free(mi_location);
        g_match_info_free(mi_checksum);
        g_match_info_free(mi_filetime);
        g_match_info_free(mi_size);

        // Get next item
        g_match_info_next (pri_pkg_match_info, NULL);
        g_match_info_next (fil_pkg_match_info, NULL);
        g_match_info_next (oth_pkg_match_info, NULL);
    }

    g_match_info_free(pri_pkg_match_info);
    g_match_info_free(fil_pkg_match_info);
    g_match_info_free(oth_pkg_match_info);

    g_regex_unref(package_re);
    g_regex_unref(filename_re);
    g_regex_unref(checksum_type_re);
    g_regex_unref(filetime_re);
    g_regex_unref(size_re);

    g_free(pri_cont);
    g_free(fil_cont);
    g_free(oth_cont);

    return metadata;
}
