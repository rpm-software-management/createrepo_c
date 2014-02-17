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
#include <assert.h>
#include "error.h"
#include "misc.h"
#include "checksum.h"
#include "modifyrepo_shared.h"
#include "compression_wrapper.h"
#include "threads.h"
#include "xml_dump.h"
#include "locate_metadata.h"

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

static int
cr_remove_listed_files(GSList *list, int retain, GError **err)
{
    int removed = 0;

    assert(!err || *err == NULL);

    if (retain < 0) retain = 0;

    for (GSList *el = g_slist_nth(list, retain); el; el = g_slist_next(el)) {
        OldFile *of = (OldFile *) el->data;
        g_debug("%s: Removing: %s", __func__, of->path);
        if (g_remove(of->path) != -1) {
            ++removed;
        } else {
            g_warning("%s: Cannot remove %s", __func__, of->path);
            g_set_error(err, CR_LOCATE_METADATA_ERROR, CRE_IO,
                        "Cannot remove %s: %s", of->path, strerror(errno));
            break;
        }
    }

    return removed;
}

// Return list of non-null pointers on strings in the passed structure
static GSList *
cr_get_list_of_md_locations(struct cr_MetadataLocation *ml)
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

static void
cr_free_list_of_md_locations(GSList *list)
{
    if (list) {
        g_slist_free(list);
    }
}

int
cr_remove_metadata_classic(const char *repopath, int retain, GError **err)
{
    gchar *full_repopath, *repomd_path;
    GDir *repodir;
    const gchar *file;
    GSList *pri_lst = NULL, *pri_db_lst = NULL;
    GSList *fil_lst = NULL, *fil_db_lst = NULL;
    GSList *oth_lst = NULL, *oth_db_lst = NULL;
    GError *tmp_err = NULL;

    assert(repopath);
    assert(!err || *err == NULL);

    if (!g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        g_debug("%s: Cannot remove %s", __func__, repopath);
        g_set_error(err, CR_LOCATE_METADATA_ERROR, CRE_NODIR,
                    "Directory %s doesn't exist", repopath);
        return CRE_NODIR;
    }

    full_repopath = g_strconcat(repopath, "/repodata/", NULL);
    repodir = g_dir_open(full_repopath, 0, &tmp_err);
    if (tmp_err) {
        g_debug("%s: Path %s doesn't exist", __func__, repopath);
        g_propagate_prefixed_error(err, tmp_err, "Cannot open a dir: ");
        g_free(full_repopath);
        return CRE_IO;
    }


    // Create sorted (by mtime) lists of old metadata files
    // More recent files are first

    while ((file = g_dir_read_name (repodir))) {
        // Get filename without suffix
        gchar *name_without_suffix;
        gchar *lastdot = strrchr(file, '.');
        if (!lastdot) continue;  // Filename doesn't contain '.'
        name_without_suffix = g_strndup(file, (lastdot - file));

        if (g_str_has_suffix(name_without_suffix, "primary.xml")) {
            cr_stat_and_insert(full_repopath, file, &pri_lst);
        } else if (g_str_has_suffix(name_without_suffix, "primary.sqlite")) {
            cr_stat_and_insert(full_repopath, file, &pri_db_lst);
        } else if (g_str_has_suffix(name_without_suffix, "filelists.xml")) {
            cr_stat_and_insert(full_repopath, file, &fil_lst);
        } else if (g_str_has_suffix(name_without_suffix, "filelists.sqlite")) {
            cr_stat_and_insert(full_repopath, file, &fil_db_lst);
        } else if (g_str_has_suffix(name_without_suffix, "other.xml")) {
            cr_stat_and_insert(full_repopath, file, &oth_lst);
        } else if (g_str_has_suffix(name_without_suffix, "other.sqlite")) {
            cr_stat_and_insert(full_repopath, file, &oth_db_lst);
        }
        g_free(name_without_suffix);
    }

    g_dir_close(repodir);

    // Remove old metadata

    int ret = CRE_OK;
    GSList *lists[] = { pri_lst, pri_db_lst,
                        fil_lst, fil_db_lst,
                        oth_lst, oth_db_lst };


    // Remove repomd.xml

    repomd_path = g_strconcat(full_repopath, "repomd.xml", NULL);

    g_debug("%s: Removing: %s", __func__, repomd_path);
    if (g_remove(repomd_path) == -1) {
        g_set_error(err, CR_LOCATE_METADATA_ERROR, CRE_IO,
                    "Cannot remove %s: %s", repomd_path, strerror(errno));
        ret = CRE_IO;
        goto cleanup;
    }


    // Remove listed files

    for (int x = 0; x < 6; x++) {
        cr_remove_listed_files(lists[x], retain, &tmp_err);
        if (tmp_err) {
            ret = tmp_err->code;
            g_propagate_error(err, tmp_err);
            break;
        }
    }

cleanup:
    g_free(repomd_path);
    g_free(full_repopath);

    for (int x = 0; x < 6; x++)
        cr_slist_free_full(lists[x], cr_free_old_file);

    return ret;
}

int
cr_remove_metadata(const char *repopath, GError **err)
{
    // XXX: TODO: This function is pretty old and ugly, refactore it

    int removed_files = 0;
    gchar *full_repopath;
    const gchar *file;
    GDir *repodir;
    struct cr_MetadataLocation *ml;
    GError *tmp_err = NULL;

    assert(repopath);
    assert(!err || *err == NULL);

    if (!g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        g_debug("%s: remove_old_metadata: Cannot remove %s", __func__, repopath);
        g_set_error(err, CR_LOCATE_METADATA_ERROR, CRE_NODIR,
                    "Directory %s doesn't exists", repopath);
        return -1;
    }

    full_repopath = g_strconcat(repopath, "/repodata/", NULL);

    repodir = g_dir_open(full_repopath, 0, &tmp_err);
    if (tmp_err) {
        g_debug("%s: Path %s doesn't exists", __func__, repopath);
        g_set_error(err, CR_LOCATE_METADATA_ERROR, CRE_IO,
                    "Cannot open directory %s: %s", repopath, strerror(errno));
        g_free(full_repopath);
        return -1;
    }


    // Remove all metadata listed in repomd.xml

    ml = cr_locate_metadata(repopath, 0, &tmp_err);
    if (tmp_err) {
        g_propagate_error(err, tmp_err);
        goto cleanup;
    }

    if (ml) {
        GSList *list = cr_get_list_of_md_locations(ml);
        GSList *element;

        for (element=list; element; element=element->next) {
            gchar *path = (char *) element->data;

            g_debug("%s: Removing: %s (path obtained from repomd.xml)", __func__, path);
            if (g_remove(path) != -1) {
                removed_files++;
            } else {
                g_warning("%s: Cannot remove %s", __func__, path);
                g_set_error(err, CR_LOCATE_METADATA_ERROR, CRE_IO,
                            "Cannot remove %s: %s", path, strerror(errno));
                cr_free_list_of_md_locations(list);
                cr_metadatalocation_free(ml);
                goto cleanup;
            }
        }

        cr_free_list_of_md_locations(list);
        cr_metadatalocation_free(ml);
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
            g_debug("%s: Removing: %s", __func__, path);
            if (g_remove(path) != -1) {
                removed_files++;
            } else {
                g_warning("%s: Cannot remove %s", __func__, path);
                g_set_error(err, CR_LOCATE_METADATA_ERROR, CRE_IO,
                            "Cannot remove %s: %s", path, strerror(errno));
                g_free(path);
                goto cleanup;
            }
        }
    }

cleanup:
    g_dir_close(repodir);
    g_free(full_repopath);

    return removed_files;
}

gboolean
cr_old_metadata_retention(const char *old_repo,
                          const char *new_repo,
                          int retain_old,
                          GError **err)
{
    gboolean ret = TRUE;
    GDir *dirp = NULL;
    const gchar *filename;
    GSList *old_basenames = NULL; /*!< List of basenames that will be skipped
                                       during copying from the old repo to the
                                       new one. */
    cr_Repomd *repomd = NULL;
    gchar *old_repomd_path = NULL;
    GError *tmp_err = NULL;

    assert(!err || *err == NULL);

    if (!g_file_test(old_repo, G_FILE_TEST_EXISTS))
        return TRUE;

    g_debug("Copying files from old repository to the new one");

    // Parse old repomd.xml
    old_repomd_path = g_build_filename(old_repo, "repomd.xml", NULL);
    repomd = cr_repomd_new();
    cr_xml_parse_repomd(old_repomd_path, repomd, NULL, NULL, &tmp_err);
    if (tmp_err) {
        g_warning("Cannot parse repomd: %s", old_repomd_path);
        g_clear_error(&tmp_err);
        cr_repomd_free(repomd);
        repomd = cr_repomd_new();
    }
    g_free(old_repomd_path);

    // repomd.xml skip always
    old_basenames = g_slist_prepend(old_basenames, g_strdup("repomd.xml"));

    // from the repomd.xml select metadata that will not be copied
    if (retain_old == 0) {
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

            // XXX: TODO: Remove in future -------------------------------
            // Currently, add to the list only primary, filelists, other
            // and its databases. For now (for compatibility)
            if (g_strcmp0(rec->type, "primary")
                && g_strcmp0(rec->type, "primary_db")
                && g_strcmp0(rec->type, "filelists")
                && g_strcmp0(rec->type, "filelists_db")
                && g_strcmp0(rec->type, "other")
                && g_strcmp0(rec->type, "other_db"))
            {
                // If the record's type is not one of the above mentioned
                // skip removing this metadata
                continue;
            }
            // XXX: TODO: Remove in future END ---------------------------

            old_basenames = g_slist_prepend(old_basenames,
                                    g_path_get_basename(rec->location_href));
        }
    }

    { // XXX: START
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
        GSList *lists[] = { pri_lst, pri_db_lst,
                            fil_lst, fil_db_lst,
                            oth_lst, oth_db_lst };
        const int num_of_lists = CR_ARRAYLEN(lists);

        dirp = g_dir_open (old_repo, 0, &tmp_err);
        if (!dirp) {
            g_warning("Cannot open directory: %s: %s", old_repo, tmp_err->message);
            g_set_error(err, CR_HELPER_ERROR, CRE_IO,
                        "Cannot open directory: %s: %s",
                        old_repo, tmp_err->message);
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

            if (g_str_has_suffix(name_without_suffix, "primary.xml")) {
                cr_stat_and_insert(old_repo, filename, &pri_lst);
            } else if (g_str_has_suffix(name_without_suffix, "primary.sqlite")) {
                cr_stat_and_insert(old_repo, filename, &pri_db_lst);
            } else if (g_str_has_suffix(name_without_suffix, "filelists.xml")) {
                cr_stat_and_insert(old_repo, filename, &fil_lst);
            } else if (g_str_has_suffix(name_without_suffix, "filelists.sqlite")) {
                cr_stat_and_insert(old_repo, filename, &fil_db_lst);
            } else if (g_str_has_suffix(name_without_suffix, "other.xml")) {
                cr_stat_and_insert(old_repo, filename, &oth_lst);
            } else if (g_str_has_suffix(name_without_suffix, "other.sqlite")) {
                cr_stat_and_insert(old_repo, filename, &oth_db_lst);
            }
            g_free(name_without_suffix);
        }

        g_dir_close(dirp);
        dirp = NULL;

        for (int x = 0; x < num_of_lists; x++) {
            ; // TODO
        }

    } // XXX: END

    // Iterate over the files in the old repository and copy all
    // that aren't listed in repomd.xml
    dirp = g_dir_open (old_repo, 0, &tmp_err);
    if (!dirp) {
        g_warning("Cannot open directory: %s: %s", old_repo, tmp_err->message);
        g_set_error(err, CR_HELPER_ERROR, CRE_IO,
                    "Cannot open directory: %s: %s",
                    old_repo, tmp_err->message);
        g_error_free(tmp_err);
        ret = FALSE;
        goto exit;
    }

    while ((filename = g_dir_read_name(dirp))) {

        if (g_slist_find_custom(old_basenames, filename, (GCompareFunc) g_strcmp0)) {
            // This file is listed in repomd.xml, do not copy it
            g_debug("Skipped file %s", filename);
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
        }

        g_free(full_path);
        g_free(new_full_path);
    }

exit:

    // Cleanup
    g_slist_free_full(old_basenames, g_free);
    cr_repomd_free(repomd);
    if (dirp)
        g_dir_close(dirp);

    return ret;
}


