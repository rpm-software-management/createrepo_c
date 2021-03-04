/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2014  Tomas Mlcoch
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
#include <errno.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "helpers.h"
#include "error.h"
#include "misc.h"
#include "checksum.h"
#include "modifyrepo_shared.h"
#include "compression_wrapper.h"
#include "threads.h"
#include "xml_dump.h"
#include "locate_metadata.h"

#define ERR_DOMAIN      CREATEREPO_C_ERROR

typedef struct _old_file {
    time_t mtime;
    gchar  *path;
} OldFile;


static void
cr_free_old_file(gpointer data)
{
    OldFile *old_file = (OldFile *) data;
    g_free(old_file->path);
    g_free(old_file);
}


static gint
cr_cmp_old_repodata_files(gconstpointer a, gconstpointer b)
{
    if (((OldFile *) a)->mtime < ((OldFile *) b)->mtime)
        return 1;
    if (((OldFile *) a)->mtime > ((OldFile *) b)->mtime)
        return -1;
    return 0;
}

static void
cr_stat_and_insert(const gchar *dirname, const gchar *filename, GSList **list)
{
    struct stat buf;
    OldFile *old_file;
    gchar *path = g_strconcat(dirname, filename, NULL);
    if (stat(path, &buf) == -1)
        buf.st_mtime = 1;
    old_file = g_malloc0(sizeof(OldFile));
    old_file->mtime = buf.st_mtime;
    old_file->path  = path;
    *list = g_slist_insert_sorted(*list, old_file, cr_cmp_old_repodata_files);
}

/* List files that should be removed from the repo or not copied
 * to the new repo. (except the repomd.xml)
 */
static gboolean
cr_repodata_excludelist_classic(const char *repodata_path,
                              int retain,
                              GSList **excludelist,
                              GError **err)
{
    /* This piece of code implement the retain_old functionality in
     * the same way as original createrepo does.
     * The way is pretty stupid. Because:
     * - Old metadata are kept in the repodata/ but not referenced by
     *   repomd.xml
     * - Thus, old repodata are searched by its filename
     * - It manipulate only with primary, filelists, other and
     *   related databases.
     */

    /* By default, createrepo_c keeps (copy from the old repo
     * to the new repo) all files that are in the repodata/ directory
     * but are not referenced by the repomd.xml.
     *
     * But this hack appends to the old_basenames list a metadata
     * that should be ignored (that should not be copied to the
     * new repository).
     */

    GSList *pri_lst = NULL, *pri_db_lst = NULL;
    GSList *fil_lst = NULL, *fil_db_lst = NULL;
    GSList *oth_lst = NULL, *oth_db_lst = NULL;
    GSList **lists[] = { &pri_lst, &pri_db_lst,
                         &fil_lst, &fil_db_lst,
                         &oth_lst, &oth_db_lst };
    const int num_of_lists = CR_ARRAYLEN(lists);

    GDir *dirp = NULL;
    const gchar *filename;
    GError *tmp_err = NULL;

    assert(excludelist);
    assert(!err || *err == NULL);

    *excludelist = NULL;

    if (retain == -1) {
        // -1 means retain all - nothing to be excluded
        return TRUE;
    } else if (retain < 0) {
        // other negative values are error
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Number of retained old metadatas "
                    "must be integer number >= -1");
        return FALSE;
    }

    // Open the repodata/ directory
    dirp = g_dir_open (repodata_path, 0, &tmp_err);
    if (!dirp) {
        g_warning("Cannot open directory: %s: %s", repodata_path, tmp_err->message);
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot open directory: %s: %s",
                    repodata_path, tmp_err->message);
        g_error_free(tmp_err);
        return FALSE;
    }

    // Create sorted (by mtime) lists of old metadata files
    // More recent files are first
    while ((filename = g_dir_read_name (dirp))) {
        // Get filename without suffix
        gchar *name_without_suffix;
        gchar *lastdot = strrchr(filename, '.');
        if (!lastdot) continue;  // Filename doesn't contain '.'
        name_without_suffix = g_strndup(filename, (lastdot - filename));

        // XXX: This detection is pretty shitty, but it mimics
        // behaviour of original createrepo
        if (g_str_has_suffix(name_without_suffix, "primary.xml")) {
            cr_stat_and_insert(repodata_path, filename, &pri_lst);
        } else if (g_str_has_suffix(name_without_suffix, "primary.sqlite")) {
            cr_stat_and_insert(repodata_path, filename, &pri_db_lst);
        } else if (g_str_has_suffix(name_without_suffix, "filelists.xml")) {
            cr_stat_and_insert(repodata_path, filename, &fil_lst);
        } else if (g_str_has_suffix(name_without_suffix, "filelists.sqlite")) {
            cr_stat_and_insert(repodata_path, filename, &fil_db_lst);
        } else if (g_str_has_suffix(name_without_suffix, "other.xml")) {
            cr_stat_and_insert(repodata_path, filename, &oth_lst);
        } else if (g_str_has_suffix(name_without_suffix, "other.sqlite")) {
            cr_stat_and_insert(repodata_path, filename, &oth_db_lst);
        }
        g_free(name_without_suffix);
    }

    g_dir_close(dirp);
    dirp = NULL;

    // Append files to the excludelist
    for (int x = 0; x < num_of_lists; x++) {
        for (GSList *el = g_slist_nth(*(lists[x]), retain); el; el = g_slist_next(el)) {
            OldFile *of = (OldFile *) el->data;
            *excludelist = g_slist_prepend(*excludelist,
                                         g_path_get_basename(of->path));
        }
        // Free the list
        cr_slist_free_full(*(lists[x]), cr_free_old_file);
    }

    return TRUE;
}

/* List files that should be removed from the repo or not copied
 * to the new repo. (except the repomd.xml)
 * This function excludes all metadata files listed in repomd.xml
 * if retain == 0, otherwise it don't exclude any file
 */
static gboolean
cr_repodata_excludelist(const char *repodata_path,
                      int retain,
                      GSList **excludelist,
                      GError **err)
{
    gchar *old_repomd_path = NULL;
    cr_Repomd *repomd = NULL;
    GError *tmp_err = NULL;

    assert(excludelist);
    assert(!err || *err == NULL);

    *excludelist = NULL;

    if (retain == -1 || retain > 0) {
        // retain all - nothing to be excluded
        return TRUE;
    } else if (retain < 0) {
        // other negative values are error
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Number of retained old metadatas "
                    "must be integer number >= -1");
        return FALSE;
    }

    // Parse old repomd.xml
    old_repomd_path = g_build_filename(repodata_path, "repomd.xml", NULL);
    repomd = cr_repomd_new();
    cr_xml_parse_repomd(old_repomd_path, repomd, NULL, NULL, &tmp_err);
    if (tmp_err) {
        g_warning("Cannot parse repomd: %s", old_repomd_path);
        g_clear_error(&tmp_err);
        cr_repomd_free(repomd);
        repomd = cr_repomd_new();
    }
    g_free(old_repomd_path);

    // Parse the old repomd.xml and append its items
    // to the old_basenames list
    for (GSList *elem = repomd->records; elem; elem = g_slist_next(elem)) {
        cr_RepomdRecord *rec = elem->data;

        if (!rec->location_href) {
            // Ignore bad records (records without location_href)
            g_warning("Record without location href in old repo");
            continue;
        }

        if (rec->location_base) {
            // Ignore files with base location
            g_debug("Old repomd record with base location is ignored: "
                    "%s - %s", rec->location_base, rec->location_href);
            continue;
        }

        *excludelist = g_slist_prepend(*excludelist,
                                     g_path_get_basename(rec->location_href));
    }

    cr_repomd_free(repomd);
    return TRUE;
}

static gboolean
cr_repodata_excludelist_by_age(const char *repodata_path,
                             gint64 md_max_age,
                             GSList **excludelist,
                             GError **err)
{
    GDir *dirp = NULL;
    const gchar *filename;
    time_t current_time;
    GError *tmp_err = NULL;

    assert(excludelist);
    assert(!err || *err == NULL);

    *excludelist = NULL;

    if (md_max_age < 0) {
        // A negative value means retain all - nothing to be excluded
        return TRUE;
    }

    // Open the repodata/ directory
    dirp = g_dir_open (repodata_path, 0, &tmp_err);
    if (!dirp) {
        g_warning("Cannot open directory: %s: %s", repodata_path, tmp_err->message);
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot open directory: %s: %s",
                    repodata_path, tmp_err->message);
        g_error_free(tmp_err);
        return FALSE;
    }

    current_time = time(NULL);

    // Create sorted (by mtime) lists of old metadata files
    // More recent files are first
    while ((filename = g_dir_read_name (dirp))) {
        struct stat buf;
        gchar *fullpath = g_strconcat(repodata_path, filename, NULL);
        if (stat(fullpath, &buf) == -1) {
            g_warning("Cannot stat %s", fullpath);
            g_free(fullpath);
            continue;
        }
        g_free(fullpath);

        // Check file age (current_time - mtime)
        gint64 age = difftime(current_time, buf.st_mtime);
        if (age <= md_max_age)
            continue;  // The file is young

        // Debug
        g_debug("File is too old (%"G_GINT64_FORMAT" > %"G_GINT64_FORMAT") %s",
                age, md_max_age, filename);

        // Add the file to the excludelist
        *excludelist = g_slist_prepend(*excludelist, g_strdup(filename));
    }

    g_dir_close(dirp);
    return TRUE;
}

int
cr_remove_metadata_classic(const char *repopath, int retain, GError **err)
{
    int rc = CRE_OK;
    gboolean ret = TRUE;
    gchar *full_repopath = NULL;
    GSList *excludelist = NULL;
    GDir *dirp = NULL;
    const gchar *filename;
    GError *tmp_err = NULL;

    assert(repopath);
    assert(!err || *err == NULL);

    full_repopath = g_strconcat(repopath, "/repodata/", NULL);

    // Get list of files that should be deleted
    ret = cr_repodata_excludelist_classic(full_repopath, retain, &excludelist, err);
    if (!ret)
        return FALSE;

    // Always remove repomd.xml
    excludelist = g_slist_prepend(excludelist, g_strdup("repomd.xml"));

    // Open the repodata/ directory
    dirp = g_dir_open(full_repopath, 0, &tmp_err);
    if (tmp_err) {
        g_debug("%s: Path %s doesn't exist", __func__, repopath);
        g_propagate_prefixed_error(err, tmp_err, "Cannot open a dir: ");
        rc = CRE_IO;
        goto cleanup;
    }

    // Iterate over the files in the repository and remove all files
    // that are listed on excludelist
    while ((filename = g_dir_read_name(dirp))) {
        gchar *full_path;

        if (!g_slist_find_custom(excludelist, filename, (GCompareFunc) g_strcmp0))
            // The filename is not excluded, skip it
            continue;

        full_path = g_strconcat(full_repopath, filename, NULL);

        // REMOVE
        // TODO: Use more sophisticated function
        if (g_remove(full_path) != -1)
            g_debug("Removed %s", full_path);
        else
            g_warning("Cannot remove %s: %s", full_path, g_strerror(errno));

        g_free(full_path);
    }

cleanup:

    cr_slist_free_full(excludelist, g_free);
    g_free(full_repopath);
    if (dirp)
        g_dir_close(dirp);

    return rc;
}

gboolean
cr_old_metadata_retention(const char *old_repo,
                          const char *new_repo,
                          cr_RetentionType type,
                          gint64 val,
                          GError **err)
{
    gboolean ret = TRUE;
    GSList *excludelist = NULL;
    GDir *dirp = NULL;
    const gchar *filename;
    GError *tmp_err = NULL;

    assert(!err || *err == NULL);

    if (!g_file_test(old_repo, G_FILE_TEST_EXISTS))
        return TRUE;

    g_debug("Copying files from old repository to the new one");

    // Get list of file that should be skiped during copying
    g_debug("Retention type: %d (%"G_GINT64_FORMAT")", type, val);
    if (type == CR_RETENTION_BYAGE)
        ret = cr_repodata_excludelist_by_age(old_repo, val, &excludelist, err);
    else if (type == CR_RETENTION_COMPATIBILITY)
        ret = cr_repodata_excludelist_classic(old_repo, (int) val, &excludelist, err);
    else // CR_RETENTION_DEFAULT
        ret = cr_repodata_excludelist(old_repo, (int) val, &excludelist, err);

    if (!ret)
        return FALSE;

    // Never copy old repomd.xml to the new repository
    excludelist = g_slist_prepend(excludelist, g_strdup("repomd.xml"));

    // Open directory with old repo
    dirp = g_dir_open (old_repo, 0, &tmp_err);
    if (!dirp) {
        g_warning("Cannot open directory: %s: %s", old_repo, tmp_err->message);
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Cannot open directory: %s: %s",
                    old_repo, tmp_err->message);
        g_error_free(tmp_err);
        ret = FALSE;
        goto exit;
    }

    // Iterate over the files in the old repository and copy all
    // that are not listed on excludelist
    while ((filename = g_dir_read_name(dirp))) {
        if (g_slist_find_custom(excludelist, filename, (GCompareFunc) g_strcmp0)) {
            g_debug("Excluded: %s", filename);
            continue;
        }

        gchar *full_path = g_strconcat(old_repo, filename, NULL);
        gchar *new_full_path = g_strconcat(new_repo, filename, NULL);

        // Do not override new file with the old one
        if (g_file_test(new_full_path, G_FILE_TEST_EXISTS)) {
            g_debug("Skipped copy: %s -> %s (file already exists)",
                    full_path, new_full_path);
            g_free(full_path);
            g_free(new_full_path);
            continue;
        }

        // COPY!
        cr_cp(full_path,
              new_full_path,
              CR_CP_RECURSIVE|CR_CP_PRESERVE_ALL,
              NULL,
              &tmp_err);

        if (tmp_err) {
            g_warning("Cannot copy %s -> %s: %s",
                      full_path, new_full_path, tmp_err->message);
            g_clear_error(&tmp_err);
        } else {
            g_debug("Copied %s -> %s", full_path, new_full_path);
        }

        g_free(full_path);
        g_free(new_full_path);
    }

exit:

    // Cleanup
    cr_slist_free_full(excludelist, g_free);
    if (dirp)
        g_dir_close(dirp);

    return ret;
}


