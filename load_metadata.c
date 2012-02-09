#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <zlib.h>
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


GHashTable *parse_xml_metadata(const char *pri_cont, const char *fil_cont, const char *oth_cont)
{
    if (!pri_cont || !fil_cont || !oth_cont) {
        return NULL;
    }

    GHashTable *metadata = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_values);

    GRegexCompileFlags c_flags = G_REGEX_DOTALL | G_REGEX_OPTIMIZE;
    GRegexMatchFlags   m_flags = 0;

    GRegex *package_re       = g_regex_new("<package.*?</package>", c_flags, m_flags, NULL);
    GRegex *filename_re      = g_regex_new("<location [^>]*href=\"([^\"]*)", c_flags, m_flags, NULL);
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
        if (!location) {
            puts("Warning: package location cannot be parsed");
            continue;
        }

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

    return metadata;
}


#define GZ_BUFFER_SIZE   131072  // 1024*128

GHashTable *load_gz_compressed_xml_metadata(const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path)
{
    GFileTest flags = G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR;
    if (!g_file_test(primary_xml_path, flags) ||
        !g_file_test(filelists_xml_path, flags) ||
        !g_file_test(other_xml_path, flags))
    {
        return NULL;
    }

    // Open files and get file descriptors
    int pri_xml_fd = open(primary_xml_path, O_RDONLY);
    int fil_xml_fd = open(filelists_xml_path, O_RDONLY);
    int oth_xml_fd = open(other_xml_path, O_RDONLY);

    // Get sizes of files
    int pri_xml_size = lseek(pri_xml_fd, 0, SEEK_END);
    lseek(pri_xml_fd, 0L, SEEK_SET);
    int fil_xml_size = lseek(fil_xml_fd, 0, SEEK_END);
    lseek(fil_xml_fd, 0L, SEEK_SET);
    int oth_xml_size = lseek(oth_xml_fd, 0, SEEK_END);
    lseek(oth_xml_fd, 0L, SEEK_SET);

    if (!pri_xml_size || !fil_xml_size || !oth_xml_size) {
        // One or more archives are empty
        close(pri_xml_fd);
        close(fil_xml_fd);
        close(oth_xml_fd);
        return NULL;
    }

    // Malloc space for store file contents in memmory
    const int compression_ratio = 10; // 15;  // Just my personal estimation

    size_t pri_cont_len = sizeof(char) * pri_xml_size * compression_ratio;
    char *pri_cont = malloc(pri_cont_len);
    size_t fil_cont_len = sizeof(char) * fil_xml_size * compression_ratio;
    char *fil_cont = malloc(fil_cont_len);
    size_t oth_cont_len = sizeof(char) * oth_xml_size * compression_ratio;
    char *oth_cont = malloc(oth_cont_len);

    // Open gziped file
    gzFile pri_xml_gzfile;
    gzFile fil_xml_gzfile;
    gzFile oth_xml_gzfile;

    if (!(pri_xml_gzfile = gzdopen(pri_xml_fd, "rb"))) {
//    if (!(pri_xml_gzfile = gzopen(primary_xml_path, "rb"))) {
        return NULL;
    }

    if (!(fil_xml_gzfile = gzdopen(fil_xml_fd, "rb"))) {
//    if (!(fil_xml_gzfile = gzopen(filelists_xml_path, "rb"))) {
        gzclose(pri_xml_gzfile);
        return NULL;
    }

    if (!(oth_xml_gzfile = gzdopen(oth_xml_fd, "rb"))) {
//    if (!(oth_xml_gzfile = gzopen(other_xml_path, "rb"))) {
        gzclose(pri_xml_gzfile);
        gzclose(fil_xml_gzfile);
        return NULL;
    }

    // Set buffers
    gzbuffer(pri_xml_gzfile, GZ_BUFFER_SIZE);
    gzbuffer(fil_xml_gzfile, GZ_BUFFER_SIZE);
    gzbuffer(oth_xml_gzfile, GZ_BUFFER_SIZE);

    size_t read;

    // TODO: This 3 read function merge into 1

    read = gzread(pri_xml_gzfile, pri_cont, pri_cont_len);
    if (read == -1) {
        printf("Cannot read %s\n", pri_xml_gzfile);
        return NULL;
    }
    if (read == pri_cont_len) {
        fflush(stdout);
        // Estimation of compress ratio failed.. Realloc
        int ext_len = (sizeof(char) * pri_xml_size * 2);  // This magic 2 is just magic, have no special meaning
        do {
            pri_cont = realloc(pri_cont, (pri_cont_len+ext_len));
            read += gzread(pri_xml_gzfile, (pri_cont+pri_cont_len), ext_len);
            pri_cont_len += ext_len;
        } while (read == ext_len);
        pri_cont[fil_cont_len-ext_len+read] = '\0';
    } else {
        pri_cont[read] = '\0';
    }

    read = gzread(fil_xml_gzfile, fil_cont, fil_cont_len);
    if (read == -1) {
        printf("Cannot read %s\n", fil_xml_gzfile);
        return NULL;
    }
    if (read == fil_cont_len) {
        // Estimation of compress ratio failed.. Realloc
        int ext_len = (sizeof(char) * fil_xml_size * 2);  // This magic 2 is just magic, have no special meaning
        do {
            fil_cont = realloc(fil_cont, fil_cont_len+ext_len);
            read = gzread(fil_xml_gzfile, (fil_cont+fil_cont_len), ext_len);
            fil_cont_len += ext_len;
        } while (read == ext_len);

        fil_cont[fil_cont_len-ext_len+read] = '\0';
    } else {
        fil_cont[read] = '\0';
    }

    read = gzread(oth_xml_gzfile, oth_cont, oth_cont_len);
    if (read == -1) {
        printf("Cannot read %s\n", oth_xml_gzfile);
        return NULL;
    }
    if (read == oth_cont_len) {
        // Estimation of compress ratio failed.. Realloc
        int ext_len = (sizeof(char) * oth_xml_size * 2);  // This magic 2 is just magic, have no special meaning
        do {
            oth_cont = realloc(oth_cont, (oth_cont_len+ext_len));
            read += gzread(oth_xml_gzfile, (oth_cont+oth_cont_len), ext_len);
            oth_cont_len += ext_len;
        } while (read == ext_len);
        oth_cont[oth_cont_len-ext_len+read] = '\0';
    } else {
        oth_cont[read] = '\0';
    }

    gzclose(pri_xml_gzfile);
    gzclose(fil_xml_gzfile);
    gzclose(oth_xml_gzfile);

    GHashTable *result = parse_xml_metadata(pri_cont, fil_cont, oth_cont);

    free(pri_cont);
    free(fil_cont);
    free(oth_cont);

    return result;
}


GHashTable *load_xml_metadata(const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path)
{
    GFileTest flags = G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR;
    if (!g_file_test(primary_xml_path, flags) ||
        !g_file_test(filelists_xml_path, flags) ||
        !g_file_test(other_xml_path, flags))
    {
        return NULL;
    }

    // Map file into memmory
    char *pri_cont, *fil_cont, *oth_cont;
    g_file_get_contents(primary_xml_path, &pri_cont, NULL, NULL);
    g_file_get_contents(filelists_xml_path, &fil_cont, NULL, NULL);
    g_file_get_contents(other_xml_path, &oth_cont, NULL, NULL);

    GHashTable *result = parse_xml_metadata(pri_cont, fil_cont, oth_cont);

    g_free(pri_cont);
    g_free(fil_cont);
    g_free(oth_cont);

    return result;
}


GHashTable *locate_and_load_xml_metadata(const char *repopath)
{
    // TODO: First try get info from repomd.xml

    if (!repopath || !g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        return NULL;
    }

    gchar *pri_gz_xml = NULL;
    gchar *fil_gz_xml = NULL;
    gchar *oth_gz_xml = NULL;
    gchar *pri_xml = NULL;
    gchar *fil_xml = NULL;
    gchar *oth_xml = NULL;

    GDir *repodir = g_dir_open(repopath, 0, NULL);
    if (!repodir) {
        return NULL;
    }

    gchar *file;
    while (file = g_dir_read_name (repodir)) {
        if (g_str_has_suffix(file, "primary.xml.gz")) {
            pri_gz_xml = g_strconcat (repopath, file, NULL);
        } else if (g_str_has_suffix(file, "filelists.xml.gz")) {
            fil_gz_xml = g_strconcat (repopath, file, NULL);
        } else if (g_str_has_suffix(file, "other.xml.gz")) {
            oth_gz_xml = g_strconcat (repopath, file, NULL);
        } else if (g_str_has_suffix(file, "primary.xml")) {
            pri_xml = g_strconcat (repopath, file, NULL);
        } else if (g_str_has_suffix(file, "filelists.xml")) {
            fil_xml = g_strconcat (repopath, file, NULL);
        } else if (g_str_has_suffix(file, "other.xml")) {
            oth_xml = g_strconcat (repopath, file, NULL);
        }
    }

    g_dir_close(repodir);

    GHashTable *result = NULL;

    if (pri_gz_xml && fil_gz_xml && oth_gz_xml) {
        result = load_gz_compressed_xml_metadata(pri_gz_xml, fil_gz_xml, oth_gz_xml);
    } else if (pri_xml && fil_xml && oth_xml) {
        result = load_xml_metadata(pri_xml, fil_xml, oth_xml);
    }

    g_free(pri_gz_xml);
    g_free(fil_gz_xml);
    g_free(oth_gz_xml);
    g_free(pri_xml);
    g_free(fil_xml);
    g_free(oth_xml);

    return result;
}

