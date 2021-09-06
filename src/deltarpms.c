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
#include "deltarpms.h"
#ifdef    CR_DELTA_RPM_SUPPORT
#include <drpm.h>
#endif
#include "package.h"
#include "parsepkg.h"
#include "misc.h"
#include "error.h"


#define ERR_DOMAIN      CREATEREPO_C_ERROR

gboolean
cr_drpm_support(void)
{
#ifdef    CR_DELTA_RPM_SUPPORT
        return TRUE;
#endif
    return FALSE;
}

#ifdef    CR_DELTA_RPM_SUPPORT

char *
cr_drpm_create(cr_DeltaTargetPackage *old,
               cr_DeltaTargetPackage *new,
               const char *destdir,
               GError **err)
{
    gchar *drpmfn, *drpmpath;

    drpmfn = g_strdup_printf("%s-%s-%s_%s-%s.%s.drpm",
                             old->name, old->version, old->release,
                             new->version, new->release, old->arch);
    drpmpath = g_build_filename(destdir, drpmfn, NULL);
    g_free(drpmfn);

    drpm_make_options *opts;
    drpm_make_options_init(&opts);
    drpm_make_options_defaults(opts);

    int ret = drpm_make(old->path, new->path, drpmpath, opts);
    if (ret != DRPM_ERR_OK) {
        g_set_error(err, ERR_DOMAIN, CRE_DELTARPM,
                    "Deltarpm cannot make %s (%d) from old: %s and new: %s", drpmpath, ret, old->path, new->path);
        free(drpmpath);
        drpm_make_options_destroy(&opts);
        return NULL;
    }

    drpm_make_options_destroy(&opts);
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
    int ret;

    assert(!err || *err == NULL);

    deltapackage = g_new0(cr_DeltaPackage, 1);
    deltapackage->chunk = g_string_chunk_new(0);

    deltapackage->package = cr_package_from_rpm_base(filename,
                                                     changelog_limit,
                                                     flags,
                                                     err);
    if (!deltapackage->package)
        goto errexit;

    ret = drpm_read(&delta, filename);
    if (ret != DRPM_ERR_OK) {
        g_set_error(err, ERR_DOMAIN, CRE_DELTARPM,
                    "Deltarpm cannot read %s (%d)", filename, ret);
        goto errexit;
    }

    ret = drpm_get_string(delta, DRPM_TAG_SRCNEVR, &str);
    if (ret != DRPM_ERR_OK) {
        g_set_error(err, ERR_DOMAIN, CRE_DELTARPM,
                    "Deltarpm cannot read source NEVR from %s (%d)",
                    filename, ret);
        goto errexit;
    }

    deltapackage->nevr = cr_safe_string_chunk_insert_null(
                                    deltapackage->chunk, str);

    ret = drpm_get_string(delta, DRPM_TAG_SEQUENCE, &str);
    if (ret != DRPM_ERR_OK) {
        g_set_error(err, ERR_DOMAIN, CRE_DELTARPM,
                    "Deltarpm cannot read delta sequence from %s (%d)",
                    filename, ret);
        goto errexit;
    }

    deltapackage->sequence = cr_safe_string_chunk_insert_null(
                                    deltapackage->chunk, str);

    drpm_destroy(&delta);

    return deltapackage;

errexit:

    if (delta)
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
cr_deltarpms_scan_oldpackagedirs(GSList *oldpackagedirs,
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
                g_warning("Cannot stat %s: %s", full_path, g_strerror(errno));
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
    GMutex mutex;
    gint64 active_work_size;
    gint active_tasks;
    GCond cond_task_finished;
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
    }

    g_debug("Deltas for \"%s\" (%"G_GINT64_FORMAT") generated",
            tpkg->name, tpkg->size_installed);

    g_mutex_lock(&(user_data->mutex));
    user_data->active_work_size -= tpkg->size_installed;
    user_data->active_tasks--;
    g_cond_signal(&(user_data->cond_task_finished));
    g_mutex_unlock(&(user_data->mutex));
    g_free(task);
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
cr_deltarpms_parallel_deltas(GSList *targetpackages,
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
        g_set_error(err, ERR_DOMAIN, CRE_DELTARPM,
                    "Number of delta workers must be a positive integer number");
        return FALSE;
    }

    // Init user_data
    user_data.outdeltadir           = outdeltadir;
    user_data.num_deltas            = num_deltas;
    user_data.oldpackages           = oldpackages;
    user_data.active_work_size      = G_GINT64_CONSTANT(0);
    user_data.active_tasks          = 0;

    g_mutex_init(&(user_data.mutex));
    g_cond_init(&(user_data.cond_task_finished));

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
        g_list_free_full(targets, (GDestroyNotify) cr_deltapackage_free);
        return FALSE;
    }

    // Push tasks into the pool
    while (targets) {
        gboolean inserted = FALSE;
        gint64 active_work_size;
        gint64 active_tasks;

        g_mutex_lock(&(user_data.mutex));
        while (user_data.active_tasks == workers)
            // Wait if all available threads are busy
            g_cond_wait(&(user_data.cond_task_finished), &(user_data.mutex));
        active_work_size = user_data.active_work_size;
        active_tasks = user_data.active_tasks;
        g_mutex_unlock(&(user_data.mutex));

        for (GList *elem = targets; elem; elem = g_list_next(elem)) {
            cr_DeltaTargetPackage *tpkg = elem->data;
            if ((active_work_size + tpkg->size_installed) <= max_work_size) {
                cr_DeltaTask *task = g_new0(cr_DeltaTask, 1);
                task->tpkg = tpkg;

                g_mutex_lock(&(user_data.mutex));
                user_data.active_work_size += tpkg->size_installed;
                user_data.active_tasks++;
                g_mutex_unlock(&(user_data.mutex));

                g_thread_pool_push(pool, task, NULL);
                targets = g_list_delete_link(targets, elem);
                inserted = TRUE;
                break;
            }
        }

        if (!inserted) {
            // In this iteration, no task was pushed to the pool
            g_mutex_lock(&(user_data.mutex));
            while (user_data.active_tasks == active_tasks)
                // Wait until any of running tasks finishes
                g_cond_wait(&(user_data.cond_task_finished), &(user_data.mutex));
            g_mutex_unlock(&(user_data.mutex));
        }
    }

    g_thread_pool_free(pool, FALSE, TRUE);
    g_list_free(targets);
    g_mutex_clear(&(user_data.mutex));
    g_cond_clear(&(user_data.cond_task_finished));

    return TRUE;
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


GSList *
cr_deltarpms_scan_targetdir(const char *path,
                            gint64 max_delta_rpm_size,
                            GError **err)
{
    GSList *targets = NULL;
    GQueue *sub_dirs = g_queue_new();
    GStringChunk *sub_dirs_chunk = g_string_chunk_new(1024);

    assert(!err || *err == NULL);

    g_queue_push_head(sub_dirs, g_strdup(path));

    // Recursively walk the dir
    gchar *dirname;
    while ((dirname = g_queue_pop_head(sub_dirs))) {

        // Open the directory
        GDir *dirp = g_dir_open(dirname, 0, NULL);
        if (!dirp) {
            g_warning("Cannot open directory %s", dirname);
            g_string_chunk_free(sub_dirs_chunk);
            return NULL;
        }

        // Iterate over files in directory
        const gchar *filename;
        while ((filename = g_dir_read_name(dirp))) {
            gchar *full_path;
            struct stat st;
            cr_DeltaTargetPackage *tpkg;

            full_path = g_build_filename(dirname, filename, NULL);

            if (!g_str_has_suffix(filename, ".rpm")) {
                if (g_file_test(full_path, G_FILE_TEST_IS_DIR)) {
                    // Directory
                    gchar *sub_dir_in_chunk;
                    sub_dir_in_chunk = g_string_chunk_insert(sub_dirs_chunk,
                                                             full_path);
                    g_queue_push_head(sub_dirs, sub_dir_in_chunk);
                    g_debug("Dir to scan: %s", sub_dir_in_chunk);
                }
                g_free(full_path);
                continue;
            }

            if (stat(full_path, &st) == -1) {
                g_warning("Cannot stat %s: %s", full_path, g_strerror(errno));
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
    }

    cr_queue_free_full(sub_dirs, g_free);
    g_string_chunk_free(sub_dirs_chunk);

    return targets;
}


/*
 * 3) Parallel xml chunk generation
 */

typedef struct {
    gchar *full_path;
} cr_PrestoDeltaTask;

typedef struct {
    GMutex mutex;
    GHashTable *ht;
    cr_ChecksumType checksum_type;
    const gchar *prefix_to_strip;
    size_t prefix_len;
} cr_PrestoDeltaUserData;

void
cr_prestodeltatask_free(cr_PrestoDeltaTask *task)
{
    if (!task)
        return;
    g_free(task->full_path);
    g_free(task);
}

static gboolean
walk_drpmsdir(const gchar *drpmsdir, GSList **inlist, GError **err)
{
    gboolean ret = TRUE;
    GSList *candidates = NULL;
    GQueue *sub_dirs = g_queue_new();
    GStringChunk *sub_dirs_chunk = g_string_chunk_new(1024);

    assert(drpmsdir);
    assert(inlist);
    assert(!err || *err == NULL);

    g_queue_push_head(sub_dirs, g_strdup(drpmsdir));

    // Recursively walk the drpmsdir
    gchar *dirname;
    while ((dirname = g_queue_pop_head(sub_dirs))) {

        // Open the directory
        GDir *dirp = g_dir_open(drpmsdir, 0, NULL);
        if (!dirp) {
            g_set_error(err, ERR_DOMAIN, CRE_IO,
                        "Cannot open directory %s", drpmsdir);
            goto exit;
        }

        // Iterate over files in directory
        const gchar *filename;
        while ((filename = g_dir_read_name(dirp))) {
            gchar *full_path = g_build_filename(dirname, filename, NULL);

            // Non .rpm files
            if (!g_str_has_suffix (filename, ".drpm")) {
                if (g_file_test(full_path, G_FILE_TEST_IS_DIR)) {
                    // Directory
                    gchar *sub_dir_in_chunk;
                    sub_dir_in_chunk = g_string_chunk_insert(sub_dirs_chunk,
                                                             full_path);
                    g_queue_push_head(sub_dirs, sub_dir_in_chunk);
                    g_debug("Dir to scan: %s", sub_dir_in_chunk);
                }
                g_free(full_path);
                continue;
            }

            // Take the file
            cr_PrestoDeltaTask *task = g_new0(cr_PrestoDeltaTask, 1);
            task->full_path = full_path;
            candidates = g_slist_prepend(candidates, task);
        }
        g_free(dirname);
        g_dir_close(dirp);
    }

    *inlist = candidates;
    candidates = NULL;

exit:
    g_slist_free_full(candidates, (GDestroyNotify) cr_prestodeltatask_free);
    cr_queue_free_full(sub_dirs, g_free);
    g_string_chunk_free(sub_dirs_chunk);

    return ret;
}


static void
cr_prestodelta_thread(gpointer data, gpointer udata)
{
    cr_PrestoDeltaTask *task          = data;
    cr_PrestoDeltaUserData *user_data = udata;

    cr_DeltaPackage *dpkg = NULL;
    struct stat st;
    gchar *xml_chunk = NULL, *key = NULL, *checksum = NULL;
    GError *tmp_err = NULL;

    printf("%s\n", task->full_path);

    // Load delta package
    dpkg = cr_deltapackage_from_drpm_base(task->full_path, 0, 0, &tmp_err);
    if (!dpkg) {
        g_warning("Cannot read drpm %s: %s", task->full_path, tmp_err->message);
        g_error_free(tmp_err);
        goto exit;
    }

    // Set the filename
    dpkg->package->location_href = cr_safe_string_chunk_insert(
                                    dpkg->package->chunk,
                                    task->full_path + user_data->prefix_len);

    // Stat the package (to get the size)
    if (stat(task->full_path, &st) == -1) {
        g_warning("%s: stat(%s) error (%s)", __func__,
                  task->full_path, g_strerror(errno));
        goto exit;
    } else {
        dpkg->package->size_package = st.st_size;
    }

    // Calculate the checksum
    checksum = cr_checksum_file(task->full_path,
                                user_data->checksum_type,
                                &tmp_err);
    if (!checksum) {
        g_warning("Cannot calculate checksum for %s: %s",
                  task->full_path, tmp_err->message);
        g_error_free(tmp_err);
        goto exit;
    }
    dpkg->package->checksum_type = cr_safe_string_chunk_insert(
                                        dpkg->package->chunk,
                                        cr_checksum_name_str(
                                            user_data->checksum_type));
    dpkg->package->pkgId = cr_safe_string_chunk_insert(dpkg->package->chunk,
                                                       checksum);

    // Generate XML
    xml_chunk = cr_xml_dump_deltapackage(dpkg, &tmp_err);
    if (tmp_err) {
        g_warning("Cannot generate xml for drpm %s: %s",
                  task->full_path, tmp_err->message);
        g_error_free(tmp_err);
        goto exit;
    }

    // Put the XML into the shared hash table
    gpointer pkey = NULL;
    gpointer pval = NULL;
    key = cr_package_nevra(dpkg->package);
    g_mutex_lock(&(user_data->mutex));
    if (g_hash_table_lookup_extended(user_data->ht, key, &pkey, &pval)) {
        // Key exists in the table
        // 1. Remove the key and value from the table without freeing them
        g_hash_table_steal(user_data->ht, key);
        // 2. Append to the list (the value from the hash table)
        GSList *list = g_slist_append(pval, xml_chunk);
        // 3. Insert the modified list again
        g_hash_table_insert(user_data->ht, pkey, list);
    } else {
        // Key doesn't exist yet
        GSList *list = g_slist_prepend(NULL, xml_chunk);
        g_hash_table_insert(user_data->ht, g_strdup(key), list);
    }
    g_mutex_unlock(&(user_data->mutex));

exit:
    g_free(checksum);
    g_free(key);
    cr_deltapackage_free(dpkg);
}

static gchar *
gen_newpackage_xml_chunk(const char *strnevra,
                         GSList *delta_chunks)
{
    cr_NEVRA *nevra;
    GString *chunk;

    if (!delta_chunks)
        return NULL;

    nevra = cr_str_to_nevra(strnevra);

    chunk = g_string_new(NULL);
    g_string_printf(chunk, "  <newpackage name=\"%s\" epoch=\"%s\" "
                    "version=\"%s\" release=\"%s\" arch=\"%s\">\n",
                    nevra->name, nevra->epoch ? nevra->epoch : "0",
                    nevra->version, nevra->release, nevra->arch);

    cr_nevra_free(nevra);

    for (GSList *elem = delta_chunks; elem; elem = g_slist_next(elem)) {
        gchar *delta_chunk = elem->data;
        g_string_append(chunk, delta_chunk);
    }

    g_string_append(chunk, "  </newpackage>\n");

    return g_string_free(chunk, FALSE);
}

gboolean
cr_deltarpms_generate_prestodelta_file(const gchar *drpmsdir,
                                       cr_XmlFile *f,
                                       cr_XmlFile *zck_f,
                                       cr_ChecksumType checksum_type,
                                       gint workers,
                                       const gchar *prefix_to_strip,
                                       GError **err)
{
    gboolean ret = TRUE;
    GSList *candidates = NULL;
    GThreadPool *pool;
    cr_PrestoDeltaUserData user_data;
    GHashTable *ht = NULL;
    GHashTableIter iter;
    gpointer key, value;
    GError *tmp_err = NULL;

    assert(drpmsdir);
    assert(f);
    assert(!err || *err == NULL);

    // Walk the drpms directory

    if (!walk_drpmsdir(drpmsdir, &candidates, &tmp_err)) {
        g_propagate_prefixed_error(err, tmp_err, "%s: ", __func__);
        ret = FALSE;
        goto exit;
    }

    // Setup pool of workers

    ht = g_hash_table_new_full(g_str_hash,
                               g_str_equal,
                               (GDestroyNotify) g_free,
                               (GDestroyNotify) cr_free_gslist_of_strings);

    user_data.ht                = ht;
    user_data.checksum_type     = checksum_type;
    user_data.prefix_to_strip   = prefix_to_strip,
    user_data.prefix_len        = prefix_to_strip ? strlen(prefix_to_strip) : 0;
    g_mutex_init(&(user_data.mutex));

    pool = g_thread_pool_new(cr_prestodelta_thread,
                             &user_data,
                             workers,
                             TRUE,
                             &tmp_err);
    if (tmp_err) {
        g_propagate_prefixed_error(err, tmp_err,
                "Cannot create pool for prestodelta file generation: ");
        ret = FALSE;
        goto exit;
    }

    // Push tasks to the pool

    for (GSList *elem = candidates; elem; elem = g_slist_next(elem)) {
        g_thread_pool_push(pool, elem->data, NULL);
    }

    // Wait until the pool finishes

    g_thread_pool_free(pool, FALSE, TRUE);

    // Write out the results


    g_hash_table_iter_init(&iter, user_data.ht);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        gchar *chunk = NULL;
        gchar *nevra = key;

        chunk = gen_newpackage_xml_chunk(nevra, (GSList *) value);
        cr_xmlfile_add_chunk(f, chunk, NULL);

        /* Write out zchunk file */
        if (zck_f) {
            cr_xmlfile_add_chunk(zck_f, chunk, NULL);
            cr_end_chunk(zck_f->f, &tmp_err);
            if (tmp_err) {
                g_free(chunk);
                g_propagate_prefixed_error(err, tmp_err,
                    "Cannot create pool for prestodelta file generation: ");
                ret = FALSE;
                goto exit;
            }
        }
        g_free(chunk);
    }

exit:
    g_slist_free_full(candidates, (GDestroyNotify) cr_prestodeltatask_free);
    g_mutex_clear(&(user_data.mutex));
    g_hash_table_destroy(ht);

    return ret;
}

#endif
