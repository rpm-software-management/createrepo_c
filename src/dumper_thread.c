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
#include "checksum.h"
#include "cleanup.h"
#include "deltarpms.h"
#include "dumper_thread.h"
#include "error.h"
#include "misc.h"
#include "parsepkg.h"
#include "xml_dump.h"
#include <fcntl.h>

#define MAX_TASK_BUFFER_LEN         20
#define CACHEDCHKSUM_BUFFER_LEN     2048

struct BufferedTask {
    long id;                        // ID of the task
    struct cr_XmlStruct res;        // XML for primary, filelists and other
    cr_Package *pkg;                // Package structure
};


static gint
buf_task_sort_func(gconstpointer a, gconstpointer b, G_GNUC_UNUSED gpointer data)
{
    const struct BufferedTask *task_a = a;
    const struct BufferedTask *task_b = b;
    if (task_a->id < task_b->id)  return -1;
    if (task_a->id == task_b->id) return 0;
    return 1;
}


static void
wait_for_incremented_ids(long id, struct UserData *udata)
{
    g_mutex_lock(&(udata->mutex_pri));
    while (udata->id_pri != id)
        g_cond_wait (&(udata->cond_pri), &(udata->mutex_pri));
    ++udata->id_pri;
    g_cond_broadcast(&(udata->cond_pri));
    g_mutex_unlock(&(udata->mutex_pri));

    g_mutex_lock(&(udata->mutex_fil));
    while (udata->id_fil != id)
        g_cond_wait (&(udata->cond_fil), &(udata->mutex_fil));
    ++udata->id_fil;
    g_cond_broadcast(&(udata->cond_fil));
    g_mutex_unlock(&(udata->mutex_fil));

    if (udata->filelists_ext) {
        g_mutex_lock(&(udata->mutex_fex));
        while (udata->id_fex != id)
            g_cond_wait (&(udata->cond_fex), &(udata->mutex_fex));
        ++udata->id_fex;
        g_cond_broadcast(&(udata->cond_fex));
        g_mutex_unlock(&(udata->mutex_fex));
    }

    g_mutex_lock(&(udata->mutex_oth));
    while (udata->id_oth != id)
        g_cond_wait (&(udata->cond_oth), &(udata->mutex_oth));
    ++udata->id_oth;
    g_cond_broadcast(&(udata->cond_oth));
    g_mutex_unlock(&(udata->mutex_oth));
}


static void
write_pkg(long id,
          struct cr_XmlStruct res,
          cr_Package *pkg,
          struct UserData *udata)
{
    GError *tmp_err = NULL;

    // Write primary data
    g_mutex_lock(&(udata->mutex_pri));
    while (udata->id_pri != id)
        g_cond_wait (&(udata->cond_pri), &(udata->mutex_pri));

    udata->package_count++;
    g_free(udata->prev_srpm);
    udata->prev_srpm = udata->cur_srpm;
    udata->cur_srpm = g_strdup(pkg->rpm_sourcerpm);
    gboolean new_pkg = FALSE;
    if (g_strcmp0(udata->prev_srpm, udata->cur_srpm) != 0)
        new_pkg = TRUE;

    ++udata->id_pri;
    cr_xmlfile_add_chunk(udata->pri_f, (const char *) res.primary, &tmp_err);
    if (tmp_err) {
        g_critical("Cannot add primary chunk:\n%s\nError: %s",
                   res.primary, tmp_err->message);
        udata->had_errors = TRUE;
        g_clear_error(&tmp_err);
    }

    if (udata->pri_db) {
        cr_db_add_pkg(udata->pri_db, pkg, &tmp_err);
        if (tmp_err) {
            g_critical("Cannot add record of %s (%s) to primary db: %s",
                       pkg->name, pkg->pkgId, tmp_err->message);
            udata->had_errors = TRUE;
            g_clear_error(&tmp_err);
        }
    }
    if (udata->pri_zck) {
        if (new_pkg) {
            cr_end_chunk(udata->pri_zck->f, &tmp_err);
            if (tmp_err) {
                g_critical("Unable to end primary zchunk: %s", tmp_err->message);
                udata->had_errors = TRUE;
                g_clear_error(&tmp_err);
            }
        }
        cr_xmlfile_add_chunk(udata->pri_zck, (const char *) res.primary, &tmp_err);
        if (tmp_err) {
            g_critical("Cannot add primary zchunk:\n%s\nError: %s",
                       res.primary, tmp_err->message);
            udata->had_errors = TRUE;
            g_clear_error(&tmp_err);
        }
    }

    g_cond_broadcast(&(udata->cond_pri));
    g_mutex_unlock(&(udata->mutex_pri));

    // Write fielists data
    g_mutex_lock(&(udata->mutex_fil));
    while (udata->id_fil != id)
        g_cond_wait (&(udata->cond_fil), &(udata->mutex_fil));
    ++udata->id_fil;
    cr_xmlfile_add_chunk(udata->fil_f, (const char *) res.filelists, &tmp_err);
    if (tmp_err) {
        g_critical("Cannot add filelists chunk:\n%s\nError: %s",
                   res.filelists, tmp_err->message);
        udata->had_errors = TRUE;
        g_clear_error(&tmp_err);
    }

    if (udata->fil_db) {
        cr_db_add_pkg(udata->fil_db, pkg, &tmp_err);
        if (tmp_err) {
            g_critical("Cannot add record of %s (%s) to filelists db: %s",
                       pkg->name, pkg->pkgId, tmp_err->message);
            udata->had_errors = TRUE;
            g_clear_error(&tmp_err);
        }
    }
    if (udata->fil_zck) {
        if (new_pkg) {
            cr_end_chunk(udata->fil_zck->f, &tmp_err);
            if (tmp_err) {
                g_critical("Unable to end filelists zchunk: %s", tmp_err->message);
                udata->had_errors = TRUE;
                g_clear_error(&tmp_err);
            }
        }
        cr_xmlfile_add_chunk(udata->fil_zck, (const char *) res.filelists, &tmp_err);
        if (tmp_err) {
            g_critical("Cannot add filelists zchunk:\n%s\nError: %s",
                       res.filelists, tmp_err->message);
            udata->had_errors = TRUE;
            g_clear_error(&tmp_err);
        }
    }

    g_cond_broadcast(&(udata->cond_fil));
    g_mutex_unlock(&(udata->mutex_fil));

    // Write filelists-ext data
    if (udata->filelists_ext) {
        g_mutex_lock(&(udata->mutex_fex));
        while (udata->id_fex != id)
            g_cond_wait (&(udata->cond_fex), &(udata->mutex_fex));
        ++udata->id_fex;
        cr_xmlfile_add_chunk(udata->fex_f, (const char *) res.filelists_ext, &tmp_err);
        if (tmp_err) {
            g_critical("Cannot add filelists-ext chunk:\n%s\nError: %s",
                       res.filelists_ext, tmp_err->message);
            udata->had_errors = TRUE;
            g_clear_error(&tmp_err);
        }

        if (udata->fex_db) {
            cr_db_add_pkg(udata->fex_db, pkg, &tmp_err);
            if (tmp_err) {
                g_critical("Cannot add record of %s (%s) to filelists-ext db: %s",
                           pkg->name, pkg->pkgId, tmp_err->message);
                udata->had_errors = TRUE;
                g_clear_error(&tmp_err);
            }
        }
        if (udata->fex_zck) {
            if (new_pkg) {
                cr_end_chunk(udata->fex_zck->f, &tmp_err);
                if (tmp_err) {
                    g_critical("Unable to end filelists-ext zchunk: %s", tmp_err->message);
                    udata->had_errors = TRUE;
                    g_clear_error(&tmp_err);
                }
            }
            cr_xmlfile_add_chunk(udata->fex_zck, (const char *) res.filelists_ext, &tmp_err);
            if (tmp_err) {
                g_critical("Cannot add filelists-ext zchunk:\n%s\nError: %s",
                           res.filelists_ext, tmp_err->message);
                udata->had_errors = TRUE;
                g_clear_error(&tmp_err);
            }
        }

        g_cond_broadcast(&(udata->cond_fex));
        g_mutex_unlock(&(udata->mutex_fex));
    }

    // Write other data
    g_mutex_lock(&(udata->mutex_oth));
    while (udata->id_oth != id)
        g_cond_wait (&(udata->cond_oth), &(udata->mutex_oth));
    ++udata->id_oth;
    cr_xmlfile_add_chunk(udata->oth_f, (const char *) res.other, &tmp_err);
    if (tmp_err) {
        g_critical("Cannot add other chunk:\n%s\nError: %s",
                   res.other, tmp_err->message);
        udata->had_errors = TRUE;
        g_clear_error(&tmp_err);
    }

    if (udata->oth_db) {
        cr_db_add_pkg(udata->oth_db, pkg, NULL);
        if (tmp_err) {
            g_critical("Cannot add record of %s (%s) to other db: %s",
                       pkg->name, pkg->pkgId, tmp_err->message);
            udata->had_errors = TRUE;
            g_clear_error(&tmp_err);
        }
    }
    if (udata->oth_zck) {
        if (new_pkg) {
            cr_end_chunk(udata->oth_zck->f, &tmp_err);
            if (tmp_err) {
                g_critical("Unable to end other zchunk: %s", tmp_err->message);
                udata->had_errors = TRUE;
                g_clear_error(&tmp_err);
            }
        }
        cr_xmlfile_add_chunk(udata->oth_zck, (const char *) res.other, &tmp_err);
        if (tmp_err) {
            g_critical("Cannot add other zchunk:\n%s\nError: %s",
                       res.other, tmp_err->message);
            udata->had_errors = TRUE;
            g_clear_error(&tmp_err);
        }
    }
    g_cond_broadcast(&(udata->cond_oth));
    g_mutex_unlock(&(udata->mutex_oth));
}


struct DelayedTask {
    cr_Package *pkg;
};


void
cr_delayed_dump_set(gpointer user_data)
{
    struct UserData *udata = (struct UserData *) user_data;
    udata->delayed_write = g_array_sized_new(TRUE, TRUE,
                                             sizeof(struct DelayedTask),
                                             udata->task_count);
}


void
cr_delayed_dump_run(gpointer user_data)
{
    GError *tmp_err = NULL;
    struct UserData *udata = (struct UserData *) user_data;
    long int stop = udata->task_count;
    g_debug("Performing the delayed metadata dump");
    for (int id = 0; id < stop; id++) {
        struct DelayedTask dtask = g_array_index(udata->delayed_write,
                                                 struct DelayedTask, id);
        if (!dtask.pkg || dtask.pkg->skip_dump) {
            // invalid || explicitly skipped task
            wait_for_incremented_ids(id, udata);
            continue;
        }

        struct cr_XmlStruct res;
        if (udata->filelists_ext) {
            res = cr_xml_dump_ext(dtask.pkg,  &tmp_err);
        } else {
            res = cr_xml_dump(dtask.pkg, &tmp_err);
        }
        if (tmp_err) {
            g_critical("Cannot dump XML for %s (%s): %s",
                       dtask.pkg->name, dtask.pkg->pkgId, tmp_err->message);
            udata->had_errors = TRUE;
            g_clear_error(&tmp_err);
        }
        else {
            write_pkg(id, res, dtask.pkg, udata);
        }

        g_free(res.primary);
        g_free(res.filelists);
        g_free(res.filelists_ext);
        g_free(res.other);
    }
}


static char *
get_checksum(const char *filename,
             cr_ChecksumType type,
             cr_Package *pkg,
             const char *cachedir,
             GError **err)
{
    GError *tmp_err = NULL;
    char *checksum = NULL;
    char *cachefn = NULL;

    if (cachedir) {
        // Prepare cache fn
        cr_ChecksumCtx *ctx = cr_checksum_new(type, err);
        if (!ctx) return NULL;

        if (pkg->siggpg)
            cr_checksum_update(ctx, pkg->siggpg->data, pkg->siggpg->size, NULL);
        if (pkg->sigpgp)
            cr_checksum_update(ctx, pkg->sigpgp->data, pkg->sigpgp->size, NULL);
        if (pkg->hdrid)
            cr_checksum_update(ctx, pkg->hdrid, strlen(pkg->hdrid), NULL);

        gchar *key = cr_checksum_final(ctx, err);
        if (!key) return NULL;

        cachefn = g_strdup_printf("%s%s-%s-%"G_GINT64_FORMAT"-%"G_GINT64_FORMAT,
                                  cachedir,
                                  cr_get_filename(pkg->location_href),
                                  key, pkg->size_installed, pkg->time_file);
        g_free(key);

        // Try to load checksum
        FILE *f = fopen(cachefn, "r");
        if (f) {
            char buf[CACHEDCHKSUM_BUFFER_LEN];
            size_t readed = fread(buf, 1, CACHEDCHKSUM_BUFFER_LEN, f);
            if (!ferror(f) && readed > 0) {
                checksum = g_strndup(buf, readed);
            }
            fclose(f);
        }

        if (checksum) {
            g_debug("Cached checksum used: %s: \"%s\"", cachefn, checksum);
            goto exit;
        }
    }

    // Calculate checksum
    checksum = cr_checksum_file(filename, type, &tmp_err);
    if (!checksum) {
        g_propagate_prefixed_error(err, tmp_err,
                                   "Error while checksum calculation: ");
        goto exit;
    }

    // Cache the checksum value
    if (cachefn && !g_file_test(cachefn, G_FILE_TEST_EXISTS)) {
        gchar *template = g_strconcat(cachefn, "-XXXXXX", NULL);
        // Files should not be executable so use only 0666
        gint fd = g_mkstemp_full(template, O_RDWR, 0666);
        if (fd < 0) {
            g_free(template);
            goto exit;
        }

        write(fd, checksum, strlen(checksum));
        close(fd);
        if (!cr_move_recursive(template, cachefn, &tmp_err)) {
            g_propagate_prefixed_error(err, tmp_err, "Error while renaming: ");
            g_remove(template);
        }
        g_free(template);
    }

exit:
    g_free(cachefn);

    return checksum;
}

gchar *
prepare_split_media_baseurl(int media_id, const char *location_base)
{
    // Default location_base "media:" in split mode
    if (!location_base || !*location_base)
        return g_strdup_printf("media:#%d", media_id);

    // Location doesn't end with "://" -> just append "#MEDIA_ID"
    if (!g_str_has_suffix(location_base, "://"))
        return g_strdup_printf("%s#%d", location_base, media_id);

    // Calculate location_base -> replace ending "//" with "#MEDIA_ID"
    size_t lb_length = strlen(location_base);
    _cleanup_free_ gchar *tmp_location_base = NULL;
    tmp_location_base = g_strndup(location_base, (lb_length-2));
    return g_strdup_printf("%s#%d", tmp_location_base, media_id);
}

static cr_Package *
load_rpm(const char *fullpath,
         cr_ChecksumType checksum_type,
         const char *checksum_cachedir,
         const char *location_href,
         const char *location_base,
         int changelog_limit,
         struct stat *stat_buf,
         cr_HeaderReadingFlags hdrrflags,
         GError **err)
{
    cr_Package *pkg = NULL;
    GError *tmp_err = NULL;

    assert(fullpath);
    assert(!err || *err == NULL);

    // Get a package object
    pkg = cr_package_from_rpm_base(fullpath, changelog_limit, hdrrflags, err);
    if (!pkg)
        goto errexit;

    // Locations
    pkg->location_href = cr_safe_string_chunk_insert(pkg->chunk, location_href);
    pkg->location_base = cr_safe_string_chunk_insert(pkg->chunk, location_base);

    // Get checksum type string
    pkg->checksum_type = cr_safe_string_chunk_insert(pkg->chunk,
                                        cr_checksum_name_str(checksum_type));

    // Get file stat
    if (!stat_buf) {
        struct stat stat_buf_own;
        if (stat(fullpath, &stat_buf_own) == -1) {
            const gchar * stat_error = g_strerror(errno);
            g_warning("%s: stat(%s) error (%s)", __func__,
                      fullpath, stat_error);
            g_set_error(err,  CREATEREPO_C_ERROR, CRE_IO, "stat(%s) failed: %s",
                        fullpath, stat_error);
            goto errexit;
        }
        pkg->time_file    = stat_buf_own.st_mtime;
        pkg->size_package = stat_buf_own.st_size;
    } else {
        pkg->time_file    = stat_buf->st_mtime;
        pkg->size_package = stat_buf->st_size;
    }

    // Compute checksum
    char *checksum = get_checksum(fullpath, checksum_type, pkg,
                                  checksum_cachedir, &tmp_err);
    if (!checksum) {
        g_propagate_error(err, tmp_err);
        goto errexit;
    }
    pkg->pkgId = cr_safe_string_chunk_insert(pkg->chunk, checksum);
    g_free(checksum);

    // Get header range
    struct cr_HeaderRangeStruct hdr_r = cr_get_header_byte_range(fullpath,
                                                                 &tmp_err);
    if (tmp_err) {
        g_propagate_prefixed_error(err, tmp_err,
                                   "Error while determining header range: ");
        goto errexit;
    }

    pkg->rpm_header_start = hdr_r.start;
    pkg->rpm_header_end = hdr_r.end;

    return pkg;

errexit:
    cr_package_free(pkg);
    return NULL;
}

void
cr_dumper_thread(gpointer data, gpointer user_data)
{
    GError *tmp_err = NULL;
    gboolean old_used = FALSE;  // To use old metadata?
    cr_Package *md  = NULL;     // Package from loaded MetaData
    cr_Package *pkg = NULL;     // Package we work with
    struct stat stat_buf;       // Struct with info from stat() on file
    struct cr_XmlStruct res;    // Structure for generated XML
    cr_HeaderReadingFlags hdrrflags = CR_HDRR_NONE;

    struct UserData *udata = (struct UserData *) user_data;
    struct PoolTask *task  = (struct PoolTask *) data;

    struct DelayedTask *dtask = NULL;
    if (udata->delayed_write) {
        // even if we might found out that this is an invalid package,
        // we have to allocate a delayed task, we have to assure that
        // len(delayed_write) == udata->task_count and that all items
        // are processed in the delayed run.
        dtask = &g_array_index(udata->delayed_write,
                               struct DelayedTask,
                               task->id);
        dtask->pkg = NULL;
    }

    // get location_href without leading part of path (path to repo)
    // including '/' char
    _cleanup_free_ gchar *location_href = NULL;
    location_href = g_strdup(task->full_path + udata->repodir_name_len);

    _cleanup_free_ gchar *location_base = NULL;
    location_base = g_strdup(udata->location_base);

    // User requested modification of the location href
    if (udata->cut_dirs) {
        gchar *tmp = location_href;
        location_href = g_strdup(cr_cut_dirs(location_href, udata->cut_dirs));
        g_free(tmp);
    }

    if (udata->location_prefix) {
        gchar *tmp = location_href;
        location_href = g_build_filename(udata->location_prefix, tmp, NULL);
        g_free(tmp);
    }

    // Prepare location base (if split option is used)
    if (task->media_id) {
        gchar *new_location_base = prepare_split_media_baseurl(task->media_id,
                                                               location_base);
        g_free(location_base);
        location_base = new_location_base;
    }

    // If --cachedir is used, load signatures and hdrid from packages too
    if (udata->checksum_cachedir)
        hdrrflags = CR_HDRR_LOADHDRID | CR_HDRR_LOADSIGNATURES;

    // Get stat info about file
    if (udata->old_metadata && !(udata->skip_stat)) {
        if (stat(task->full_path, &stat_buf) == -1) {
            g_critical("Stat() on %s: %s", task->full_path, g_strerror(errno));
            goto task_cleanup;
        }
    }

    // Update stuff
    if (udata->old_metadata) {
        char *cache_key = cr_get_cleaned_href(location_href);

        // We have old metadata
        g_mutex_lock(&(udata->mutex_old_md));
        md = (cr_Package *) g_hash_table_lookup(
                                cr_metadata_hashtable(udata->old_metadata),
                                cache_key);
        // Remove the pkg from the hash table of old metadata, so that no other
        // thread can use it as CACHE, because later we modify it destructively
        g_hash_table_steal(cr_metadata_hashtable(udata->old_metadata),
                                                 cache_key);
        g_mutex_unlock(&(udata->mutex_old_md));

        if (md) {
            g_debug("CACHE HIT %s", task->filename);

            if (udata->skip_stat) {
                old_used = TRUE;
            } else if (stat_buf.st_mtime == md->time_file
                       && stat_buf.st_size == md->size_package
                       && !strcmp(udata->checksum_type_str, md->checksum_type))
            {
                old_used = TRUE;
            } else {
                g_debug("%s metadata are obsolete -> generating new",
                        task->filename);
            }

            if (old_used) {
                // CR_PACKAGE_SINGLE_CHUNK used with the preloaded (old)
                // metadata.  Create a new per-package chunk.
                assert (!md->chunk);
                md->chunk = g_string_chunk_new(1024);
                md->loadingflags &= ~CR_PACKAGE_SINGLE_CHUNK;

                // We have usable old data, but we have to set proper locations
                // WARNING! location_href is overidden
                // location_base is kept by default, unless specified differently
                //
                md->location_href = cr_safe_string_chunk_insert(md->chunk, location_href);
                if (location_base)
                    md->location_base = cr_safe_string_chunk_insert(md->chunk, location_base);
                // ^^^ The location_base and location_href create a new data
                // chunk, even though the rest of the metadata is stored in the
                // global chunk (shared with all packages).
            }
        }
    }

    // Load package and gen XML metadata
    if (!old_used) {
        // Load package from file
        pkg = load_rpm(task->full_path, udata->checksum_type,
                       udata->checksum_cachedir, location_href,
                       location_base, udata->changelog_limit,
                       NULL, hdrrflags, &tmp_err);
        assert(pkg || tmp_err);

        if (!pkg) {
            g_warning("Cannot read package: %s: %s",
                      task->full_path, tmp_err->message);
            udata->had_errors = TRUE;
            g_clear_error(&tmp_err);
            goto task_cleanup;
        }

        if (udata->output_pkg_list){
            g_mutex_lock(&(udata->mutex_output_pkg_list));
            fprintf(udata->output_pkg_list, "%s\n", pkg->location_href);
            g_mutex_unlock(&(udata->mutex_output_pkg_list));
        }
    } else {
        // Just gen XML from old loaded metadata
        pkg = md;
    }

#ifdef CR_DELTA_RPM_SUPPORT
    // Delta candidate
    if (udata->deltas
        && !old_used
        && pkg->size_installed < udata->max_delta_rpm_size)
    {
        cr_DeltaTargetPackage *tpkg;
        tpkg = cr_deltatargetpackage_from_package(pkg,
                                                  task->full_path,
                                                  NULL);
        if (tpkg) {
            g_mutex_lock(&(udata->mutex_deltatargetpackages));
            udata->deltatargetpackages = g_slist_prepend(
                                                udata->deltatargetpackages,
                                                tpkg);
            g_mutex_unlock(&(udata->mutex_deltatargetpackages));
        } else {
            g_warning("Cannot create deltatargetpackage for: %s-%s-%s",
                      pkg->name, pkg->version, pkg->release);
        }
    }
#endif

    // Allow checking that the same package (NEVRA) isn't present multiple times in the metadata
    // Keep a hashtable of NEVRA mapped to an array-list of location_href values
    g_mutex_lock(&(udata->mutex_nevra_table));
    gchar *nevra = cr_package_nevra(pkg);
    GArray *pkg_locations = g_hash_table_lookup(udata->nevra_table, nevra);
    if (!pkg_locations) {
        pkg_locations = g_array_new(FALSE, TRUE, sizeof(struct DuplicateLocation));
        g_hash_table_insert(udata->nevra_table, nevra, pkg_locations);
    } else {
        g_free(nevra);
    }

    struct DuplicateLocation location;
    location.location = g_strdup(pkg->location_href);
    // location (udate->nevra_table) gathers all handled packages:
    //  - new cr_Packages produced by processing rpm files
    //  - old cr_Packages transferred from parsed old metadata (during --update)
    // Therefore it takes ownership of them, it is resposible for freeing them.
    location.pkg = pkg;
    g_array_append_val(pkg_locations, location);
    g_mutex_unlock(&(udata->mutex_nevra_table));

    if (dtask) {
        dtask->pkg = pkg;
        g_free(task->full_path);
        g_free(task->filename);
        g_free(task->path);
        g_free(task);
        return;
    }

    // Pre-calculate the XML data aside any critical section, and early enough
    // so we can put it into the buffer (so buffered single-threaded write later
    // is faster).
    if (udata->filelists_ext) {
        res = cr_xml_dump_ext(pkg,  &tmp_err);
    } else {
        res = cr_xml_dump(pkg, &tmp_err);
    }
    if (tmp_err) {
        g_critical("Cannot dump XML for %s (%s): %s",
                   pkg->name, pkg->pkgId, tmp_err->message);
        udata->had_errors = TRUE;
        g_clear_error(&tmp_err);
        goto task_cleanup;
    }

    // Buffering stuff
    g_mutex_lock(&(udata->mutex_buffer));

    if (g_queue_get_length(udata->buffer) < MAX_TASK_BUFFER_LEN
        && udata->id_pri != task->id
        && udata->task_count > (task->id + 1))
    {
        // If:
        //  * this isn't our turn
        //  * the buffer isn't full
        //  * this isn't the last task
        // Then: save the task to the buffer

        struct BufferedTask *buf_task = g_malloc0(sizeof(struct BufferedTask));
        buf_task->id  = task->id;
        buf_task->res = res;
        buf_task->pkg = pkg;

        g_queue_insert_sorted(udata->buffer, buf_task, buf_task_sort_func, NULL);
        g_mutex_unlock(&(udata->mutex_buffer));

        g_free(task->full_path);
        g_free(task->filename);
        g_free(task->path);
        g_free(task);

        return;
    }

    g_mutex_unlock(&(udata->mutex_buffer));

    // Dump XML and SQLite
    write_pkg(task->id, res, pkg, udata);

    g_free(res.primary);
    g_free(res.filelists);
    g_free(res.filelists_ext);
    g_free(res.other);

task_cleanup:
    // Clean up
    if (!dtask && udata->id_pri <= task->id) {
        // An error was encountered and we have to wait to increment counters
        wait_for_incremented_ids(task->id, udata);
    }

    g_free(task->full_path);
    g_free(task->filename);
    g_free(task->path);
    g_free(task);

    // Try to write all results from buffer which was waiting for us
    while (1) {
        struct BufferedTask *buf_task;
        g_mutex_lock(&(udata->mutex_buffer));
        buf_task = g_queue_peek_head(udata->buffer);
        if (buf_task && buf_task->id == udata->id_pri) {
            buf_task = g_queue_pop_head (udata->buffer);
            g_mutex_unlock(&(udata->mutex_buffer));
            // Dump XML and SQLite
            write_pkg(buf_task->id, buf_task->res, buf_task->pkg, udata);
            // Clean up
            g_free(buf_task->res.primary);
            g_free(buf_task->res.filelists);
            g_free(buf_task->res.filelists_ext);
            g_free(buf_task->res.other);
            g_free(buf_task);
        } else {
            g_mutex_unlock(&(udata->mutex_buffer));
            break;
        }
    }

    return;
}
