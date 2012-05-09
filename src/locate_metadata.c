#include <glib.h>
#include <glib/gstdio.h>
#include <libxml/xmlreader.h>
#include <curl/curl.h>
#include <string.h>
#include <errno.h>
#include "misc.h"
#include "locate_metadata.h"

#undef MODULE
#define MODULE "locate_metadata: "

#define TMPDIR_PATTERN  "/tmp/createrepo_c_tmp_repo_XXXXXX"

#define FORMAT_XML      1
#define FORMAT_LEVEL    0



void free_metadata_location(struct MetadataLocation *ml)
{
    if (!ml) {
        return;
    }

    if (ml->tmp_dir) {
        g_debug(MODULE"%s: Removing %s", __func__,  ml->tmp_dir);
        remove_dir(ml->tmp_dir);
    }

    g_free(ml->pri_xml_href);
    g_free(ml->fil_xml_href);
    g_free(ml->oth_xml_href);
    g_free(ml->pri_sqlite_href);
    g_free(ml->fil_sqlite_href);
    g_free(ml->oth_sqlite_href);
    g_free(ml->groupfile_href);
    g_free(ml->cgroupfile_href);
    g_free(ml->repomd);
    g_free(ml->tmp_dir);
    g_free(ml);
}



struct MetadataLocation *parse_repomd(const char *repomd_path, const char *repopath)
{
    int ret;
    xmlChar *name;
    xmlTextReaderPtr reader;


    // Parsing
    reader = xmlReaderForFile(repomd_path, NULL, XML_PARSE_NOBLANKS);
    if (!reader) {
        g_warning(MODULE"%s: Error while xmlReaderForFile()", __func__);
        return NULL;
    }

    ret = xmlTextReaderRead(reader);
    name = xmlTextReaderName(reader);
    if (g_strcmp0((char *) name, "repomd")) {
        g_warning(MODULE"%s: Bad xml - missing repomd element? (%s)", __func__, name);
        xmlFree(name);
        xmlFreeTextReader(reader);
        return NULL;
    }
    xmlFree(name);

    ret = xmlTextReaderRead(reader);
    name = xmlTextReaderName(reader);
    if (g_strcmp0((char *) name, "revision")) {
        g_warning(MODULE"%s: Bad xml - missing revision element? (%s)", __func__, name);
        xmlFree(name);
        xmlFreeTextReader(reader);
        return NULL;
    }
    xmlFree(name);


    // Parse data elements

    while (ret) {
        // Find first data element
        ret = xmlTextReaderRead(reader);
        name = xmlTextReaderName(reader);
        if (!g_strcmp0((char *) name, "data")) {
            xmlFree(name);
            break;
        }
        xmlFree(name);
    }

    if (!ret) {
        // No elements left -> Bad xml
        g_warning(MODULE"%s: Bad xml - missing data elements?", __func__);
        xmlFreeTextReader(reader);
        return NULL;
    }

    struct MetadataLocation *mdloc;
    mdloc = g_malloc0(sizeof(struct MetadataLocation));
    mdloc->repomd = g_strdup(repomd_path);

    xmlChar *data_type = NULL;
    xmlChar *location_href = NULL;

    while (ret) {
        if (xmlTextReaderNodeType(reader) != 1) {
            ret = xmlTextReaderNext(reader);
            continue;
        }

        xmlNodePtr data_node = xmlTextReaderExpand(reader);
        data_type = xmlGetProp(data_node, (xmlChar *) "type");
        xmlNodePtr sub_node = data_node->children;

        while (sub_node) {
            if (sub_node->type != XML_ELEMENT_NODE) {
                sub_node = xmlNextElementSibling(sub_node);
                continue;
            }

            if (!g_strcmp0((char *) sub_node->name, "location")) {
                location_href = xmlGetProp(sub_node, (xmlChar *) "href");
            }

            // TODO: Check repodata validity checksum? mtime? size?

            sub_node = xmlNextElementSibling(sub_node);
        }


        // Build absolute path

        gchar *full_location_href;
        full_location_href = g_strconcat(repopath, (char *) location_href, NULL);


        // Store the path

        if (!g_strcmp0((char *) data_type, "primary")) {
            mdloc->pri_xml_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "filelists")) {
            mdloc->fil_xml_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "other")) {
            mdloc->oth_xml_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "primary_db")) {
            mdloc->pri_sqlite_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "filelists_db")) {
            mdloc->fil_sqlite_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "other_db")) {
            mdloc->oth_sqlite_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "group")) {
            mdloc->groupfile_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "group_gz")) { // even with a createrepo param --xz this name has a _gz suffix
            mdloc->cgroupfile_href = full_location_href;
        }


        // Memory cleanup

        xmlFree(data_type);
        xmlFree(location_href);
        location_href = NULL;

        ret = xmlTextReaderNext(reader);
    }

    xmlFreeTextReader(reader);

    return mdloc;
}



struct MetadataLocation *get_local_metadata(const char *in_repopath)
{
    struct MetadataLocation *ret = NULL;

    if (!in_repopath) {
        return ret;
    }

    if (!g_file_test(in_repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        g_warning(MODULE"%s: %s is not a directory", __func__, in_repopath);
        return ret;
    }


    // Create path to repomd.xml and check if it exists

    gchar *repomd;
    gchar *repopath;
    if (!g_str_has_suffix(in_repopath, "/"))
        repopath = g_strconcat(in_repopath, "/", NULL);
    else
        repopath = g_strdup(in_repopath);
    repomd = g_strconcat(repopath, "repodata/repomd.xml", NULL);

    if (!g_file_test(repomd, G_FILE_TEST_EXISTS)) {
        g_debug(MODULE"%s: %s doesn't exists", __func__, repomd);
        g_free(repomd);
        return ret;
    }

    ret = parse_repomd(repomd, repopath);

    g_free(repomd);
    g_free(repopath);

    return ret;
}



struct MetadataLocation *get_remote_metadata(const char *repopath)
{
    gchar *url = NULL;
    gchar *tmp_repomd = NULL;
    gchar *tmp_repodata = NULL;
    FILE *f_repomd = NULL;
    CURL *handle = NULL;
    CURLcode rcode;
    char errorbuf[CURL_ERROR_SIZE];
    gchar tmp_dir[] = TMPDIR_PATTERN;
    struct MetadataLocation *r_location = NULL;
    struct MetadataLocation *ret = NULL;

    if (!repopath) {
        return ret;
    }


    // Create temporary repo in /tmp

    if(!mkdtemp(tmp_dir)) {
        g_critical(MODULE"%s: Cannot create a temporary directory: %s", __func__, strerror(errno));
        return ret;
    }

    tmp_repodata = g_strconcat(tmp_dir, "/repodata/", NULL);

    if (g_mkdir (tmp_repodata, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
        g_critical(MODULE"%s: Cannot create a temporary directory", __func__);
        return ret;
    }


    // Prepare temporary repomd.xml

    tmp_repomd = g_strconcat(tmp_repodata, "repomd.xml", NULL);
    f_repomd = fopen(tmp_repomd, "w");
    if (!f_repomd) {
        g_critical(MODULE"%s: Cannot open %s", __func__, tmp_repomd);
        goto get_remote_metadata_cleanup;
    }

    g_debug(MODULE"%s: Using tmp dir: %s", __func__, tmp_dir);


    // Download repo files

    url = g_strconcat(repopath, "repodata/repomd.xml", NULL);
    handle = curl_easy_init();

    // Set error buffer
    if (curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errorbuf) != CURLE_OK) {
        g_critical(MODULE"%s: curl_easy_setopt(CURLOPT_ERRORBUFFER) error", __func__);
        goto get_remote_metadata_cleanup;
    }

    // Set URL
    if (curl_easy_setopt(handle, CURLOPT_URL, url) != CURLE_OK) {
        g_critical(MODULE"%s: curl_easy_setopt(CURLOPT_URL) error", __func__);
        goto get_remote_metadata_cleanup;
    }

    // Set output file descriptor
    if (curl_easy_setopt(handle, CURLOPT_WRITEDATA, f_repomd) != CURLE_OK) {
        g_critical(MODULE"%s: curl_easy_setopt(CURLOPT_WRITEDATA) error", __func__);
        goto get_remote_metadata_cleanup;
    }

    // Fail on HTTP error (return code >= 400)
    if (curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1) != CURLE_OK) {
        g_critical(MODULE"%s: curl_easy_setopt(CURLOPT_FAILONERROR) error", __func__);
        goto get_remote_metadata_cleanup;
    }


    // Perform a file transfer of repomd.xml

    rcode = curl_easy_perform(handle);
    if (rcode != 0) {
        g_critical(MODULE"%s: curl_easy_perform() error: %s", __func__, errorbuf);
        goto get_remote_metadata_cleanup;
    }

    fclose(f_repomd);
    f_repomd = NULL;


    // Parse downloaded repomd.xml

    r_location = parse_repomd(tmp_repomd, repopath);
    if (!r_location) {
        g_critical(MODULE"%s: repomd.xml parser failed on %s", __func__, tmp_repomd);
        goto get_remote_metadata_cleanup;
    }


    // Download all other repofiles
    char *error = NULL;

    if (r_location->pri_xml_href)
        download(handle, r_location->pri_xml_href, tmp_repodata, &error);
    if (!error && r_location->fil_xml_href)
        download(handle, r_location->fil_xml_href, tmp_repodata, &error);
    if (!error && r_location->oth_xml_href)
        download(handle, r_location->oth_xml_href, tmp_repodata, &error);
    if (!error && r_location->pri_sqlite_href)
        download(handle, r_location->pri_sqlite_href, tmp_repodata, &error);
    if (!error && r_location->fil_sqlite_href)
        download(handle, r_location->fil_sqlite_href, tmp_repodata, &error);
    if (!error && r_location->oth_sqlite_href)
        download(handle, r_location->oth_sqlite_href, tmp_repodata, &error);
    if (!error && r_location->groupfile_href)
        download(handle, r_location->groupfile_href, tmp_repodata, &error);
    if (!error && r_location->cgroupfile_href)
        download(handle, r_location->cgroupfile_href, tmp_repodata, &error);

    if (error) {
        g_critical(MODULE"%s: Error while downloadig files: %s", __func__, error);
        g_free(error);
        goto get_remote_metadata_cleanup;
    }

    g_debug(MODULE"%s: Remote metadata was successfully downloaded", __func__);


    // Parse downloaded data

    ret = get_local_metadata(tmp_dir);
    if (ret) {
        ret->tmp_dir = g_strdup(tmp_dir);
    }


get_remote_metadata_cleanup:

    if (f_repomd) fclose(f_repomd);
    g_free(tmp_repomd);
    g_free(tmp_repodata);
    g_free(url);
    curl_easy_cleanup(handle);
    if (!ret) remove_dir(tmp_dir);
    if (r_location) free_metadata_location(r_location);

    return ret;
}



struct MetadataLocation *get_metadata_location(const char *in_repopath)
{
    struct MetadataLocation *ret = NULL;

    // repopath must ends with slash

    gchar *repopath;

    if (g_str_has_suffix(in_repopath, "/")) {
        repopath = g_strdup(in_repopath);
    } else {
        repopath = g_strconcat(in_repopath, "/", NULL);
    }

    if (g_str_has_prefix(repopath, "ftp://") ||
        g_str_has_prefix(repopath, "http://") ||
        g_str_has_prefix(repopath, "https://"))
    {
        // Download data via curl
        ret = get_remote_metadata(repopath);
    } else {
        const char *path = repopath;
        if (g_str_has_prefix(repopath, "file://")) {
            path = repopath+7;
        }
        ret = get_local_metadata(path);
    }

    g_free(repopath);
    return ret;
}



// Return list of non-null pointers on strings in the passed structure
GSList *get_list_of_md_locations (struct MetadataLocation *ml)
{
    GSList *list = NULL;

    if (!ml) {
        return list;
    }

    if (ml->pri_xml_href)    list = g_slist_prepend(list, (gpointer) ml->pri_xml_href);
    if (ml->fil_xml_href)    list = g_slist_prepend(list, (gpointer) ml->fil_xml_href);
    if (ml->oth_xml_href)    list = g_slist_prepend(list, (gpointer) ml->oth_xml_href);
    if (ml->pri_sqlite_href) list = g_slist_prepend(list, (gpointer) ml->pri_sqlite_href);
    if (ml->fil_sqlite_href) list = g_slist_prepend(list, (gpointer) ml->fil_sqlite_href);
    if (ml->oth_sqlite_href) list = g_slist_prepend(list, (gpointer) ml->oth_sqlite_href);
    if (ml->groupfile_href)  list = g_slist_prepend(list, (gpointer) ml->groupfile_href);
    if (ml->cgroupfile_href) list = g_slist_prepend(list, (gpointer) ml->cgroupfile_href);
    if (ml->repomd)          list = g_slist_prepend(list, (gpointer) ml->repomd);
    if (ml->tmp_dir)         list = g_slist_prepend(list, (gpointer) ml->tmp_dir);

    return list;
}



void free_list_of_md_locations(GSList *list)
{
    if (list) {
        g_slist_free(list);
    }
}



int remove_metadata(const char *repopath)
{
    if (!repopath || !g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        g_debug(MODULE"%s: remove_old_metadata: Cannot remove %s", __func__, repopath);
        return -1;
    }

    gchar *full_repopath;
    full_repopath = g_strconcat(repopath, "/repodata/", NULL);

    GDir *repodir;
    repodir = g_dir_open(full_repopath, 0, NULL);
    if (!repodir) {
        g_debug(MODULE"%s: Path %s doesn't exists", __func__, repopath);
        return -1;
    }


    // Remove all metadata listed in repomd.xml

    int removed_files = 0;

    struct MetadataLocation *ml;
    ml = get_metadata_location(repopath);
    if (ml) {
        GSList *list = get_list_of_md_locations(ml);
        GSList *element;

        for (element=list; element; element=element->next) {
            gchar *path = (char *) element->data;

            g_debug(MODULE"%s: Removing: %s (path obtained from repomd.xml)", __func__, path);
            if (g_remove(path) == -1) {
                g_warning(MODULE"%s: remove_old_metadata: Cannot remove %s", __func__, path);
            } else {
                removed_files++;
            }
        }

        free_list_of_md_locations(list);
        free_metadata_location(ml);
    }


    // (Just for sure) List dir and remove all files which could be related to an old metadata

    const gchar *file;
    while ((file = g_dir_read_name (repodir))) {
        if (g_str_has_suffix(file, "primary.xml.gz") ||
            g_str_has_suffix(file, "filelists.xml.gz") ||
            g_str_has_suffix(file, "other.xml.gz") ||
            g_str_has_suffix(file, "primary.xml.bz2") ||
            g_str_has_suffix(file, "filelists.xml.bz2") ||
            g_str_has_suffix(file, "other.xml.bz2") ||
            g_str_has_suffix(file, "primary.xml") ||
            g_str_has_suffix(file, "filelists.xml") ||
            g_str_has_suffix(file, "other.xml") ||
            !g_strcmp0(file, "repomd.xml"))
        {
            gchar *path;
            path = g_strconcat(full_repopath, file, NULL);

            g_debug(MODULE"%s: Removing: %s", __func__, path);
            if (g_remove(path) == -1) {
                g_warning(MODULE"%s: Cannot remove %s", __func__, path);
            } else {
                removed_files++;
            }

            g_free(path);
        }
    }

    g_dir_close(repodir);
    g_free(full_repopath);

    return removed_files;
}
