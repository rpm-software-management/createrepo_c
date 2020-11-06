/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2013  Tomas Mlcoch
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
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "cleanup.h"
#include "error.h"
#include "misc.h"
#include "checksum.h"
#include "modifyrepo_shared.h"
#include "compression_wrapper.h"
#include "threads.h"
#include "xml_dump.h"

#define ERR_DOMAIN              CREATEREPO_C_ERROR
#define DEFAULT_COMPRESSION     CR_CW_GZ_COMPRESSION
#define DEFAULT_CHECKSUM        CR_CHECKSUM_SHA256

cr_ModifyRepoTask *
cr_modifyrepotask_new(void)
{
    cr_ModifyRepoTask *task = g_new0(cr_ModifyRepoTask, 1);
    task->chunk = g_string_chunk_new(16);
    return task;
}

void
cr_modifyrepotask_free(cr_ModifyRepoTask *task)
{
    if (!task) return;
    g_string_chunk_free(task->chunk);
    g_free(task);
}

gchar *
cr_remove_compression_suffix_if_present(gchar* name, GError **err)
{
    cr_CompressionType src_fn_com_type = cr_detect_compression(name, err);
    if (src_fn_com_type != CR_CW_NO_COMPRESSION && src_fn_com_type != CR_CW_UNKNOWN_COMPRESSION){
        const gchar *src_suffix = cr_compression_suffix(src_fn_com_type);
        if (src_suffix){
            if (g_str_has_suffix(name, src_suffix)){
                int name_len = strlen(name);
                int suffix_len = strlen(src_suffix);
                return g_strndup(name, name_len - suffix_len);
            }
        }
    }
    return g_strdup(name);
}

gchar *
cr_write_file(gchar *repopath, cr_ModifyRepoTask *task,
           cr_CompressionType compress_type, GError **err)
{
    const gchar *suffix = NULL;

    if (task->compress)
        suffix = cr_compression_suffix(compress_type);

    gchar *src_fn = task->path;  // Shortcut
    gchar *dst_fn = NULL;

    char* sufixless_src_fn = cr_remove_compression_suffix_if_present(task->path, err);

    // Prepare dst filename - Get basename
    _cleanup_free_ gchar *filename = NULL;
    if (task->new_name)
        filename = g_path_get_basename(task->new_name);
    else
        filename = g_path_get_basename(sufixless_src_fn);
    g_free(sufixless_src_fn);

    // Prepare dst filename - Add suffix
    if (suffix) {
        gchar *tmp_fn = g_strconcat(filename, suffix, NULL);
        g_free(filename);
        filename = tmp_fn;
    }

    // Prepare dst filename - Full path
    dst_fn = g_build_filename(repopath, filename, NULL);
    task->dst_fn = g_string_chunk_insert(task->chunk, dst_fn);

    // Check if the src and dst is the same file
    gboolean identical = FALSE;
    if (!cr_identical_files(src_fn, dst_fn, &identical, err))
        return NULL;

    if (identical) {
        // Source and destination file is the same file
        g_debug("Using already existing file: %s", dst_fn);
    } else {
        // Check if the file already exist
        if (g_file_test(dst_fn, G_FILE_TEST_EXISTS) &&
            g_str_has_suffix(dst_fn, cr_compression_suffix(compress_type))) {
            g_warning("Destination file \"%s\" already exists and will be "
                      "overwritten", dst_fn);
        }

        // Do the copy
        g_debug("%s: Copy & compress operation %s -> %s",
                 __func__, src_fn, dst_fn);

        if (cr_compress_file(src_fn, dst_fn, compress_type,
                             task->zck_dict_dir, TRUE, err) != CRE_OK) {
            g_debug("%s: Copy & compress operation failed", __func__);
            return NULL;
        }
    }
    return dst_fn;
}

gboolean
cr_modifyrepo(GSList *modifyrepotasks, gchar *repopath, GError **err)
{
    assert(!err || *err == NULL);

    if (!modifyrepotasks) {
        g_debug("%s: No tasks to process", __func__);
        return TRUE;
    }

    // Parse repomd.xml

    gchar *repomd_path = g_build_filename(repopath, "repomd.xml", NULL);
    if (!g_file_test(repomd_path, G_FILE_TEST_IS_REGULAR)) {
        g_set_error(err, ERR_DOMAIN, CRE_IO,
                    "Regular file \"%s\" doesn't exists", repomd_path);
        g_free(repomd_path);
        return FALSE;
    }

    cr_Repomd *repomd = cr_repomd_new();
    int rc = cr_xml_parse_repomd(repomd_path, repomd, cr_warning_cb,
                                  "Repomd XML parser", err);
    if (rc != CRE_OK) {
        g_debug("%s: Error while parsing repomd.xml", __func__);
        cr_repomd_free(repomd);
        g_free(repomd_path);
        return FALSE;
    }

    // TODO:
    // (?) Autodetect used checksum_type
    // (?) Autodetect if unique_md_filenames are used

    // Prepare tasks

    for (GSList *elem = modifyrepotasks; elem; elem = g_slist_next(elem)) {
        cr_ModifyRepoTask *task = elem->data;

        if (!task->type) {
            // If type is not specified, derive it from path or new name
            gchar *basename;

            if (task->new_name)
                basename = g_path_get_basename(task->new_name);
            else
                basename = g_path_get_basename(task->path);

            // Split at first '.' in filename and use only first part
            for (gchar *tmp=basename; *tmp; tmp++) {
                if (*tmp == '.') {
                    *tmp = '\0';
                    break;
                }
            }

            task->type = cr_safe_string_chunk_insert_null(task->chunk,
                                                          basename);
            g_debug("%s: Use derived type \"%s\" (%s)",
                    __func__, task->type, basename);
            g_free(basename);
        }

        if (task->remove)
            continue;

        if (task->compress && task->compress_type == CR_CW_UNKNOWN_COMPRESSION)
            // If compression enabled but type not specified, use default
            task->compress_type = DEFAULT_COMPRESSION;

        if (task->checksum_type == CR_CHECKSUM_UNKNOWN)
            // If no checksum type specified, use default
            task->checksum_type = DEFAULT_CHECKSUM;
    }

    // Check tasks

    for (GSList *elem = modifyrepotasks; elem; elem = g_slist_next(elem)) {
        cr_ModifyRepoTask *task = elem->data;

        if (task->remove) {

            // Check if metadata of a type that should be removed
            // exists in repomd
            if (!cr_repomd_get_record(repomd, task->type))
                g_warning("Record of type \"%s\", which should be removed, "
                          "doesn't exist in repomd.xml", task->path);

            if (task->new_name)
                g_warning("Use remove with new_name doesn't make a sense");

        } else {

            // Check if file exists
            if (!g_file_test(task->path, G_FILE_TEST_IS_REGULAR)) {
                g_debug("%s: Regular file \"%s\" doesn't exist",
                        __func__, task->path);
                cr_repomd_free(repomd);
                g_free(repomd_path);
                return FALSE;
            }

            // Check if new_name is not empty string
            if (task->new_name) {
                if (!g_strcmp0(task->new_name, "")) {
                    g_debug("%s: New name cannot be empty", __func__);
                    cr_repomd_free(repomd);
                    g_free(repomd_path);
                    return FALSE;
                }
            }

            // Check if record with this name doesn't exists yet
            if (cr_repomd_get_record(repomd, task->type))
                g_warning("Record with type \"%s\" already exists "
                          "in repomd.xml", task->type);

        }
    }

    //
    // Modifications of the target repository starts here
    //

    // Add (copy) new metadata to repodata/ directory
    for (GSList *elem = modifyrepotasks; elem; elem = g_slist_next(elem)) {
        cr_ModifyRepoTask *task = elem->data;
        _cleanup_free_ gchar *dst_fn = NULL;

        if (task->remove)
            // Skip removing task
            continue;

        cr_CompressionType compress_type = CR_CW_NO_COMPRESSION;

        if (task->compress)
            compress_type = task->compress_type;

        dst_fn = cr_write_file(repopath, task, compress_type, err);
        if (dst_fn == NULL) {
            cr_repomd_free(repomd);
            g_free(repomd_path);
            return FALSE;
        }
        
        task->repopath = cr_safe_string_chunk_insert_null(task->chunk, dst_fn);
#ifdef WITH_ZCHUNK
        if (task->zck) {
            free(dst_fn);
            dst_fn = cr_write_file(repopath, task, CR_CW_ZCK_COMPRESSION, err);
            if (dst_fn == NULL) {
                cr_repomd_free(repomd);
                g_free(repomd_path);
                return FALSE;
            }
            task->zck_repopath = cr_safe_string_chunk_insert_null(task->chunk, dst_fn);
        }
#endif
    }

    // Prepare new repomd records
    GSList *repomdrecords = NULL;
    GSList *repomdrecords_uniquefn = NULL;
    GSList *repomdrecordfilltasks = NULL;

    GThreadPool *fill_pool = g_thread_pool_new(cr_repomd_record_fill_thread,
                                               NULL, 5, FALSE, NULL);

    for (GSList *elem = modifyrepotasks; elem; elem = g_slist_next(elem)) {
        cr_ModifyRepoTask *task = elem->data;

        if (task->remove)
            continue;

        cr_RepomdRecord *rec = cr_repomd_record_new(task->type,
                                                    task->repopath);
        cr_RepomdRecordFillTask *filltask = cr_repomdrecordfilltask_new(rec,
                                            task->checksum_type, NULL);
        g_thread_pool_push(fill_pool, filltask, NULL);

        repomdrecords = g_slist_append(repomdrecords, rec); 

        if (task->unique_md_filenames)
            repomdrecords_uniquefn = g_slist_prepend(repomdrecords_uniquefn, rec);
        repomdrecordfilltasks = g_slist_prepend(repomdrecordfilltasks,
                                                filltask);
        if (task->zck) {
            _cleanup_free_ gchar *type = g_strconcat(task->type, "_zck", NULL);
            rec = cr_repomd_record_new(type, task->zck_repopath);
            filltask = cr_repomdrecordfilltask_new(rec, task->checksum_type, NULL);
            g_thread_pool_push(fill_pool, filltask, NULL);

            repomdrecords = g_slist_append(repomdrecords, rec);

            if (task->unique_md_filenames)
                repomdrecords_uniquefn = g_slist_prepend(repomdrecords_uniquefn, rec);
            repomdrecordfilltasks = g_slist_prepend(repomdrecordfilltasks,
                                                    filltask);
        }
    }

    g_thread_pool_free(fill_pool, FALSE, TRUE); // Wait

    for (GSList *elem = repomdrecordfilltasks; elem; elem = g_slist_next(elem)) {
        // Clean up tasks
        cr_RepomdRecordFillTask *filltask = elem->data;
        cr_repomdrecordfilltask_free(filltask, NULL);
    }
    g_slist_free(repomdrecordfilltasks);

    // Detach records from repomd
    GSList *recordstoremove = NULL;
    for (GSList *elem = modifyrepotasks; elem; elem = g_slist_next(elem)) {
        cr_ModifyRepoTask *task = elem->data;

        // Remove both, records that will be removed but also
        // records with types that will be added.
        cr_RepomdRecord *rec = cr_repomd_get_record(repomd, task->type);
        if (rec) {
            g_debug("%s: Removing record \"%s\" from repomd.xml",
                    __func__, task->type);
            recordstoremove = g_slist_prepend(recordstoremove, rec);
            cr_repomd_detach_record(repomd, rec);
            if (task->zck) {
                _cleanup_free_ gchar *type = g_strconcat(task->type, "_zck", NULL);
                cr_RepomdRecord *rec = cr_repomd_get_record(repomd, type);
                if (rec) {
                    g_debug("%s: Removing record \"%s\" from repomd.xml",
                            __func__, type);
                    recordstoremove = g_slist_prepend(recordstoremove, rec);
                    cr_repomd_detach_record(repomd, rec);
                }
            }
        }
    }

    // Prepend checksum
    for (GSList *elem = repomdrecords_uniquefn;
         elem;
         elem = g_slist_next(elem))
    {
        cr_RepomdRecord *rec = elem->data;
        cr_repomd_record_rename_file(rec, NULL);
    }
    g_slist_free(repomdrecords_uniquefn);

    // Add records into repomd
    for (GSList *elem = repomdrecords; elem; elem = g_slist_next(elem)) {
        cr_RepomdRecord *rec = elem->data;
        g_debug("Adding record \"%s\"", rec->type);
        cr_repomd_set_record(repomd, rec);
    }
    g_slist_free(repomdrecords);

    // Write repomd.xml
    cr_repomd_sort_records(repomd);
    gchar *repomd_xml = cr_xml_dump_repomd(repomd, NULL);
    g_debug("Generated repomd.xml:\n%s", repomd_xml);

    g_debug("%s: Writing modified %s", __func__, repomd_path);
    gboolean ret = cr_write_to_file(err, repomd_path, "%s", repomd_xml);

    g_free(repomd_xml);
    g_free(repomd_path);

    if (!ret) {
        assert(!err || *err);
        cr_repomd_free(repomd);
        return FALSE;
    }

    // Delete files of removed records
    for (GSList *elem = recordstoremove; elem; elem = g_slist_next(elem)) {
        cr_RepomdRecord *rec = elem->data;

        if (rec->location_base)
            // Do not even try to remove records with base location
            continue;

        // Construct filename
        _cleanup_free_ gchar *realpath = g_build_filename(repopath,
                                         "../",
                                         rec->location_href,
                                         NULL);

        // Firstly check if the file that should be deleted isn't
        // really used by other record anymore.
        // It could happend if user add a file, that already exists,
        // in repodata. Then we don't want to remove this file.
        gboolean remove_this = TRUE;

        // Check if a file that is referenced by a record that should
        // be removed belongs to any other record (a record that
        // shouldn't be removed).
        for (GSList *e = repomd->records; e; e = g_slist_next(e)) {
            cr_RepomdRecord *lrec = e->data;
            _cleanup_free_ gchar *lrealpath = NULL;

            // Construct filename
            lrealpath = g_build_filename(repopath,
                                         "../",
                                         lrec->location_href,
                                         NULL);

            // Check if files are identical
            gboolean identical = FALSE;
            if (!cr_identical_files(realpath, lrealpath, &identical, err))
                return FALSE;

            // If yes, do not remove it
            if (identical) {
                remove_this = FALSE;
                break;
            }
        }

        if (!remove_this)
            continue;

        g_debug("%s: Removing \"%s\"", __func__, realpath);

        if (remove(realpath) == -1)
            g_warning("Cannot remove \"%s\": %s", realpath, g_strerror(errno));
    }
    cr_slist_free_full(recordstoremove, (GDestroyNotify)cr_repomd_record_free);

    cr_repomd_free(repomd);

    return TRUE;
}

gboolean
cr_modifyrepo_parse_batchfile(const gchar *path,
                              GSList **modifyrepotasks,
                              GError **err)
{
    assert(!err || *err == NULL);

    if (!path)
        return TRUE;

    GKeyFile *keyfile = g_key_file_new();

    gboolean ret = TRUE;
    ret = g_key_file_load_from_file(keyfile, path, G_KEY_FILE_NONE, err);
    if (!ret) {
        g_debug("%s: Parsing of modifyrepo batchfile failed", __func__);
        return FALSE;
    }

    gsize length;
    gchar **groups = g_key_file_get_groups(keyfile, &length);
    GSList *tasks = NULL;
    gboolean success = TRUE;
    for (gsize x = 0; x < length; x++) {
        gchar *group = groups[x];
        assert(group);

        g_debug("%s: Group: \"%s\"", __func__, group);

        cr_ModifyRepoTask *task = cr_modifyrepotask_new();
        tasks = g_slist_append(tasks, task);

        gchar *tmp_str;

        // As path use path option value or group
        // name if no path option specified
        task->path = cr_safe_string_chunk_insert_and_free(task->chunk,
                g_key_file_get_string(keyfile, group, "path", NULL));
        if (!task->path)
            task->path = cr_safe_string_chunk_insert(task->chunk, group);

        task->type = cr_safe_string_chunk_insert_and_free(task->chunk,
                    g_key_file_get_string(keyfile, group, "type", NULL));
        task->remove = cr_key_file_get_boolean_default(keyfile, group,
                   "remove", FALSE, NULL);
        task->compress = cr_key_file_get_boolean_default(keyfile, group,
                   "compress", TRUE, NULL);
        tmp_str = g_key_file_get_string(keyfile, group, "compress-type", NULL);
        task->compress_type = cr_compression_type(tmp_str);
        g_free(tmp_str);
        task->unique_md_filenames = cr_key_file_get_boolean_default(keyfile,
                    group, "unique-md-filenames", TRUE, NULL);
        tmp_str = g_key_file_get_string(keyfile, group, "checksum", NULL);
        task->checksum_type = cr_checksum_type(tmp_str);
        g_free(tmp_str);
        task->new_name = cr_safe_string_chunk_insert_and_free(task->chunk,
                    g_key_file_get_string(keyfile, group, "new-name", NULL));

        g_debug("Task: [path: %s, type: %s, remove: %d, compress: %d, "
                "compress_type: %d (%s), unique_md_filenames: %d, "
                "checksum_type: %d (%s), new_name: %s]",
                task->path, task->type, task->remove, task->compress,
                task->compress_type, cr_compression_suffix(task->compress_type),
                task->unique_md_filenames, task->checksum_type,
                cr_checksum_name_str(task->checksum_type), task->new_name);

    }

    g_strfreev(groups);

    if (success) {
        *modifyrepotasks = g_slist_concat(*modifyrepotasks, tasks);
    } else {
        cr_slist_free_full(tasks, (GDestroyNotify)cr_modifyrepotask_free);
    }

    g_key_file_free(keyfile);
    return success;
}
