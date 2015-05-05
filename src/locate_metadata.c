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
#include <assert.h>
#include <libxml/xmlreader.h>
#include <curl/curl.h>
#include <string.h>
#include <errno.h>
#include "error.h"
#include "misc.h"
#include "locate_metadata.h"
#include "repomd.h"
#include "xml_parser.h"
#include "cleanup.h"

#define ERR_DOMAIN      CREATEREPO_C_ERROR

#define TMPDIR_PATTERN  "createrepo_c_tmp_repo_XXXXXX"

#define FORMAT_XML      1
#define FORMAT_LEVEL    0



void
cr_metadatalocation_free(struct cr_MetadataLocation *ml)
{
    if (!ml)
        return;

    if (ml->tmp && ml->local_path) {
        g_debug("%s: Removing %s", __func__,  ml->local_path);
        cr_remove_dir(ml->local_path, NULL);
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
    g_free(ml);
}

static struct cr_MetadataLocation *
cr_parse_repomd(const char *repomd_path,
                const char *repopath,
                int ignore_sqlite)
{
    assert(repomd_path);

    GError *tmp_err = NULL;
    cr_Repomd *repomd = cr_repomd_new();

    cr_xml_parse_repomd(repomd_path, repomd, cr_warning_cb,
                        "Repomd xml parser", &tmp_err);
    if (tmp_err) {
        g_critical("%s: %s", __func__, tmp_err->message);
        g_error_free(tmp_err);
        cr_repomd_free(repomd);
        return NULL;
    }

    struct cr_MetadataLocation *mdloc;
    mdloc = g_malloc0(sizeof(struct cr_MetadataLocation));
    mdloc->repomd = g_strdup(repomd_path);
    mdloc->local_path = g_strdup(repopath);

    for (GSList *elem = repomd->records; elem; elem = g_slist_next(elem)) {
        cr_RepomdRecord *record = elem->data;

        gchar *full_location_href = g_build_filename(
                                        repopath,
                                        (char *) record->location_href,
                                        NULL);

        if (!g_strcmp0(record->type, "primary"))
            mdloc->pri_xml_href = full_location_href;
        else if (!g_strcmp0(record->type, "primary_db") && !ignore_sqlite)
            mdloc->pri_sqlite_href = full_location_href;
        else if (!g_strcmp0(record->type, "filelists"))
            mdloc->fil_xml_href = full_location_href;
        else if (!g_strcmp0(record->type, "filelists_db") && !ignore_sqlite)
            mdloc->fil_sqlite_href = full_location_href;
        else if (!g_strcmp0(record->type, "other"))
            mdloc->oth_xml_href = full_location_href;
        else if (!g_strcmp0(record->type, "other_db") && !ignore_sqlite)
            mdloc->oth_sqlite_href = full_location_href;
        else if (!g_strcmp0(record->type, "group"))
            mdloc->groupfile_href = full_location_href;
        else if (!g_strcmp0(record->type, "group_gz"))
            mdloc->cgroupfile_href = full_location_href;
        else if (!g_strcmp0(record->type, "updateinfo"))
            mdloc->updateinfo_href = full_location_href;
        else
            g_free(full_location_href);
    }

    cr_repomd_free(repomd);

    return mdloc;
}

static struct cr_MetadataLocation *
cr_get_local_metadata(const char *repopath, gboolean ignore_sqlite)
{
    _cleanup_free_ gchar *repomd = NULL;
    struct cr_MetadataLocation *ret = NULL;

    if (!repopath)
        return ret;

    if (!g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        g_warning("%s: %s is not a directory", __func__, repopath);
        return ret;
    }

    // Create path to repomd.xml and check if it exists
    repomd = g_build_filename(repopath, "repodata", "repomd.xml", NULL);
    if (!g_file_test(repomd, G_FILE_TEST_EXISTS)) {
        g_debug("%s: %s doesn't exists", __func__, repomd);
        return ret;
    }

    ret = cr_parse_repomd(repomd, repopath, ignore_sqlite);

    return ret;
}


static struct cr_MetadataLocation *
cr_get_remote_metadata(const char *repopath, gboolean ignore_sqlite)
{
    CURL *handle = NULL;
    _cleanup_free_ gchar *tmp_dir = NULL;
    _cleanup_free_ gchar *tmp_repodata = NULL;
    _cleanup_free_ gchar *tmp_repomd = NULL;
    _cleanup_free_ gchar *url = NULL;
    struct cr_MetadataLocation *r_location = NULL;
    struct cr_MetadataLocation *ret = NULL;
    _cleanup_error_free_ GError *tmp_err = NULL;

    if (!repopath)
        return ret;

    // Create temporary repo in /tmp
    tmp_dir = g_build_filename(g_get_tmp_dir(), TMPDIR_PATTERN, NULL);
    if(!mkdtemp(tmp_dir)) {
        g_critical("%s: Cannot create a temporary directory: %s",
                   __func__, g_strerror(errno));
        return ret;
    }

    g_debug("%s: Using tmp dir: %s", __func__, tmp_dir);

    // Create repodata subdir in tmp dir
    tmp_repodata = g_build_filename(tmp_dir, "repodata", NULL);
    if (g_mkdir (tmp_repodata, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
        g_critical("%s: Cannot create a temporary directory", __func__);
        return ret;
    }

    // Prepare temporary repomd.xml filename
    tmp_repomd = g_build_filename(tmp_repodata, "repomd.xml", NULL);

    // Prepare repomd.xml URL
    if (g_str_has_suffix(repopath, "/"))
        url = g_strconcat(repopath, "repodata/repomd.xml", NULL);
    else
        url = g_strconcat(repopath, "/repodata/repomd.xml", NULL);

    // Create and setup CURL handle
    handle = curl_easy_init();

    // Fail on HTTP error (return code >= 400)
    if (curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1) != CURLE_OK) {
        g_critical("%s: curl_easy_setopt(CURLOPT_FAILONERROR) error", __func__);
        goto get_remote_metadata_cleanup;
    }

    // Follow redirs
    if (curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1) != CURLE_OK) {
        g_critical("%s: curl_easy_setopt(CURLOPT_FOLLOWLOCATION) error", __func__);
        goto get_remote_metadata_cleanup;
    }

    // Maximal number of redirects
    if (curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 6) != CURLE_OK) {
        g_critical("%s: curl_easy_setopt(CURLOPT_MAXREDIRS) error", __func__);
        goto get_remote_metadata_cleanup;
    }

    // Download repomd.xml
    cr_download(handle, url, tmp_repomd, &tmp_err);
    if (tmp_err) {
        g_critical("%s: %s", __func__, tmp_err->message);
        goto get_remote_metadata_cleanup;
    }

    // Parse downloaded repomd.xml
    r_location = cr_parse_repomd(tmp_repomd, repopath, ignore_sqlite);
    if (!r_location) {
        g_critical("%s: repomd.xml parser failed on %s", __func__, tmp_repomd);
        goto get_remote_metadata_cleanup;
    }

    // Download all other repofiles
    if (r_location->pri_xml_href)
        cr_download(handle, r_location->pri_xml_href, tmp_repodata, &tmp_err);
    if (!tmp_err && r_location->fil_xml_href)
        cr_download(handle, r_location->fil_xml_href, tmp_repodata, &tmp_err);
    if (!tmp_err && r_location->oth_xml_href)
        cr_download(handle, r_location->oth_xml_href, tmp_repodata, &tmp_err);
    if (!tmp_err && r_location->pri_sqlite_href)
        cr_download(handle, r_location->pri_sqlite_href, tmp_repodata, &tmp_err);
    if (!tmp_err && r_location->fil_sqlite_href)
        cr_download(handle, r_location->fil_sqlite_href, tmp_repodata, &tmp_err);
    if (!tmp_err && r_location->oth_sqlite_href)
        cr_download(handle, r_location->oth_sqlite_href, tmp_repodata, &tmp_err);
    if (!tmp_err && r_location->groupfile_href)
        cr_download(handle, r_location->groupfile_href, tmp_repodata, &tmp_err);
    if (!tmp_err && r_location->cgroupfile_href)
        cr_download(handle, r_location->cgroupfile_href, tmp_repodata, &tmp_err);
    if (!tmp_err && r_location->updateinfo_href)
        cr_download(handle, r_location->updateinfo_href, tmp_repodata, &tmp_err);

    cr_metadatalocation_free(r_location);

    if (tmp_err) {
        g_critical("%s: Error while downloadig files: %s",
                   __func__, tmp_err->message);
        goto get_remote_metadata_cleanup;
    }

    g_debug("%s: Remote metadata was successfully downloaded", __func__);

    // Parse downloaded data
    ret = cr_get_local_metadata(tmp_dir, ignore_sqlite);
    if (ret)
        ret->tmp = 1;

get_remote_metadata_cleanup:

    if (handle)
        curl_easy_cleanup(handle);
    if (!ret) cr_remove_dir(tmp_dir, NULL);

    return ret;
}


// XXX: err is not really used in this function yet
struct cr_MetadataLocation *
cr_locate_metadata(const char *repopath, gboolean ignore_sqlite, GError **err)
{
    struct cr_MetadataLocation *ret = NULL;

    assert(repopath);
    assert(!err || *err == NULL);

    if (g_str_has_prefix(repopath, "ftp://") ||
        g_str_has_prefix(repopath, "http://") ||
        g_str_has_prefix(repopath, "https://"))
    {
        // Remote metadata - Download them via curl
        ret = cr_get_remote_metadata(repopath, ignore_sqlite);
    } else {
        // Local metadata
        if (g_str_has_prefix(repopath, "file://"))
            repopath += 7;
        ret = cr_get_local_metadata(repopath, ignore_sqlite);
    }

    if (ret)
        ret->original_url = g_strdup(repopath);

    // XXX
    if (!ret) {
        g_set_error(err, ERR_DOMAIN, CRE_NODIR,
                    "Cannot locate metadata");
        return NULL;
    }

    return ret;
}
