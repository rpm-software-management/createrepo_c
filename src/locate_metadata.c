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
#include <glib/gstdio.h>
#include <libxml/xmlreader.h>
#include <curl/curl.h>
#include <string.h>
#include <errno.h>
#include "logging.h"
#include "misc.h"
#include "locate_metadata.h"

#undef MODULE
#define MODULE "locate_metadata: "

#define TMPDIR_PATTERN  "/tmp/createrepo_c_tmp_repo_XXXXXX"

#define FORMAT_XML      1
#define FORMAT_LEVEL    0



void
cr_free_metadata_location(struct cr_MetadataLocation *ml)
{
    if (!ml) {
        return;
    }

    if (ml->tmp_dir) {
        g_debug(MODULE"%s: Removing %s", __func__,  ml->tmp_dir);
        cr_remove_dir(ml->tmp_dir);
    }

    g_free(ml->pri_xml_href);
    g_free(ml->fil_xml_href);
    g_free(ml->oth_xml_href);
    g_free(ml->pri_sqlite_href);
    g_free(ml->fil_sqlite_href);
    g_free(ml->oth_sqlite_href);
    g_free(ml->groupfile_href);
    g_free(ml->cgroupfile_href);
    g_free(ml->updateinfo_href);
    g_free(ml->repomd);
    g_free(ml->original_url);
    g_free(ml->local_path);
    g_free(ml->tmp_dir);
    g_free(ml);
}



struct cr_MetadataLocation *
parse_repomd(const char *repomd_path, const char *repopath, int ignore_sqlite)
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
        g_warning(MODULE"%s: Bad xml - missing repomd element? (%s)",
                  __func__, name);
        xmlFree(name);
        xmlFreeTextReader(reader);
        return NULL;
    }
    xmlFree(name);

    ret = xmlTextReaderRead(reader);
    name = xmlTextReaderName(reader);
    if (g_strcmp0((char *) name, "revision")) {
        g_warning(MODULE"%s: Bad xml - missing revision element? (%s)",
                  __func__, name);
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

    struct cr_MetadataLocation *mdloc;
    mdloc = g_malloc0(sizeof(struct cr_MetadataLocation));
    mdloc->repomd = g_strdup(repomd_path);
    mdloc->local_path = g_strdup(repopath);

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
            if (ignore_sqlite) {
                g_free(full_location_href);
                full_location_href = NULL;
            }
            mdloc->pri_sqlite_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "filelists_db")) {
            if (ignore_sqlite) {
                g_free(full_location_href);
                full_location_href = NULL;
            }
            mdloc->fil_sqlite_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "other_db")) {
            if (ignore_sqlite) {
                g_free(full_location_href);
                full_location_href = NULL;
            }
            mdloc->oth_sqlite_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "group")) {
            mdloc->groupfile_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "group_gz")) {
            // even with a createrepo param --xz this name has a _gz suffix
            mdloc->cgroupfile_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "updateinfo")) {
            mdloc->updateinfo_href = full_location_href;
        } else {
            g_warning("Unknown data in repomd.xml \"%s\"", data_type);
            g_free(full_location_href);
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



struct cr_MetadataLocation *
get_local_metadata(const char *in_repopath, int ignore_sqlite)
{
    struct cr_MetadataLocation *ret = NULL;

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
        g_free(repopath);
        return ret;
    }

    ret = parse_repomd(repomd, repopath, ignore_sqlite);

    g_free(repomd);
    g_free(repopath);

    return ret;
}



struct cr_MetadataLocation *
get_remote_metadata(const char *repopath, int ignore_sqlite)
{
    gchar *url = NULL;
    gchar *tmp_repomd = NULL;
    gchar *tmp_repodata = NULL;
    FILE *f_repomd = NULL;
    CURL *handle = NULL;
    CURLcode rcode;
    char errorbuf[CURL_ERROR_SIZE];
    gchar tmp_dir[] = TMPDIR_PATTERN;
    struct cr_MetadataLocation *r_location = NULL;
    struct cr_MetadataLocation *ret = NULL;

    if (!repopath) {
        return ret;
    }


    // Create temporary repo in /tmp

    if(!mkdtemp(tmp_dir)) {
        g_critical(MODULE"%s: Cannot create a temporary directory: %s",
                   __func__, strerror(errno));
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

    r_location = parse_repomd(tmp_repomd, repopath, ignore_sqlite);
    if (!r_location) {
        g_critical(MODULE"%s: repomd.xml parser failed on %s", __func__, tmp_repomd);
        goto get_remote_metadata_cleanup;
    }


    // Download all other repofiles
    char *error = NULL;

    if (r_location->pri_xml_href)
        cr_download(handle, r_location->pri_xml_href, tmp_repodata, &error);
    if (!error && r_location->fil_xml_href)
        cr_download(handle, r_location->fil_xml_href, tmp_repodata, &error);
    if (!error && r_location->oth_xml_href)
        cr_download(handle, r_location->oth_xml_href, tmp_repodata, &error);
    if (!error && r_location->pri_sqlite_href)
        cr_download(handle, r_location->pri_sqlite_href, tmp_repodata, &error);
    if (!error && r_location->fil_sqlite_href)
        cr_download(handle, r_location->fil_sqlite_href, tmp_repodata, &error);
    if (!error && r_location->oth_sqlite_href)
        cr_download(handle, r_location->oth_sqlite_href, tmp_repodata, &error);
    if (!error && r_location->groupfile_href)
        cr_download(handle, r_location->groupfile_href, tmp_repodata, &error);
    if (!error && r_location->cgroupfile_href)
        cr_download(handle, r_location->cgroupfile_href, tmp_repodata, &error);
    if (!error && r_location->updateinfo_href)
        cr_download(handle, r_location->updateinfo_href, tmp_repodata, &error);

    if (error) {
        g_critical(MODULE"%s: Error while downloadig files: %s", __func__, error);
        g_free(error);
        goto get_remote_metadata_cleanup;
    }

    g_debug(MODULE"%s: Remote metadata was successfully downloaded", __func__);


    // Parse downloaded data

    ret = get_local_metadata(tmp_dir, ignore_sqlite);
    if (ret) {
        ret->tmp_dir = g_strdup(tmp_dir);
    }


get_remote_metadata_cleanup:

    if (f_repomd) fclose(f_repomd);
    g_free(tmp_repomd);
    g_free(tmp_repodata);
    g_free(url);
    curl_easy_cleanup(handle);
    if (!ret) cr_remove_dir(tmp_dir);
    if (r_location) cr_free_metadata_location(r_location);

    return ret;
}



struct cr_MetadataLocation *
cr_get_metadata_location(const char *in_repopath, int ignore_sqlite)
{
    struct cr_MetadataLocation *ret = NULL;

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
        ret = get_remote_metadata(repopath, ignore_sqlite);
    } else {
        const char *path = repopath;
        if (g_str_has_prefix(repopath, "file://")) {
            path = repopath+7;
        }
        ret = get_local_metadata(path, ignore_sqlite);
    }

    if (ret) {
        ret->original_url = g_strdup(in_repopath);
    }
    g_free(repopath);
    return ret;
}



// Return list of non-null pointers on strings in the passed structure
GSList *
get_list_of_md_locations (struct cr_MetadataLocation *ml)
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
    if (ml->updateinfo_href) list = g_slist_prepend(list, (gpointer) ml->updateinfo_href);
    if (ml->repomd)          list = g_slist_prepend(list, (gpointer) ml->repomd);

    return list;
}



void
free_list_of_md_locations(GSList *list)
{
    if (list) {
        g_slist_free(list);
    }
}



int
cr_remove_metadata(const char *repopath)
{
    int removed_files = 0;
    gchar *full_repopath;
    const gchar *file;
    GDir *repodir;
    struct cr_MetadataLocation *ml;

    if (!repopath || !g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        g_debug(MODULE"%s: remove_old_metadata: Cannot remove %s", __func__, repopath);
        return -1;
    }

    full_repopath = g_strconcat(repopath, "/repodata/", NULL);
    repodir = g_dir_open(full_repopath, 0, NULL);
    if (!repodir) {
        g_debug(MODULE"%s: Path %s doesn't exists", __func__, repopath);
        g_free(full_repopath);
        return -1;
    }


    // Remove all metadata listed in repomd.xml

    ml = cr_get_metadata_location(repopath, 0);
    if (ml) {
        GSList *list = get_list_of_md_locations(ml);
        GSList *element;

        for (element=list; element; element=element->next) {
            gchar *path = (char *) element->data;

            g_debug(MODULE"%s: Removing: %s (path obtained from repomd.xml)", __func__, path);
            if (g_remove(path) == -1) {
                // g_warning(MODULE"%s: remove_old_metadata: Cannot remove %s", __func__, path);
                ;
            } else {
                removed_files++;
            }
        }

        free_list_of_md_locations(list);
        cr_free_metadata_location(ml);
    }


    // (Just for sure) List dir and remove all files which could be related to an old metadata

    while ((file = g_dir_read_name (repodir))) {
        if (g_str_has_suffix(file, "primary.xml.gz") ||
            g_str_has_suffix(file, "filelists.xml.gz") ||
            g_str_has_suffix(file, "other.xml.gz") ||
            g_str_has_suffix(file, "primary.xml.bz2") ||
            g_str_has_suffix(file, "filelists.xml.bz2") ||
            g_str_has_suffix(file, "other.xml.bz2") ||
            g_str_has_suffix(file, "primary.xml.xz") ||
            g_str_has_suffix(file, "filelists.xml.xz") ||
            g_str_has_suffix(file, "other.xml.xz") ||
            g_str_has_suffix(file, "primary.xml") ||
            g_str_has_suffix(file, "filelists.xml") ||
            g_str_has_suffix(file, "other.xml") ||
            g_str_has_suffix(file, "updateinfo.xml") ||
            !g_strcmp0(file, "repomd.xml"))
        {
            gchar *path = g_strconcat(full_repopath, file, NULL);
            g_debug(MODULE"%s: Removing: %s", __func__, path);
            if (g_remove(path) == -1)
                g_warning(MODULE"%s: Cannot remove %s", __func__, path);
            else
                removed_files++;
            g_free(path);
        }
    }

    g_dir_close(repodir);
    g_free(full_repopath);

    return removed_files;
}


typedef struct _old_file {
    time_t mtime;
    gchar  *path;
} OldFile;


void
free_old_file(gpointer data)
{
    OldFile *old_file = (OldFile *) data;
    g_free(old_file->path);
    g_free(old_file);
}


gint
cmp_old_repodata_files(gconstpointer a, gconstpointer b)
{
    if (((OldFile *) a)->mtime < ((OldFile *) b)->mtime)
        return 1;
    if (((OldFile *) a)->mtime > ((OldFile *) b)->mtime)
        return -1;
    return 0;
}


void
stat_and_insert(const gchar *dirname, const gchar *filename, GSList **list)
{
    GStatBuf buf;
    OldFile *old_file;
    gchar *path = g_strconcat(dirname, filename, NULL);
    if (g_stat(path, &buf))
        buf.st_mtime = 1;
    old_file = g_malloc0(sizeof(OldFile));
    old_file->mtime = buf.st_mtime;
    old_file->path  = path;
    *list = g_slist_insert_sorted(*list, old_file, cmp_old_repodata_files);
}


int
remove_listed_files(GSList *list, int retain)
{
    int removed = 0;
    GSList *el;

    if (retain < 0) retain = 0;

    el = g_slist_nth(list, retain);
    for (; el; el = g_slist_next(el)) {
        OldFile *of = (OldFile *) el->data;
        g_debug(MODULE"%s: Removing: %s", __func__, of->path);
        if (g_remove(of->path) == -1)
            g_warning(MODULE"%s: Cannot remove %s", __func__, of->path);
        else
            removed++;
    }
    return removed;
}


int
cr_remove_metadata_classic(const char *repopath, int retain)
{
    gchar *full_repopath;
    GDir *repodir;
    const gchar *file;
    GSList *pri_lst = NULL, *pri_db_lst = NULL;
    GSList *fil_lst = NULL, *fil_db_lst = NULL;
    GSList *oth_lst = NULL, *oth_db_lst = NULL;

    if (!repopath || !g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        g_debug(MODULE"%s: remove_old_metadata: Cannot remove %s", __func__, repopath);
        return -1;
    }

    full_repopath = g_strconcat(repopath, "/repodata/", NULL);
    repodir = g_dir_open(full_repopath, 0, NULL);
    if (!repodir) {
        g_debug(MODULE"%s: Path %s doesn't exists", __func__, repopath);
        g_free(full_repopath);
        return -1;
    }


    // Create sorted (by mtime) lists of old metadata files
    // More recent files are first

    while ((file = g_dir_read_name (repodir))) {
        // Get filename without suffix
        gchar *name_without_suffix, *dot = NULL, *i = file;
        for (; *i != '\0'; i++) if (*i == '.') dot = i;
        if (!dot) continue;  // Filename doesn't contain '.'
        name_without_suffix = g_strndup(file, (dot - file));

        if (g_str_has_suffix(name_without_suffix, "primary.xml")) {
            stat_and_insert(full_repopath, file, &pri_lst);
        } else if (g_str_has_suffix(name_without_suffix, "primary.sqlite")) {
            stat_and_insert(full_repopath, file, &pri_db_lst);
        } else if (g_str_has_suffix(name_without_suffix, "filelists.xml")) {
            stat_and_insert(full_repopath, file, &fil_lst);
        } else if (g_str_has_suffix(name_without_suffix, "filelists.sqlite")) {
            stat_and_insert(full_repopath, file, &fil_db_lst);
        } else if (g_str_has_suffix(name_without_suffix, "other.xml")) {
            stat_and_insert(full_repopath, file, &oth_lst);
        } else if (g_str_has_suffix(name_without_suffix, "other.sqlite")) {
            stat_and_insert(full_repopath, file, &oth_db_lst);
        }
        g_free(name_without_suffix);
    }

    g_dir_close(repodir);
    g_free(full_repopath);

    // Remove old metadata

    remove_listed_files(pri_lst, retain);
    remove_listed_files(pri_db_lst, retain);
    remove_listed_files(fil_lst, retain);
    remove_listed_files(fil_db_lst, retain);
    remove_listed_files(oth_lst, retain);
    remove_listed_files(oth_db_lst, retain);

    g_slist_free_full(pri_lst, free_old_file);
    g_slist_free_full(pri_db_lst, free_old_file);
    g_slist_free_full(fil_lst, free_old_file);
    g_slist_free_full(fil_db_lst, free_old_file);
    g_slist_free_full(oth_lst, free_old_file);
    g_slist_free_full(oth_db_lst, free_old_file);

    return 0;
}
