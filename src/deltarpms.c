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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <drpm.h>
#include "deltarpms.h"
#include "package.h"
#include "misc.h"
#include "error.h"

#define MAKEDELTARPM    "/usr/bin/makedeltarpm"


gboolean
cr_drpm_support(void)
{
    if (!g_file_test(MAKEDELTARPM, G_FILE_TEST_IS_REGULAR
                                   | G_FILE_TEST_IS_EXECUTABLE))
        return FALSE;
    return TRUE;
}


char *
cr_drpm_create(cr_DeltaTargetPackage *old,
               cr_DeltaTargetPackage *new,
               const char *destdir,
               GError **err)
{
    gchar *drpmfn, *drpmpath, *error_str = NULL;
    GPtrArray *cmd_array;
    int spawn_flags = G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL;
    GError *tmp_err = NULL;
    gint status = 0;
    gboolean ret;

    drpmfn = g_strdup_printf("%s-%s-%s_%s-%s.%s.drpm",
                             old->name, old->version, old->release,
                             new->version, new->release, old->arch);
    drpmpath = g_build_filename(destdir, drpmfn, NULL);
    g_free(drpmfn);

    cmd_array = g_ptr_array_new();
    g_ptr_array_add(cmd_array, MAKEDELTARPM);
    g_ptr_array_add(cmd_array, (gpointer) old->path);
    g_ptr_array_add(cmd_array, (gpointer) new->path);
    g_ptr_array_add(cmd_array, (gpointer) drpmpath);
    g_ptr_array_add(cmd_array, (gpointer) NULL);

    g_spawn_sync(NULL,              // working directory
                 (char **) cmd_array->pdata,  // argv
                 NULL,              // envp
                 spawn_flags,       // spawn flags
                 NULL,              // child setup function
                 NULL,              // user data for child setup
                 NULL,              // stdout
                 &error_str,        // stderr
                 &status,           // status
                 &tmp_err           // err
                );

    g_ptr_array_free(cmd_array, TRUE);

    if (tmp_err) {
        g_free(error_str);
        free(drpmpath);
        g_propagate_error(err, tmp_err);
        return NULL;
    }

    ret = cr_spawn_check_exit_status(status, &tmp_err);
    if (!ret) {
        g_propagate_prefixed_error(err, tmp_err, "%s: ", error_str);
        free(drpmpath);
        g_free(error_str);
        return NULL;
    }

    g_free(error_str);

    return drpmpath;
}

void
cr_deltapackage_free(cr_DeltaPackage *deltapackage)
{
    if (!deltapackage)
        return;
    cr_package_free(deltapackage->package);
    g_string_chunk_free(deltapackage->chunk);
    g_free(deltapackage);
}

cr_DeltaPackage *
cr_deltapackage_from_drpm_base(const char *filename,
                               int changelog_limit,
                               cr_HeaderReadingFlags flags,
                               GError **err)
{
    struct drpm *delta = NULL;
    cr_DeltaPackage *deltapackage = NULL;
    char *str;

    assert(!err || *err == NULL);

    deltapackage = g_new0(cr_DeltaPackage, 1);
    deltapackage->chunk = g_string_chunk_new(0);

    deltapackage->package = cr_package_from_rpm_base(filename,
                                                     changelog_limit,
                                                     flags,
                                                     err);
    if (!deltapackage->package)
        goto errexit;

    if (drpm_read(filename, &delta) != EOK) {
        g_set_error(err, CR_DELTARPMS_ERROR, CRE_DELTARPM,
                    "Deltarpm cannot read %s", filename);
        goto errexit;
    }

    if (drpm_get_string(delta, DRPM_SOURCE_NEVR, &str) != EOK) {
        g_set_error(err, CR_DELTARPMS_ERROR, CRE_DELTARPM,
                    "Deltarpm cannot read source NEVR from %s", filename);
        goto errexit;
    }

    deltapackage->nevr = cr_safe_string_chunk_insert_null(
                                    deltapackage->chunk, str);

    if (drpm_get_string(delta, DRPM_SEQUENCE, &str) != EOK) {
        g_set_error(err, CR_DELTARPMS_ERROR, CRE_DELTARPM,
                    "Deltarpm cannot read delta sequence from %s", filename);
        goto errexit;
    }

    deltapackage->sequence = cr_safe_string_chunk_insert_null(
                                    deltapackage->chunk, str);

    drpm_destroy(&delta);

    return deltapackage;

errexit:

    drpm_destroy(&delta);
    cr_deltapackage_free(deltapackage);

    return NULL;
}


static void
cr_free_gslist_of_strings(gpointer list)
{
    if (!list) return;
    cr_slist_free_full((GSList *) list, (GDestroyNotify) g_free);
}

/*
 * 1) Scanning for old candidate rpms
 */

GHashTable *
cr_scan_oldpackagedirs(GSList *oldpackagedirs,
                       gint64 max_delta_rpm_size,
                       GError **err)
{
    GHashTable *ht = NULL;

    assert(!err || *err == NULL);

    ht = g_hash_table_new_full(g_str_hash,
                               g_str_equal,
                               (GDestroyNotify) g_free,
                               (GDestroyNotify) cr_free_gslist_of_strings);

    for (GSList *elem = oldpackagedirs; elem; elem = g_slist_next(elem)) {
        gchar *dirname = elem->data;
        const gchar *filename;
        GDir *dirp;
        GSList *filenames = NULL;

        dirp = g_dir_open(dirname, 0, NULL);
        if (!dirp) {
            g_warning("Cannot open directory %s", dirname);
            continue;
        }

        while ((filename = g_dir_read_name(dirp))) {
            gchar *full_path;
            struct stat st;

            if (!g_str_has_suffix(filename, ".rpm"))
                continue;  // Skip non rpm files

            full_path = g_build_filename(dirname, filename, NULL);

            if (stat(full_path, &st) == -1) {
                g_warning("Cannot stat %s: %s", full_path, strerror(errno));
                g_free(full_path);
                continue;
            }

            if (st.st_size > max_delta_rpm_size) {
                g_debug("%s: Skipping %s that is > max_delta_rpm_size",
                        __func__, full_path);
                g_free(full_path);
                continue;
            }

            g_free(full_path);

            filenames = g_slist_prepend(filenames, g_strdup(filename));
        }

        if (filenames) {
            g_hash_table_replace(ht,
                                 (gpointer) g_strdup(dirname),
                                 (gpointer) filenames);
        }

        g_dir_close(dirp);
    }


    return ht;
}

/*
 * 2) Parallel delta generation
 */


typedef struct {
    cr_DeltaTargetPackage *tpkg;
} cr_DeltaTask;


typedef struct {
    const char *outdeltadir;
    gint num_deltas;
    GHashTable *oldpackages;
    GMutex *active_work_size_mutex;
    gint64 active_work_size;
    gint active_tasks;
    GCond *cond_task_finished;
} cr_DeltaThreadUserData;


static gint
cmp_deltatargetpackage_evr(gconstpointer aa, gconstpointer bb)
{
    const cr_DeltaTargetPackage *a = aa;
    const cr_DeltaTargetPackage *b = bb;

    return cr_cmp_evr(a->epoch, a->version, a->release,
                      b->epoch, b->version, b->release);
}


static void
cr_delta_thread(gpointer data, gpointer udata)
{
    cr_DeltaTask *task = data;
    cr_DeltaThreadUserData *user_data = udata;
    cr_DeltaTargetPackage *tpkg = task->tpkg;  // Shortcut

    GHashTableIter iter;
    gpointer key, value;

    // Iterate through specified oldpackage directories
    g_hash_table_iter_init(&iter, user_data->oldpackages);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        gchar *dirname = key;
        GSList *local_candidates = NULL;

        // Select appropriate candidates from the directory
        for (GSList *elem = value; elem; elem = g_slist_next(elem)) {
            gchar *filename = elem->data;
            if (g_str_has_prefix(filename, tpkg->name)) {
                cr_DeltaTargetPackage *l_tpkg;
                gchar *path = g_build_filename(dirname, filename, NULL);
                l_tpkg = cr_deltatargetpackage_from_rpm(path, NULL);
                g_free(path);
                if (!l_tpkg)
                    continue;

                // Check the candidate more carefully

                if (g_strcmp0(tpkg->name, l_tpkg->name)) {
                    cr_deltatargetpackage_free(l_tpkg);
                    continue;
                }

                if (g_strcmp0(tpkg->arch, l_tpkg->arch)) {
                    cr_deltatargetpackage_free(l_tpkg);
                    continue;
                }

                if (cr_cmp_evr(tpkg->epoch, tpkg->version, tpkg->release,
                        l_tpkg->epoch, l_tpkg->version, l_tpkg->release) <= 0)
                {
                    cr_deltatargetpackage_free(l_tpkg);
                    continue;
                }

                // This candidate looks good
                local_candidates = g_slist_prepend(local_candidates, l_tpkg);
            }
        }

        // Sort the candidates
        local_candidates = g_slist_sort(local_candidates,
                                        cmp_deltatargetpackage_evr);
        local_candidates = g_slist_reverse(local_candidates);

        // Generate deltas
        int x = 0;
        for (GSList *lelem = local_candidates; lelem; lelem = g_slist_next(lelem)){
            GError *tmp_err = NULL;
            cr_DeltaTargetPackage *old = lelem->data;

            g_debug("Generating delta %s -> %s", old->path, tpkg->path);
            cr_drpm_create(old, tpkg, user_data->outdeltadir, &tmp_err);
            if (tmp_err) {
                g_warning("Cannot generate delta %s -> %s : %s",
                          old->path, tpkg->path, tmp_err->message);
                g_error_free(tmp_err);
                continue;
            }
            if (++x == user_data->num_deltas)
                break;
        }

        for (GSList *lelem = local_candidates; lelem; lelem = g_slist_next(lelem)){
            cr_DeltaTargetPackage *c_tpkg = lelem->data;
            printf(">>> %s - %s.%s\n", c_tpkg->name, c_tpkg->version, c_tpkg->release);
        }
    }

    printf("Delta for \"%s\" (%"G_GINT64_FORMAT") generated\n",
           tpkg->name, tpkg->size_installed);

    g_mutex_lock(user_data->active_work_size_mutex);
    user_data->active_work_size -= tpkg->size_installed;
    user_data->active_tasks--;
    g_cond_signal(user_data->cond_task_finished);
    g_mutex_unlock(user_data->active_work_size_mutex);
}


static gint
cmp_deltatargetpackage_sizes(gconstpointer a, gconstpointer b)
{
    const cr_DeltaTargetPackage *dtpk_a = a;
    const cr_DeltaTargetPackage *dtpk_b = b;

    if (dtpk_a->size_installed < dtpk_b->size_installed)
        return -1;
    else if (dtpk_a->size_installed == dtpk_b->size_installed)
        return 0;
    else
        return 1;
}


gboolean
cr_parallel_deltas(GSList *targetpackages,
                   GHashTable *oldpackages,
                   const char *outdeltadir,
                   gint num_deltas,
                   gint workers,
                   gint64 max_delta_rpm_size,
                   gint64 max_work_size,
                   GError **err)
{
    GThreadPool *pool;
    cr_DeltaThreadUserData user_data;
    GList *targets = NULL;
    GError *tmp_err = NULL;

    assert(!err || *err == NULL);

    if (num_deltas < 1)
        return TRUE;

    if (workers < 1) {
        g_set_error(err, CR_DELTARPMS_ERROR, CRE_DELTARPM,
                    "Number of delta workers must be a positive integer number");
        return FALSE;
    }

    // Init user_data
    user_data.outdeltadir               = outdeltadir;
    user_data.num_deltas                = num_deltas;
    user_data.oldpackages               = oldpackages;
    user_data.active_work_size_mutex    = g_mutex_new();
    user_data.active_work_size          = 0;
    user_data.active_tasks              = 0;
    user_data.cond_task_finished        = g_cond_new();

    // Make sorted list of targets without packages
    // that are bigger then max_delta_rpm_size
    for (GSList *elem = targetpackages; elem; elem = g_slist_next(elem)) {
        cr_DeltaTargetPackage *tpkg = elem->data;
        if (tpkg->size_installed < max_delta_rpm_size)
            targets = g_list_insert_sorted(targets, tpkg, cmp_deltatargetpackage_sizes);
    }
    targets = g_list_reverse(targets);

    // Setup the pool of workers
    pool = g_thread_pool_new(cr_delta_thread,
                             &user_data,
                             workers,
                             TRUE,
                             &tmp_err);
    if (tmp_err) {
        g_propagate_prefixed_error(err, tmp_err, "Cannot create delta pool: ");
        return FALSE;
    }

    for (GList *elem = targets; elem; elem = g_list_next(elem)) {
        cr_DeltaTargetPackage *tpkg = elem->data;
    }

    // Push tasks into the pool
    while (targets) {
        gboolean inserted = FALSE;
        gint64 active_work_size;
        gint64 active_tasks;

        g_mutex_lock(user_data.active_work_size_mutex);
        while (user_data.active_tasks == workers)
            // Wait if all available threads are busy
            g_cond_wait(user_data.cond_task_finished, user_data.active_work_size_mutex);
        active_work_size = user_data.active_work_size;
        active_tasks = user_data.active_tasks;
        g_mutex_unlock(user_data.active_work_size_mutex);

        for (GList *elem = targets; elem; elem = g_list_next(elem)) {
            cr_DeltaTargetPackage *tpkg = elem->data;
            if ((active_work_size + tpkg->size_installed) <= max_work_size) {
                cr_DeltaTask *task = g_new0(cr_DeltaTask, 1);
                task->tpkg = tpkg;

                g_mutex_lock(user_data.active_work_size_mutex);
                user_data.active_work_size += tpkg->size_installed;
                user_data.active_tasks++;
                g_mutex_unlock(user_data.active_work_size_mutex);

                g_thread_pool_push(pool, task, NULL);
                targets = g_list_delete_link(targets, elem);
                inserted = TRUE;
                break;
            }
        }

        if (!inserted) {
            // In this iteration, no task was pushed to the pool
            g_mutex_lock(user_data.active_work_size_mutex);
            while (user_data.active_tasks == active_tasks)
                // Wait until any of running tasks finishes
                g_cond_wait(user_data.cond_task_finished, user_data.active_work_size_mutex);
            g_mutex_unlock(user_data.active_work_size_mutex);
        }
    }

    g_thread_pool_free(pool, FALSE, TRUE);
    g_list_free(targets);
}


cr_DeltaTargetPackage *
cr_deltatargetpackage_from_package(cr_Package *pkg,
                                   const char *path,
                                   GError **err)
{
    cr_DeltaTargetPackage *tpkg;

    assert(pkg);
    assert(!err || *err == NULL);

    tpkg = g_new0(cr_DeltaTargetPackage, 1);
    tpkg->chunk = g_string_chunk_new(0);
    tpkg->name = cr_safe_string_chunk_insert(tpkg->chunk, pkg->name);
    tpkg->arch = cr_safe_string_chunk_insert(tpkg->chunk, pkg->arch);
    tpkg->epoch = cr_safe_string_chunk_insert(tpkg->chunk, pkg->epoch);
    tpkg->version = cr_safe_string_chunk_insert(tpkg->chunk, pkg->version);
    tpkg->release = cr_safe_string_chunk_insert(tpkg->chunk, pkg->release);
    tpkg->location_href = cr_safe_string_chunk_insert(tpkg->chunk, pkg->location_href);
    tpkg->size_installed = pkg->size_installed;
    tpkg->path = cr_safe_string_chunk_insert(tpkg->chunk, path);

    return tpkg;
}


cr_DeltaTargetPackage *
cr_deltatargetpackage_from_rpm(const char *path, GError **err)
{
    cr_Package *pkg;
    cr_DeltaTargetPackage *tpkg;

    assert(!err || *err == NULL);

    pkg = cr_package_from_rpm_base(path, 0, 0, err);
    if (!pkg)
        return NULL;

    tpkg = cr_deltatargetpackage_from_package(pkg, path, err);
    cr_package_free(pkg);
    return tpkg;
}


void
cr_deltatargetpackage_free(cr_DeltaTargetPackage *tpkg)
{
    if (!tpkg)
        return;
    g_string_chunk_free(tpkg->chunk);
    g_free(tpkg);
}


// TODO: Make it recursive (?)
GSList *
cr_scan_targetdir(const char *dirname,
                  gint64 max_delta_rpm_size,
                  GError **err)
{
    GSList *targets = NULL;
    GDir *dirp;
    const gchar *filename;

    assert(!err || *err == NULL);

    dirp = g_dir_open(dirname, 0, NULL);
    if (!dirp) {
        g_warning("Cannot open directory %s", dirname);
        return NULL;
    }

    while ((filename = g_dir_read_name(dirp))) {
        gchar *full_path;
        struct stat st;
        cr_DeltaTargetPackage *tpkg;

        if (!g_str_has_suffix(filename, ".rpm"))
            continue;  // Skip non rpm files

        full_path = g_build_filename(dirname, filename, NULL);

        if (stat(full_path, &st) == -1) {
            g_warning("Cannot stat %s: %s", full_path, strerror(errno));
            g_free(full_path);
            continue;
        }

        if (st.st_size > max_delta_rpm_size) {
            g_debug("%s: Skipping %s that is > max_delta_rpm_size",
                    __func__, full_path);
            g_free(full_path);
            continue;
        }

        tpkg = cr_deltatargetpackage_from_rpm(full_path, NULL);
        if (tpkg)
            targets = g_slist_prepend(targets, tpkg);
        g_free(full_path);
    }

    g_dir_close(dirp);

    return targets;
}

