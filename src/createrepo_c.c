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
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include "cmd_parser.h"
#include "compression_wrapper.h"
#include "createrepo_shared.h"
#include "deltarpms.h"
#include "dumper_thread.h"
#include "checksum.h"
#include "cleanup.h"
#include "error.h"
#include "helpers.h"
#include "load_metadata.h"
#include "metadata_internal.h"
#include "locate_metadata.h"
#include "misc.h"
#include "parsepkg.h"
#include "repomd.h"
#include "sqlite.h"
#include "threads.h"
#include "version.h"
#include "xml_dump.h"
#include "xml_file.h"

#ifdef WITH_LIBMODULEMD
#include <modulemd.h>
#endif /* WITH_LIBMODULEMD */

#define OUTDELTADIR "drpms/"

/** Check if the filename is excluded by any exclude mask.
 * @param filename      Filename (basename).
 * @param exclude_masks List of exclude masks
 * @return              TRUE if file should be included, FALSE otherwise
 */
static gboolean
allowed_file(const gchar *filename, GSList *exclude_masks)
{
    // Check file against exclude glob masks
    if (exclude_masks) {
        int str_len = strlen(filename);
        gchar *reversed_filename = g_utf8_strreverse(filename, str_len);

        GSList *element = exclude_masks;
        for (; element; element=g_slist_next(element)) {
            if (g_pattern_match((GPatternSpec *) element->data,
                                str_len, filename, reversed_filename))
            {
                g_free(reversed_filename);
                g_debug("Exclude masks hit - skipping: %s", filename);
                return FALSE;
            }
        }
        g_free(reversed_filename);
    }
    return TRUE;
}

static gboolean
allowed_modulemd_module_metadata_file(const gchar *filename)
{
    if (g_strrstr(filename, "modules.yaml"))
        return TRUE;
    if (g_strrstr(filename, ".modulemd.yaml"))
        return TRUE;
    if (g_strrstr(filename, ".modulemd-defaults.yaml"))
        return TRUE;

    return FALSE;
}


/** Function used to sort pool tasks.
 * This function is responsible for order of packages in metadata.
 *
 * @param a_p           Pointer to first struct PoolTask
 * @param b_p           Pointer to second struct PoolTask
 * @param user_data     Unused (user data)
 */
static int
task_cmp(gconstpointer a_p, gconstpointer b_p, G_GNUC_UNUSED gpointer user_data)
{
    int ret;
    const struct PoolTask *a = a_p;
    const struct PoolTask *b = b_p;
    ret = g_strcmp0(a->filename, b->filename);
    if (ret) return ret;
    return g_strcmp0(a->path, b->path);
}


/** Recursively walkt throught the input directory and add push the found
 * rpms to the thread pool (create a PoolTask and push it to the pool).
 * If the filelists is supplied then no recursive walk is done and only
 * files from filelists are pushed into the pool.
 * This function also filters out files that shouldn't be processed
 * (e.g. directories with .rpm suffix, files that match one of
 * the exclude masks, etc.).
 *
 * @param pool              GThreadPool pool
 * @param in_dir            Directory to scan
 * @param cmd_options       Options specified on command line
 * @param current_pkglist   Pointer to a list where basenames of files that
 *                          will be processed will be appended to.
 * @return                  Number of packages that are going to be processed
 */
static long
fill_pool(GThreadPool *pool,
          gchar *in_dir,
          struct CmdOptions *cmd_options,
          GSList **current_pkglist,
          long *task_count,
          int  media_id)
{
    GQueue queue = G_QUEUE_INIT;
    struct PoolTask *task;

    if ( ! cmd_options->split ) {
        media_id = 0;
    }


    if ((cmd_options->pkglist || cmd_options->recycle_pkglist) && !cmd_options->include_pkgs) {
        g_warning("Used pkglist doesn't contain any useful items");
    } else if (!(cmd_options->include_pkgs)) {
        // --pkglist (or --includepkg, or --recycle-pkglist) is not supplied
        //  --> do dir walk

        g_message("Directory walk started");

        size_t in_dir_len = strlen(in_dir);
        GStringChunk *sub_dirs_chunk = g_string_chunk_new(1024);
        GQueue *sub_dirs = g_queue_new();
        gchar *input_dir_stripped;

        input_dir_stripped = g_string_chunk_insert_len(sub_dirs_chunk,
                                                       in_dir,
                                                       in_dir_len-1);
        g_queue_push_head(sub_dirs, input_dir_stripped);

        char *dirname;
        while ((dirname = g_queue_pop_head(sub_dirs))) {
            // Open dir
            GDir *dirp;
            dirp = g_dir_open (dirname, 0, NULL);
            if (!dirp) {
                g_warning("Cannot open directory: %s", dirname);
                continue;
            }

            const gchar *filename;
            while ((filename = g_dir_read_name(dirp))) {
                if (!allowed_file(filename, cmd_options->exclude_masks)) {
                    continue;
                }

                gchar *full_path = g_strconcat(dirname, "/", filename, NULL);

                if (!g_file_test(full_path, G_FILE_TEST_IS_REGULAR)) {
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

                // Skip symbolic links if --skip-symlinks arg is used
                if (cmd_options->skip_symlinks
                    && g_file_test(full_path, G_FILE_TEST_IS_SYMLINK))
                {
                    g_debug("Skipped symlink: %s", full_path);
                    g_free(full_path);
                    continue;
                }

                if (allowed_modulemd_module_metadata_file(full_path)) {
#ifdef WITH_LIBMODULEMD
                    cmd_options->modulemd_metadata = g_slist_prepend(
                        cmd_options->modulemd_metadata,
                        (gpointer) full_path);
#else
                    g_warning("createrepo_c not compiled with libmodulemd support, "
                              "ignoring found module metadata: %s", full_path);
                    g_free(full_path);
#endif /* WITH_LIBMODULEMD */
                    continue;
                }

                // Non .rpm files are ignored
                if (!g_str_has_suffix (filename, ".rpm")) {
                    g_free(full_path);
                    continue;
                }

                // Check filename against exclude glob masks
                const gchar *repo_relative_path = filename;
                if (in_dir_len < strlen(full_path))
                    // This probably should be always true
                    repo_relative_path = full_path + in_dir_len;

                if (allowed_file(repo_relative_path, cmd_options->exclude_masks)) {
                    // FINALLY! Add file into pool
                    g_debug("Adding pkg: %s", full_path);
                    task = g_malloc(sizeof(struct PoolTask));
                    task->full_path = full_path;
                    task->filename = g_strdup(filename);
                    task->path = g_strdup(dirname);
                    *current_pkglist = g_slist_prepend(*current_pkglist, task->filename);
                    // TODO: One common path for all tasks with the same path?
                    g_queue_insert_sorted(&queue, task, task_cmp, NULL);
                } else {
                    g_free(full_path);
                }
            }

            // Cleanup
            g_dir_close (dirp);
        }

        g_string_chunk_free (sub_dirs_chunk);
        g_queue_free(sub_dirs);
    } else {
        // pkglist is supplied - use only files in pkglist

        g_debug("Skipping dir walk - using pkglist");

        GSList *element = cmd_options->include_pkgs;
        for (; element; element=g_slist_next(element)) {
            gchar *relative_path = (gchar *) element->data;
            //     ^^^ path from pkglist e.g. packages/i386/foobar.rpm

            if (allowed_modulemd_module_metadata_file(relative_path)) {
#ifdef WITH_LIBMODULEMD
                cmd_options->modulemd_metadata = g_slist_prepend(
                    cmd_options->modulemd_metadata,
                    (gpointer) g_strdup(relative_path));
#else
            g_warning("createrepo_c not compiled with libmodulemd support, "
                      "ignoring found module metadata: %s", relative_path);
#endif /* WITH_LIBMODULEMD */
                continue;
            }

            gchar *filename; // foobar.rpm

            // Get index of last '/'
            int x = strlen(relative_path);
            for (; x > 0 && relative_path[x] != '/'; x--)
                ;

            if (!x) // There was no '/' in path
                filename = relative_path;
            else    // Use only a last part of the path
                filename = relative_path + x + 1;

            if (allowed_file(relative_path, cmd_options->exclude_masks)) {
                // Check filename against exclude glob masks
                gchar *full_path = g_strconcat(in_dir, relative_path, NULL);
                //     ^^^ /path/to/in_repo/packages/i386/foobar.rpm
                g_debug("Adding pkg: %s", full_path);
                task = g_malloc(sizeof(struct PoolTask));
                task->full_path = full_path;
                task->filename  = g_strdup(filename);         // foobar.rpm
                task->path      = strndup(relative_path, x);  // packages/i386/
                *current_pkglist = g_slist_prepend(*current_pkglist, task->filename);
                g_queue_insert_sorted(&queue, task, task_cmp, NULL);
            }
        }
    }

    // Push sorted tasks into the thread pool
    while ((task = g_queue_pop_head(&queue)) != NULL) {
        task->id = *task_count;
        task->media_id = media_id;
        g_thread_pool_push(pool, task, NULL);
        ++*task_count;
    }

    return *task_count;
}


/** Prepare cache dir for checksums.
 * Called only if --cachedir options is used.
 * It tries to create cache directory if it doesn't exist yet.
 * It also fill checksum_cachedir option in cmd_options structure.
 *
 * @param cmd_options       Commandline options
 * @param out_dir           Repo output directory
 * @param err               GError **
 * @return                  FALSE if err is set, TRUE otherwise
 */
static gboolean
prepare_cache_dir(struct CmdOptions *cmd_options,
                  const gchar *out_dir,
                  GError **err)
{
    if (!cmd_options->cachedir)
        return TRUE;

    if (g_str_has_prefix(cmd_options->cachedir, "/")) {
        // Absolute local path
        cmd_options->checksum_cachedir = cr_normalize_dir_path(
                                                    cmd_options->cachedir);
    } else {
        // Relative path (from intput_dir)
        gchar *tmp = g_strconcat(out_dir, cmd_options->cachedir, NULL);
        cmd_options->checksum_cachedir = cr_normalize_dir_path(tmp);
        g_free(tmp);
    }

    // Create the cache directory
    if (g_mkdir(cmd_options->checksum_cachedir, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH)) {
        if (errno == EEXIST) {
            if (!g_file_test(cmd_options->checksum_cachedir,
                             G_FILE_TEST_IS_DIR))
            {
                g_set_error(err, CREATEREPO_C_ERROR, CRE_BADARG,
                            "The %s already exists and it is not a directory!",
                            cmd_options->checksum_cachedir);
                return FALSE;
            }
        } else {
            g_set_error(err, CREATEREPO_C_ERROR, CRE_BADARG,
                        "cannot use cachedir %s: %s",
                        cmd_options->checksum_cachedir, g_strerror(errno));
            return FALSE;
        }
    }

    g_debug("Cachedir for checksums is %s", cmd_options->checksum_cachedir);
    return TRUE;
}

/** Adds groupfile cr_RepomdRecords to additional_metadata_rec list.
 *  Groupfile is a special case, because it's the only metadatum
 *  that can be inputed to createrepo_c via command line option.
 *
 * @param group_metadatum           Cr_Metadatum for used groupfile
 * @param additional_metadata_rec   GSList of cr_RepomdRecords
 * @param comp_type                 Groupfile compression type
 * @param repomd_checksum_type
 *
 * @return                          GSList with added cr_RepomdRecords for
 *                                  groupfile
 */
GSList*
cr_create_repomd_records_for_groupfile_metadata(const cr_Metadatum *group_metadatum,
                                                GSList *additional_metadata_rec,
                                                cr_CompressionType comp_type,
                                                cr_ChecksumType repomd_checksum_type)
{
    GError *tmp_err = NULL;
    char *compression_suffix = g_strdup(cr_compression_suffix(comp_type));
    compression_suffix[0] = '_'; //replace '.'
    additional_metadata_rec = g_slist_prepend(additional_metadata_rec,
                                              cr_repomd_record_new(
                                                  group_metadatum->type,
                                                  group_metadatum->name
                                              ));

    gchar *compressed_record_type = g_strconcat(group_metadatum->type, compression_suffix, NULL);
    additional_metadata_rec = g_slist_prepend(additional_metadata_rec,
                                              cr_repomd_record_new(
                                                  compressed_record_type,
                                                  NULL
                                              ));

    cr_repomd_record_compress_and_fill(additional_metadata_rec->next->data,
                                       additional_metadata_rec->data,
                                       repomd_checksum_type,
                                       comp_type,
                                       NULL,
                                       &tmp_err);

    if (tmp_err) {
        g_critical("Cannot process %s %s: %s",
                   group_metadatum->type,
                   group_metadatum->name,
                   tmp_err->message);
        g_free(compression_suffix);
        g_free(compressed_record_type);
        g_clear_error(&tmp_err);
        exit(EXIT_FAILURE);
    }

    g_free(compressed_record_type);
    g_free(compression_suffix);

    return additional_metadata_rec;
}

/** Creates list of cr_RepomdRecords from list
 *  of additional metadata (cr_Metadatum) 
 *
 * @param additional_metadata       List of cr_Metadatum
 * @param repomd_checksum_type      
 *
 * @return                          New GSList of cr_RepomdRecords
 */
static GSList*
cr_create_repomd_records_for_additional_metadata(GSList *additional_metadata,
                                                 cr_ChecksumType repomd_checksum_type)
{
    GError *tmp_err = NULL;
    GSList *additional_metadata_rec = NULL; 
    GSList *element = additional_metadata;
    for (; element; element=g_slist_next(element)) {
        additional_metadata_rec = g_slist_prepend(additional_metadata_rec,
                                                  cr_repomd_record_new(
                                                      ((cr_Metadatum *) element->data)->type,
                                                      ((cr_Metadatum *) element->data)->name
                                                  ));
        cr_repomd_record_fill(additional_metadata_rec->data,
                              repomd_checksum_type,
                              &tmp_err);

        if (tmp_err) {
            g_critical("Cannot process %s %s: %s",
                       ((cr_Metadatum *) element->data)->type,
                       ((cr_Metadatum *) element->data)->name,
                       tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }
    }

    return additional_metadata_rec;
}

/** Check if task finished without error, if yes
 *  use content stats of the new file
 *
 * @param task          Rewrite pkg count task
 * @param filename      Name of file with wrong package count 
 * @param exit_val      If errors occured set createrepo_c exit value
 * @param content_stat  Content stats for filename    
 *
 */
static void
error_check_and_set_content_stat(cr_CompressionTask *task, char *filename, int *exit_val, cr_ContentStat **content_stat){
    if (task->err) {
        g_critical("Cannot rewrite pkg count in %s: %s",
                   filename, task->err->message);
        *exit_val = 2;
    }else{
        cr_contentstat_free(*content_stat, NULL);
        *content_stat = task->stat;
        task->stat = NULL;
    }
}

static void
load_old_metadata(cr_Metadata **md,
                  struct cr_MetadataLocation **md_location,
                  GSList *current_pkglist,
                  struct CmdOptions *cmd_options,
                  gchar *dir,
                  GThreadPool *pool,
                  GError *tmp_err)
{
    *md_location = cr_locate_metadata(dir, TRUE, &tmp_err);
    if (tmp_err) {
        if (tmp_err->domain == CRE_MODULEMD) {
            g_thread_pool_free(pool, FALSE, FALSE);
            g_clear_pointer(md_location, cr_metadatalocation_free);
            g_critical("%s\n",tmp_err->message);
            exit(tmp_err->code);
        } else {
            g_debug("Old metadata from default outputdir not found: %s",tmp_err->message);
            g_clear_error(&tmp_err);
        }
    }

    *md = cr_metadata_new(CR_HT_KEY_HREF, 1, current_pkglist);
    cr_metadata_set_dupaction(*md, CR_HT_DUPACT_REMOVEALL);

    int ret;

    if (*md_location) {
        ret = cr_metadata_load_xml(*md, *md_location, &tmp_err);
        assert(ret == CRE_OK || tmp_err);

        if (ret == CRE_OK) {
            g_debug("Old metadata from: %s - loaded",
                    (*md_location)->original_url);
        } else {
            g_debug("Old metadata from %s - loading failed: %s",
                    (*md_location)->original_url, tmp_err->message);
            g_clear_error(&tmp_err);
        }
    }

    // Load repodata from --update-md-path
    GSList *element = cmd_options->l_update_md_paths;
    for (; element; element = g_slist_next(element)) {
        char *path = (char *) element->data;
        g_message("Loading metadata from md-path: %s", path);

        ret = cr_metadata_locate_and_load_xml(*md, path, &tmp_err);
        assert(ret == CRE_OK || tmp_err);

        if (ret == CRE_OK) {
            g_debug("Metadata from md-path %s - loaded", path);
        } else {
            g_warning("Metadata from md-path %s - loading failed: %s",
                      path, tmp_err->message);
            g_clear_error(&tmp_err);
        }
    }

    g_message("Loaded information about %d packages",
              g_hash_table_size(cr_metadata_hashtable(*md)));
}

int
main(int argc, char **argv)
{
    struct CmdOptions *cmd_options;
    gboolean ret;
    GError *tmp_err = NULL;
    int exit_val = EXIT_SUCCESS;

    // Arguments parsing
    cmd_options = parse_arguments(&argc, &argv, &tmp_err);
    if (!cmd_options) {
        g_printerr("Argument parsing failed: %s\n", tmp_err->message);
        g_error_free(tmp_err);
        exit(EXIT_FAILURE);
    }

    // Arguments pre-check
    if (cmd_options->version) {
        // Just print version
        printf("Version: %s\n", cr_version_string_with_features());
        free_options(cmd_options);
        exit(EXIT_SUCCESS);
    }

    if ( cmd_options->split ) {
        if (argc < 2) {
            g_printerr("Must specify at least one directory to index.\n");
            g_printerr("Usage: %s [options] <directory_to_index> [directory_to_index] ...\n\n",
                     cr_get_filename(argv[0]));
            free_options(cmd_options);
            exit(EXIT_FAILURE);
        }
    } else {
        if (argc != 2) {
            // No mandatory arguments
            g_printerr("Must specify exactly one directory to index.\n");
            g_printerr("Usage: %s [options] <directory_to_index>\n\n",
                     cr_get_filename(argv[0]));
            free_options(cmd_options);
            exit(EXIT_FAILURE);
        }
    }

    // Dirs
    gchar *in_dir       = NULL;  // path/to/repo/
    gchar *in_repo      = NULL;  // path/to/repo/repodata/
    gchar *out_dir      = NULL;  // path/to/out_repo/
    gchar *out_repo     = NULL;  // path/to/out_repo/repodata/
    gchar *tmp_out_repo = NULL;  // usually path/to/out_repo/.repodata/
    gchar *lock_dir     = NULL;  // path/to/out_repo/.repodata/

    if (cmd_options->basedir && !g_str_has_prefix(argv[1], "/")) {
        gchar *tmp = cr_normalize_dir_path(argv[1]);
        in_dir = g_build_filename(cmd_options->basedir, tmp, NULL);
        g_free(tmp);
    } else {
        in_dir = cr_normalize_dir_path(argv[1]);
    }

    // Check if inputdir exists
    if (!g_file_test(in_dir, G_FILE_TEST_IS_DIR)) {
        g_printerr("Directory %s must exist\n", in_dir);
        g_free(in_dir);
        free_options(cmd_options);
        exit(EXIT_FAILURE);
    }


    // Check parsed arguments
    if (!check_arguments(cmd_options, in_dir, &tmp_err)) {
        g_printerr("%s\n", tmp_err->message);
        g_error_free(tmp_err);
        g_free(in_dir);
        free_options(cmd_options);
        exit(EXIT_FAILURE);
    }

    // Set logging stuff
    cr_setup_logging(cmd_options->quiet, cmd_options->verbose);

    // Emit debug message with version
    g_debug("Version: %s", cr_version_string_with_features());

    // Set paths of input and output repos
    in_repo = g_strconcat(in_dir, "repodata/", NULL);

    if (cmd_options->outputdir) {
        out_dir = cr_normalize_dir_path(cmd_options->outputdir);
        out_repo = g_strconcat(out_dir, "repodata/", NULL);
    } else {
        out_dir  = g_strdup(in_dir);
        out_repo = g_strdup(in_repo);
    }

    // Prepare cachedir for checksum if --cachedir is used
    if (!prepare_cache_dir(cmd_options, out_dir, &tmp_err)) {
        g_printerr("%s\n", tmp_err->message);
        g_error_free(tmp_err);
        g_free(in_dir);
        g_free(in_repo);
        g_free(out_dir);
        g_free(out_repo);
        free_options(cmd_options);
        exit(EXIT_FAILURE);
    }

    // Block signals that terminates the process
    if (!cr_block_terminating_signals(&tmp_err)) {
        g_printerr("%s\n", tmp_err->message);
        exit(EXIT_FAILURE);
    }

    // Check if lock exists & Create lock dir
    if (!cr_lock_repo(out_dir, cmd_options->ignore_lock, &lock_dir, &tmp_out_repo, &tmp_err)) {
        g_printerr("%s\n", tmp_err->message);
        exit(EXIT_FAILURE);
    }

    // Setup cleanup handlers
    if (!cr_set_cleanup_handler(lock_dir, tmp_out_repo, &tmp_err)) {
        g_printerr("%s\n", tmp_err->message);
        exit(EXIT_FAILURE);
    }

    // Unblock the blocked signals
    if (!cr_unblock_terminating_signals(&tmp_err)) {
        g_printerr("%s\n", tmp_err->message);
        exit(EXIT_FAILURE);
    }

    // Open package list
    FILE *output_pkg_list = NULL;
    if (cmd_options->read_pkgs_list) {
        output_pkg_list = fopen(cmd_options->read_pkgs_list, "w");
        if (!output_pkg_list) {
            g_critical("Cannot open \"%s\" for writing: %s",
                       cmd_options->read_pkgs_list, g_strerror(errno));
            exit(EXIT_FAILURE);
        }
    }


    // Init package parser
    cr_package_parser_init();
    cr_xml_dump_init();

    // Thread pool - Creation
    struct UserData user_data = {0};
    GThreadPool *pool = g_thread_pool_new(cr_dumper_thread,
                                          &user_data,
                                          0,
                                          TRUE,
                                          NULL);
    g_debug("Thread pool ready");

    long task_count = 0;
    GSList *current_pkglist = NULL;
    /* ^^^ List with basenames of files which will be processed */

    // Load old metadata if --update
    struct cr_MetadataLocation *old_metadata_location = NULL;
    cr_Metadata *old_metadata = NULL;

    gchar *old_metadata_dir = cmd_options->outputdir ? out_dir : in_dir;

    if (cmd_options->recycle_pkglist) {
        // load the old metadata early, so we can read the list of RPMs
        load_old_metadata(&old_metadata,
                          &old_metadata_location,
                          NULL /* no filter wanted in this case */,
                          cmd_options,
                          old_metadata_dir,
                          pool,
                          tmp_err);

        GHashTableIter iter;
        g_hash_table_iter_init(&iter, cr_metadata_hashtable(old_metadata));
        gpointer pkg_pointer;
        while (g_hash_table_iter_next(&iter, NULL, &pkg_pointer)) {
            cr_Package *pkg = (cr_Package *)pkg_pointer;
            cmd_options->include_pkgs = g_slist_prepend(
                    cmd_options->include_pkgs,
                    (gpointer) g_strdup(pkg->location_href));
        }
    }

    for (int media_id = 1; media_id < argc; media_id++ ) {
        gchar *tmp_in_dir = cr_normalize_dir_path(argv[media_id]);
        // Thread pool - Fill with tasks
        fill_pool(pool,
                  tmp_in_dir,
                  cmd_options,
                  &current_pkglist,
                  &task_count,
                  media_id);
        g_free(tmp_in_dir);
    }

    g_debug("Package count: %ld", task_count);
    g_message("Directory walk done - %ld packages", task_count);

    if (cmd_options->update) {
        if (old_metadata)
            g_debug("Old metadata already loaded.");
        else if (!task_count)
            g_debug("No packages found - skipping metadata loading");
        else
            load_old_metadata(&old_metadata,
                              &old_metadata_location,
                              current_pkglist,
                              cmd_options,
                              old_metadata_dir,
                              pool,
                              tmp_err);
    }

    g_slist_free(current_pkglist);
    current_pkglist = NULL;
    GSList *additional_metadata = NULL;

    // Setup compression types
    const char *xml_compression_suffix = NULL;
    const char *sqlite_compression_suffix = NULL;
    const char *compression_suffix = NULL;
    cr_CompressionType xml_compression = CR_CW_GZ_COMPRESSION;
    cr_CompressionType sqlite_compression = CR_CW_BZ2_COMPRESSION;
    cr_CompressionType compression = CR_CW_GZ_COMPRESSION;

    if (cmd_options->compression_type != CR_CW_UNKNOWN_COMPRESSION) {
        sqlite_compression = cmd_options->compression_type;
        compression        = cmd_options->compression_type;
    }

    if (cmd_options->general_compression_type != CR_CW_UNKNOWN_COMPRESSION) {
        xml_compression    = cmd_options->general_compression_type;
        sqlite_compression = cmd_options->general_compression_type;
        compression        = cmd_options->general_compression_type;
    }

    xml_compression_suffix = cr_compression_suffix(xml_compression);
    sqlite_compression_suffix = cr_compression_suffix(sqlite_compression);
    compression_suffix = cr_compression_suffix(compression);

    cr_Metadatum *new_groupfile_metadatum = NULL;

    // Groupfile specified as argument 
    if (cmd_options->groupfile_fullpath) {
        new_groupfile_metadatum = g_malloc0(sizeof(cr_Metadatum));
        new_groupfile_metadatum->name = cr_copy_metadatum(cmd_options->groupfile_fullpath, tmp_out_repo, &tmp_err);
        new_groupfile_metadatum->type = g_strdup("group");
        //remove old groupfile(s) (every [compressed] variant)
        if (old_metadata_location){
            GSList *node_iter = old_metadata_location->additional_metadata;
            while (node_iter != NULL){
                cr_Metadatum *m = node_iter->data;
                GSList *next = g_slist_next(node_iter);
                if(g_str_has_prefix(m->type, "group")){
                    old_metadata_location->additional_metadata = g_slist_delete_link(old_metadata_location->additional_metadata,
                                                                                     node_iter);
                    cr_metadatum_free(m);
                }
                node_iter = next;
            }
        }
    }

#ifdef WITH_LIBMODULEMD
    // module metadata found in repo
    if (cmd_options->modulemd_metadata) {
        gboolean merger_is_empty = TRUE;
        ModulemdModuleIndexMerger *merger = modulemd_module_index_merger_new();
        if (!merger) {
            g_critical("Could not allocate module merger");
            exit(EXIT_FAILURE);
        }

        if (cmd_options->update && old_metadata_location && old_metadata_location->additional_metadata){
            //associate old metadata into the merger if we want to keep them (--keep-all-metadata)
            if (cr_metadata_modulemd(old_metadata) && cmd_options->keep_all_metadata){
                modulemd_module_index_merger_associate_index(merger, cr_metadata_modulemd(old_metadata), 0);
                merger_is_empty = FALSE;
                if (tmp_err) {
                    g_critical("%s: Cannot merge old module index with new: %s", __func__, tmp_err->message);
                    g_clear_error(&tmp_err);
                    g_clear_pointer(&merger, g_object_unref);
                    exit(EXIT_FAILURE);
                }
            }
            //remove old modules (every [compressed] variant)
            GSList *node_iter = old_metadata_location->additional_metadata;
            while (node_iter != NULL){
                GSList *next = g_slist_next(node_iter);
                cr_Metadatum *m = node_iter->data;

                /* If we are updating some existing repodata that have modular metadata
                 * remove those from found cmd_options->modulemd_metadata.
                 * If --keel-all-metadata is not specified we don't want them and if it is they
                 * were already added from old_metadata module index above.
                 */
                GSList *element_iter = cmd_options->modulemd_metadata;
                while (element_iter != NULL){
                    GSList *next_inner = g_slist_next(element_iter);
                    gchar *path_to_found_md = (gchar *) element_iter->data;
                    if (!g_strcmp0(path_to_found_md, m->name)) {
                        cmd_options->modulemd_metadata = g_slist_delete_link(
                            cmd_options->modulemd_metadata, element_iter);
                    }
                    element_iter = next_inner;
                }

                if(g_str_has_prefix(m->type, "modules")){
                    old_metadata_location->additional_metadata = g_slist_delete_link(
                        old_metadata_location->additional_metadata, node_iter);
                    cr_metadatum_free(m);
                }
                node_iter = next;
            }
        }

        ModulemdModuleIndex *moduleindex;

        //load all found module metatada and associate it with merger
        GSList *element = cmd_options->modulemd_metadata;
        for (; element; element=g_slist_next(element)) {
            int result = cr_metadata_load_modulemd(&moduleindex, (char *) element->data, &tmp_err);
            if (result != CRE_OK) {
                g_critical("Could not load module index file %s: %s", (char *) element->data,
                           (tmp_err ? tmp_err->message : "Unknown error"));
                g_clear_error(&tmp_err);
                g_clear_pointer(&moduleindex, g_object_unref);
                g_clear_pointer(&merger, g_object_unref);
                exit(EXIT_FAILURE);
            }

            modulemd_module_index_merger_associate_index(merger, moduleindex, 0);
            merger_is_empty = FALSE;
            g_clear_pointer(&moduleindex, g_object_unref);
        }

        if (!merger_is_empty) {
            //merge module metadata and dump it to string
            moduleindex = modulemd_module_index_merger_resolve (merger, &tmp_err);
            char *moduleindex_str = modulemd_module_index_dump_to_string (moduleindex, &tmp_err);
            g_clear_pointer(&moduleindex, g_object_unref);
            if (tmp_err) {
                g_critical("%s: Cannot dump module index: %s", __func__, tmp_err->message);
                free(moduleindex_str);
                g_clear_error(&tmp_err);
                g_clear_pointer(&merger, g_object_unref);
                exit(EXIT_FAILURE);
            }

            //compress new module metadata string to a file in temporary .repodata
            gchar *modules_metadata_path = g_strconcat(tmp_out_repo, "modules.yaml", compression_suffix, NULL);
            CR_FILE *modules_file = NULL;
            modules_file = cr_open(modules_metadata_path, CR_CW_MODE_WRITE, compression, &tmp_err);
            if (modules_file == NULL) {
                g_critical("%s: Cannot open source file %s: %s", __func__, modules_metadata_path,
                           (tmp_err ? tmp_err->message : "Unknown error"));
                g_clear_error(&tmp_err);
                free(moduleindex_str);
                free(modules_metadata_path);
                g_clear_pointer(&merger, g_object_unref);
                exit(EXIT_FAILURE);
            }
            cr_puts(modules_file, moduleindex_str, &tmp_err);
            free(moduleindex_str);
            cr_close(modules_file, &tmp_err);
            if (tmp_err) {
                g_critical("%s: Error while closing: : %s", __func__, tmp_err->message);
                g_clear_error(&tmp_err);
                free(modules_metadata_path);
                g_clear_pointer(&merger, g_object_unref);
                exit(EXIT_FAILURE);
            }

            //create additional metadatum for new module metadata file
            cr_Metadatum *new_modules_metadatum = g_malloc0(sizeof(cr_Metadatum));
            new_modules_metadatum->name = modules_metadata_path;
            new_modules_metadatum->type = g_strdup("modules");
            additional_metadata = g_slist_prepend(additional_metadata, new_modules_metadatum);
        }

        g_clear_pointer(&merger, g_object_unref);

    }
#endif /* WITH_LIBMODULEMD */

    if (cmd_options->update && cmd_options->keep_all_metadata &&
        old_metadata_location && old_metadata_location->additional_metadata)
    {
        GSList *element = old_metadata_location->additional_metadata;
        cr_Metadatum *m;
        for (; element; element=g_slist_next(element)) {
            m = g_malloc0(sizeof(cr_Metadatum));
            m->name = cr_copy_metadatum(((cr_Metadatum *) element->data)->name, tmp_out_repo, &tmp_err);
            m->type = g_strdup(((cr_Metadatum *) element->data)->type);
            additional_metadata = g_slist_prepend(additional_metadata, m);
        }
    }

    cr_metadatalocation_free(old_metadata_location);
    old_metadata_location = NULL;

    // Create and open new compressed files
    cr_XmlFile *pri_cr_file;
    cr_XmlFile *fil_cr_file;
    cr_XmlFile *oth_cr_file;

    cr_ContentStat *pri_stat;
    cr_ContentStat *fil_stat;
    cr_ContentStat *oth_stat;

    gchar *pri_xml_filename;
    gchar *fil_xml_filename;
    gchar *oth_xml_filename;

    g_message("Temporary output repo path: %s", tmp_out_repo);
    g_debug("Creating .xml.gz files");

    pri_xml_filename = g_strconcat(tmp_out_repo, "/primary.xml", xml_compression_suffix, NULL);
    fil_xml_filename = g_strconcat(tmp_out_repo, "/filelists.xml", xml_compression_suffix, NULL);
    oth_xml_filename = g_strconcat(tmp_out_repo, "/other.xml", xml_compression_suffix, NULL);

    pri_stat = cr_contentstat_new(cmd_options->repomd_checksum_type, NULL);
    pri_cr_file = cr_xmlfile_sopen_primary(pri_xml_filename,
                                           xml_compression,
                                           pri_stat,
                                           &tmp_err);
    assert(pri_cr_file || tmp_err);
    if (!pri_cr_file) {
        g_critical("Cannot open file %s: %s",
                   pri_xml_filename, tmp_err->message);
        g_clear_error(&tmp_err);
        cr_contentstat_free(pri_stat, NULL);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        exit(EXIT_FAILURE);
    }

    fil_stat = cr_contentstat_new(cmd_options->repomd_checksum_type, NULL);
    fil_cr_file = cr_xmlfile_sopen_filelists(fil_xml_filename,
                                            xml_compression,
                                            fil_stat,
                                            &tmp_err);
    assert(fil_cr_file || tmp_err);
    if (!fil_cr_file) {
        g_critical("Cannot open file %s: %s",
                   fil_xml_filename, tmp_err->message);
        g_clear_error(&tmp_err);
        cr_contentstat_free(pri_stat, NULL);
        cr_contentstat_free(fil_stat, NULL);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        cr_xmlfile_close(pri_cr_file, NULL);
        exit(EXIT_FAILURE);
    }

    oth_stat = cr_contentstat_new(cmd_options->repomd_checksum_type, NULL);
    oth_cr_file = cr_xmlfile_sopen_other(oth_xml_filename,
                                        xml_compression,
                                        oth_stat,
                                        &tmp_err);
    assert(oth_cr_file || tmp_err);
    if (!oth_cr_file) {
        g_critical("Cannot open file %s: %s",
                   oth_xml_filename, tmp_err->message);
        g_clear_error(&tmp_err);
        cr_contentstat_free(pri_stat, NULL);
        cr_contentstat_free(fil_stat, NULL);
        cr_contentstat_free(oth_stat, NULL);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        cr_xmlfile_close(fil_cr_file, NULL);
        cr_xmlfile_close(pri_cr_file, NULL);
        exit(EXIT_FAILURE);
    }

    // Set number of packages
    g_debug("Setting number of packages");
    cr_xmlfile_set_num_of_pkgs(pri_cr_file, task_count, NULL);
    cr_xmlfile_set_num_of_pkgs(fil_cr_file, task_count, NULL);
    cr_xmlfile_set_num_of_pkgs(oth_cr_file, task_count, NULL);

    // Open sqlite databases
    gchar *pri_db_filename = NULL;
    gchar *fil_db_filename = NULL;
    gchar *oth_db_filename = NULL;
    cr_SqliteDb *pri_db = NULL;
    cr_SqliteDb *fil_db = NULL;
    cr_SqliteDb *oth_db = NULL;

    if (!cmd_options->no_database) {
        _cleanup_file_close_ int pri_db_fd = -1;
        _cleanup_file_close_ int fil_db_fd = -1;
        _cleanup_file_close_ int oth_db_fd = -1;

        g_message("Preparing sqlite DBs");
        if (!cmd_options->local_sqlite) {
            g_debug("Creating databases");
            pri_db_filename = g_strconcat(tmp_out_repo, "/primary.sqlite", NULL);
            fil_db_filename = g_strconcat(tmp_out_repo, "/filelists.sqlite", NULL);
            oth_db_filename = g_strconcat(tmp_out_repo, "/other.sqlite", NULL);
        } else {
            g_debug("Creating databases localy");
            const gchar *tmpdir = g_get_tmp_dir();
            pri_db_filename = g_build_filename(tmpdir, "primary.XXXXXX.sqlite", NULL);
            fil_db_filename = g_build_filename(tmpdir, "filelists.XXXXXX.sqlite", NULL);
            oth_db_filename = g_build_filename(tmpdir, "other.XXXXXXX.sqlite", NULL);
            pri_db_fd = g_mkstemp(pri_db_filename);
            g_debug("%s", pri_db_filename);
            if (pri_db_fd == -1) {
                g_critical("Cannot open %s: %s", pri_db_filename, g_strerror(errno));
                exit(EXIT_FAILURE);
            }
            fil_db_fd = g_mkstemp(fil_db_filename);
            g_debug("%s", fil_db_filename);
            if (fil_db_fd == -1) {
                g_critical("Cannot open %s: %s", fil_db_filename, g_strerror(errno));
                exit(EXIT_FAILURE);
            }
            oth_db_fd = g_mkstemp(oth_db_filename);
            g_debug("%s", oth_db_filename);
            if (oth_db_fd == -1) {
                g_critical("Cannot open %s: %s", oth_db_filename, g_strerror(errno));
                exit(EXIT_FAILURE);
            }
        }

        pri_db = cr_db_open_primary(pri_db_filename, &tmp_err);
        assert(pri_db || tmp_err);
        if (!pri_db) {
            g_critical("Cannot open %s: %s",
                       pri_db_filename, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }

        fil_db = cr_db_open_filelists(fil_db_filename, &tmp_err);
        assert(fil_db || tmp_err);
        if (!fil_db) {
            g_critical("Cannot open %s: %s",
                       fil_db_filename, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }

        oth_db = cr_db_open_other(oth_db_filename, &tmp_err);
        assert(oth_db || tmp_err);
        if (!oth_db) {
            g_critical("Cannot open %s: %s",
                       oth_db_filename, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }
    }

    gchar *pri_zck_filename = NULL;
    gchar *fil_zck_filename = NULL;
    gchar *oth_zck_filename = NULL;
    cr_XmlFile *pri_cr_zck = NULL;
    cr_XmlFile *fil_cr_zck = NULL;
    cr_XmlFile *oth_cr_zck = NULL;
    cr_ContentStat *pri_zck_stat = NULL;
    cr_ContentStat *fil_zck_stat = NULL;
    cr_ContentStat *oth_zck_stat = NULL;
    gchar *pri_dict = NULL;
    gchar *fil_dict = NULL;
    gchar *oth_dict = NULL;
    size_t pri_dict_size = 0;
    size_t fil_dict_size = 0;
    size_t oth_dict_size = 0;
    gchar *pri_dict_file = NULL;
    gchar *fil_dict_file = NULL;
    gchar *oth_dict_file = NULL;

    if (cmd_options->zck_dict_dir) {
        pri_dict_file = cr_get_dict_file(cmd_options->zck_dict_dir,
                                         "primary.xml");
        fil_dict_file = cr_get_dict_file(cmd_options->zck_dict_dir,
                                         "filelists.xml");
        oth_dict_file = cr_get_dict_file(cmd_options->zck_dict_dir,
                                         "other.xml");
        if (pri_dict_file && !g_file_get_contents(pri_dict_file, &pri_dict,
                                                 &pri_dict_size, &tmp_err)) {
            g_critical("Error reading zchunk primary dict %s: %s",
                       pri_dict_file, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }
        if (fil_dict_file && !g_file_get_contents(fil_dict_file, &fil_dict,
                                                 &fil_dict_size, &tmp_err)) {
            g_critical("Error reading zchunk filelists dict %s: %s",
                       fil_dict_file, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }
        if (oth_dict_file && !g_file_get_contents(oth_dict_file, &oth_dict,
                                                 &oth_dict_size, &tmp_err)) {
            g_critical("Error reading zchunk other dict %s: %s",
                       oth_dict_file, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }
    }
    if (cmd_options->zck_compression) {
        g_debug("Creating .xml.zck files");

        pri_zck_filename = g_strconcat(tmp_out_repo, "/primary.xml.zck", NULL);
        fil_zck_filename = g_strconcat(tmp_out_repo, "/filelists.xml.zck", NULL);
        oth_zck_filename = g_strconcat(tmp_out_repo, "/other.xml.zck", NULL);

        pri_zck_stat = cr_contentstat_new(cmd_options->repomd_checksum_type, NULL);
        pri_cr_zck = cr_xmlfile_sopen_primary(pri_zck_filename,
                                              CR_CW_ZCK_COMPRESSION,
                                              pri_zck_stat,
                                              &tmp_err);
        assert(pri_cr_zck || tmp_err);
        if (!pri_cr_zck) {
            g_critical("Cannot open file %s: %s",
                       pri_zck_filename, tmp_err->message);
            g_clear_error(&tmp_err);
            cr_contentstat_free(pri_zck_stat, NULL);
            g_free(pri_zck_filename);
            g_free(fil_zck_filename);
            g_free(oth_zck_filename);
            exit(EXIT_FAILURE);
        }
        cr_set_dict(pri_cr_zck->f, pri_dict, pri_dict_size, &tmp_err);
        if (tmp_err) {
            g_critical("Error reading setting primary dict %s: %s",
                       pri_dict_file, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }
        g_free(pri_dict);

        fil_zck_stat = cr_contentstat_new(cmd_options->repomd_checksum_type, NULL);
        fil_cr_zck = cr_xmlfile_sopen_filelists(fil_zck_filename,
                                                CR_CW_ZCK_COMPRESSION,
                                                fil_zck_stat,
                                                &tmp_err);
        assert(fil_cr_zck || tmp_err);
        if (!fil_cr_zck) {
            g_critical("Cannot open file %s: %s",
                       fil_zck_filename, tmp_err->message);
            g_clear_error(&tmp_err);
            cr_contentstat_free(pri_zck_stat, NULL);
            cr_contentstat_free(fil_zck_stat, NULL);
            g_free(pri_zck_filename);
            g_free(fil_zck_filename);
            g_free(oth_zck_filename);
            cr_xmlfile_close(pri_cr_zck, NULL);
            exit(EXIT_FAILURE);
        }
        cr_set_dict(fil_cr_zck->f, fil_dict, fil_dict_size, &tmp_err);
        if (tmp_err) {
            g_critical("Error reading setting filelists dict %s: %s",
                       fil_dict_file, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }
        g_free(fil_dict);

        oth_zck_stat = cr_contentstat_new(cmd_options->repomd_checksum_type, NULL);
        oth_cr_zck = cr_xmlfile_sopen_other(oth_zck_filename,
                                            CR_CW_ZCK_COMPRESSION,
                                            oth_zck_stat,
                                            &tmp_err);
        assert(oth_cr_zck || tmp_err);
        if (!oth_cr_zck) {
            g_critical("Cannot open file %s: %s",
                       oth_zck_filename, tmp_err->message);
            g_clear_error(&tmp_err);
            cr_contentstat_free(pri_zck_stat, NULL);
            cr_contentstat_free(fil_zck_stat, NULL);
            cr_contentstat_free(oth_zck_stat, NULL);
            g_free(pri_zck_filename);
            g_free(fil_zck_filename);
            g_free(oth_zck_filename);
            cr_xmlfile_close(fil_cr_zck, NULL);
            cr_xmlfile_close(pri_cr_zck, NULL);
            exit(EXIT_FAILURE);
        }
        cr_set_dict(oth_cr_zck->f, oth_dict, oth_dict_size, &tmp_err);
        if (tmp_err) {
            g_critical("Error reading setting other dict %s: %s",
                       oth_dict_file, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }
        g_free(oth_dict);

        // Set number of packages
        g_debug("Setting number of packages");
        cr_xmlfile_set_num_of_pkgs(pri_cr_zck, task_count, NULL);
        cr_xmlfile_set_num_of_pkgs(fil_cr_zck, task_count, NULL);
        cr_xmlfile_set_num_of_pkgs(oth_cr_zck, task_count, NULL);
    }

    // Thread pool - User data initialization
    user_data.pri_f             = pri_cr_file;
    user_data.fil_f             = fil_cr_file;
    user_data.oth_f             = oth_cr_file;
    user_data.pri_db            = pri_db;
    user_data.fil_db            = fil_db;
    user_data.oth_db            = oth_db;
    user_data.pri_zck           = pri_cr_zck;
    user_data.fil_zck           = fil_cr_zck;
    user_data.oth_zck           = oth_cr_zck;
    if (cmd_options->compatibility && cmd_options->changelog_limit == DEFAULT_CHANGELOG_LIMIT ) {
      user_data.changelog_limit   = -1;
    } else {
      user_data.changelog_limit   = cmd_options->changelog_limit;
    }
    user_data.location_base     = cmd_options->location_base;
    user_data.checksum_type_str = cr_checksum_name_str(cmd_options->checksum_type);
    user_data.checksum_type     = cmd_options->checksum_type;
    user_data.checksum_cachedir = cmd_options->checksum_cachedir;
    user_data.skip_symlinks     = cmd_options->skip_symlinks;
    user_data.repodir_name_len  = strlen(in_dir);
    user_data.task_count        = task_count;
    user_data.package_count     = 0;
    user_data.skip_stat         = cmd_options->skip_stat;
    user_data.old_metadata      = old_metadata;
    user_data.id_pri            = 0;
    user_data.id_fil            = 0;
    user_data.id_oth            = 0;
    user_data.buffer            = g_queue_new();
    user_data.deltas            = cmd_options->deltas;
    user_data.max_delta_rpm_size= cmd_options->max_delta_rpm_size;
    user_data.deltatargetpackages = NULL;
    user_data.cut_dirs          = cmd_options->cut_dirs;
    user_data.location_prefix   = cmd_options->location_prefix;
    user_data.had_errors        = 0;
    user_data.output_pkg_list   = output_pkg_list;

    g_mutex_init(&(user_data.mutex_output_pkg_list));
    g_mutex_init(&(user_data.mutex_pri));
    g_mutex_init(&(user_data.mutex_fil));
    g_mutex_init(&(user_data.mutex_oth));
    g_cond_init(&(user_data.cond_pri));
    g_cond_init(&(user_data.cond_fil));
    g_cond_init(&(user_data.cond_oth));
    g_mutex_init(&(user_data.mutex_buffer));
    g_mutex_init(&(user_data.mutex_old_md));
    g_mutex_init(&(user_data.mutex_deltatargetpackages));

    g_debug("Thread pool user data ready");

    // Start pool
    g_thread_pool_set_max_threads(pool, cmd_options->workers, NULL);
    g_message("Pool started (with %d workers)", cmd_options->workers);

    // Wait until pool is finished
    g_thread_pool_free(pool, FALSE, TRUE);

    // if there were any errors, exit nonzero
    if ( cmd_options->error_exit_val && user_data.had_errors ) {
	exit_val = 2;
    }

    g_message("Pool finished%s", (user_data.had_errors ? " with errors" : ""));

    cr_xml_dump_cleanup();

    if (output_pkg_list)
        fclose(output_pkg_list);

    cr_xmlfile_close(pri_cr_file, &tmp_err);
    if (!tmp_err)
        cr_xmlfile_close(fil_cr_file, &tmp_err);
    if (!tmp_err)
        cr_xmlfile_close(oth_cr_file, &tmp_err);
    if (tmp_err) {
        g_critical("Error while closing xml files: %s", tmp_err->message);
        g_clear_error(&tmp_err);
        exit(EXIT_FAILURE);
    }

    cr_xmlfile_close(pri_cr_zck, &tmp_err);
    if (tmp_err) {
        g_critical("%s: %s",
                   pri_zck_filename, tmp_err->message);
        g_clear_error(&tmp_err);
        exit(EXIT_FAILURE);
    }
    cr_xmlfile_close(fil_cr_zck, &tmp_err);
    if (tmp_err) {
        g_critical("%s: %s",
                   fil_zck_filename, tmp_err->message);
        g_clear_error(&tmp_err);
        exit(EXIT_FAILURE);
    }
    cr_xmlfile_close(oth_cr_zck, &tmp_err);
    if (tmp_err) {
        g_critical("%s: %s",
                   oth_zck_filename, tmp_err->message);
        g_clear_error(&tmp_err);
        exit(EXIT_FAILURE);
    }

    /* At the time of writing xml metadata headers we haven't yet parsed all
     * the packages and we don't know whether there were some invalid ones,
     * therefore we write the task count into the headers instead of the actual package count.
     * If there actually were some invalid packages we have to correct this value
     * that unfortunately means we have to decompress metadata files change package
     * count value and compress them again.
     */
    if (user_data.package_count != user_data.task_count){
        g_message("Warning: There were some invalid packages: we have to recompress other, filelists and primary xml metadata files in order to have correct package counts");

        GThreadPool *rewrite_pkg_count_pool = g_thread_pool_new(cr_rewrite_pkg_count_thread,
                                                                &user_data, 3, FALSE, NULL);

        cr_CompressionTask *pri_rewrite_pkg_count_task     = NULL;
        cr_CompressionTask *fil_rewrite_pkg_count_task     = NULL;
        cr_CompressionTask *oth_rewrite_pkg_count_task     = NULL;
        cr_CompressionTask *pri_zck_rewrite_pkg_count_task = NULL;
        cr_CompressionTask *fil_zck_rewrite_pkg_count_task = NULL;
        cr_CompressionTask *oth_zck_rewrite_pkg_count_task = NULL;

        pri_rewrite_pkg_count_task = cr_compressiontask_new(pri_xml_filename,
                                                            NULL,
                                                            xml_compression,
                                                            cmd_options->repomd_checksum_type,
                                                            NULL, FALSE, 1,
                                                            &tmp_err);
        g_thread_pool_push(rewrite_pkg_count_pool, pri_rewrite_pkg_count_task, NULL);

        fil_rewrite_pkg_count_task = cr_compressiontask_new(fil_xml_filename,
                                                            NULL,
                                                            xml_compression,
                                                            cmd_options->repomd_checksum_type,
                                                            NULL, FALSE, 1,
                                                            &tmp_err);
        g_thread_pool_push(rewrite_pkg_count_pool, fil_rewrite_pkg_count_task, NULL);

        oth_rewrite_pkg_count_task = cr_compressiontask_new(oth_xml_filename,
                                                            NULL,
                                                            xml_compression,
                                                            cmd_options->repomd_checksum_type,
                                                            NULL, FALSE, 1,
                                                            &tmp_err);
        g_thread_pool_push(rewrite_pkg_count_pool, oth_rewrite_pkg_count_task, NULL);

        if (cmd_options->zck_compression) {
            pri_zck_rewrite_pkg_count_task = cr_compressiontask_new(pri_zck_filename,
                                                                NULL,
                                                                CR_CW_ZCK_COMPRESSION,
                                                                cmd_options->repomd_checksum_type,
                                                                pri_dict_file,
                                                                FALSE, 1, &tmp_err);
            g_thread_pool_push(rewrite_pkg_count_pool, pri_zck_rewrite_pkg_count_task, NULL);

            fil_zck_rewrite_pkg_count_task = cr_compressiontask_new(fil_zck_filename,
                                                                NULL,
                                                                CR_CW_ZCK_COMPRESSION,
                                                                cmd_options->repomd_checksum_type,
                                                                fil_dict_file,
                                                                FALSE, 1, &tmp_err);
            g_thread_pool_push(rewrite_pkg_count_pool, fil_zck_rewrite_pkg_count_task, NULL);

            oth_zck_rewrite_pkg_count_task = cr_compressiontask_new(oth_zck_filename,
                                                                NULL,
                                                                CR_CW_ZCK_COMPRESSION,
                                                                cmd_options->repomd_checksum_type,
                                                                oth_dict_file,
                                                                FALSE, 1, &tmp_err);
            g_thread_pool_push(rewrite_pkg_count_pool, oth_zck_rewrite_pkg_count_task, NULL);
        }

        g_thread_pool_free(rewrite_pkg_count_pool, FALSE, TRUE);

        error_check_and_set_content_stat(pri_rewrite_pkg_count_task, pri_xml_filename, &exit_val, &pri_stat);
        error_check_and_set_content_stat(fil_rewrite_pkg_count_task, fil_xml_filename, &exit_val, &fil_stat);
        error_check_and_set_content_stat(oth_rewrite_pkg_count_task, oth_xml_filename, &exit_val, &oth_stat);

        cr_compressiontask_free(pri_rewrite_pkg_count_task, NULL);
        cr_compressiontask_free(fil_rewrite_pkg_count_task, NULL);
        cr_compressiontask_free(oth_rewrite_pkg_count_task, NULL);

        if (cmd_options->zck_compression){
            error_check_and_set_content_stat(pri_zck_rewrite_pkg_count_task, pri_zck_filename, &exit_val, &pri_zck_stat);
            error_check_and_set_content_stat(fil_zck_rewrite_pkg_count_task, fil_zck_filename, &exit_val, &fil_zck_stat);
            error_check_and_set_content_stat(oth_zck_rewrite_pkg_count_task, oth_zck_filename, &exit_val, &oth_zck_stat);

            cr_compressiontask_free(pri_zck_rewrite_pkg_count_task, NULL);
            cr_compressiontask_free(fil_zck_rewrite_pkg_count_task, NULL);
            cr_compressiontask_free(oth_zck_rewrite_pkg_count_task, NULL);
        }

    }
    if (cmd_options->zck_compression){
        g_free(pri_dict_file);
        g_free(fil_dict_file);
        g_free(oth_dict_file);
    }

    g_queue_free(user_data.buffer);
    g_mutex_clear(&(user_data.mutex_output_pkg_list));
    g_mutex_clear(&(user_data.mutex_pri));
    g_mutex_clear(&(user_data.mutex_fil));
    g_mutex_clear(&(user_data.mutex_oth));
    g_cond_clear(&(user_data.cond_pri));
    g_cond_clear(&(user_data.cond_fil));
    g_cond_clear(&(user_data.cond_oth));
    g_mutex_clear(&(user_data.mutex_buffer));
    g_mutex_clear(&(user_data.mutex_old_md));
    g_mutex_clear(&(user_data.mutex_deltatargetpackages));

    // Create repomd records for each file
    g_debug("Generating repomd.xml");

    cr_Repomd *repomd_obj = cr_repomd_new();

    cr_RepomdRecord *pri_xml_rec = cr_repomd_record_new("primary", pri_xml_filename);
    cr_RepomdRecord *fil_xml_rec = cr_repomd_record_new("filelists", fil_xml_filename);
    cr_RepomdRecord *oth_xml_rec = cr_repomd_record_new("other", oth_xml_filename);
    cr_RepomdRecord *pri_db_rec               = NULL;
    cr_RepomdRecord *fil_db_rec               = NULL;
    cr_RepomdRecord *oth_db_rec               = NULL;
    cr_RepomdRecord *pri_zck_rec              = NULL;
    cr_RepomdRecord *fil_zck_rec              = NULL;
    cr_RepomdRecord *oth_zck_rec              = NULL;
    cr_RepomdRecord *prestodelta_rec          = NULL;
    cr_RepomdRecord *prestodelta_zck_rec      = NULL;

    // List of cr_RepomdRecords
    GSList *additional_metadata_rec           = NULL; 

    // XML
    cr_repomd_record_load_contentstat(pri_xml_rec, pri_stat);
    cr_repomd_record_load_contentstat(fil_xml_rec, fil_stat);
    cr_repomd_record_load_contentstat(oth_xml_rec, oth_stat);

    cr_contentstat_free(pri_stat, NULL);
    cr_contentstat_free(fil_stat, NULL);
    cr_contentstat_free(oth_stat, NULL);

    GThreadPool *fill_pool = g_thread_pool_new(cr_repomd_record_fill_thread,
                                               NULL, 3, FALSE, NULL);

    cr_RepomdRecordFillTask *pri_fill_task;
    cr_RepomdRecordFillTask *fil_fill_task;
    cr_RepomdRecordFillTask *oth_fill_task;

    pri_fill_task = cr_repomdrecordfilltask_new(pri_xml_rec,
                                                cmd_options->repomd_checksum_type,
                                                NULL);
    g_thread_pool_push(fill_pool, pri_fill_task, NULL);

    fil_fill_task = cr_repomdrecordfilltask_new(fil_xml_rec,
                                                cmd_options->repomd_checksum_type,
                                                NULL);
    g_thread_pool_push(fill_pool, fil_fill_task, NULL);

    oth_fill_task = cr_repomdrecordfilltask_new(oth_xml_rec,
                                                cmd_options->repomd_checksum_type,
                                                NULL);
    g_thread_pool_push(fill_pool, oth_fill_task, NULL);


    additional_metadata_rec = cr_create_repomd_records_for_additional_metadata(additional_metadata,
                                                                               cmd_options->repomd_checksum_type);

    if (new_groupfile_metadatum) {
        additional_metadata_rec = cr_create_repomd_records_for_groupfile_metadata(new_groupfile_metadatum,
                                                                                  additional_metadata_rec,
                                                                                  compression,
                                                                                  cmd_options->repomd_checksum_type);

        //NOTE(amatej): Now we can add groupfile metadata to the additional_metadata list, for unified handlig while zck compressing
        additional_metadata = g_slist_prepend(additional_metadata, new_groupfile_metadatum);
        cr_Metadatum *compressed_new_groupfile_metadatum = g_malloc0(sizeof(cr_Metadatum));
        compressed_new_groupfile_metadatum->name = g_strdup(((cr_RepomdRecord *) additional_metadata_rec->data)->location_real);
        compressed_new_groupfile_metadatum->type = g_strdup(((cr_RepomdRecord *) additional_metadata_rec->data)->type);
        additional_metadata = g_slist_prepend(additional_metadata, compressed_new_groupfile_metadatum);
    }

    // Wait till repomd record fill task of xml files ends.
    g_thread_pool_free(fill_pool, FALSE, TRUE);

    cr_repomdrecordfilltask_free(pri_fill_task, NULL);
    cr_repomdrecordfilltask_free(fil_fill_task, NULL);
    cr_repomdrecordfilltask_free(oth_fill_task, NULL);

    // Sqlite db
    if (!cmd_options->no_database) {

        gchar *pri_db_name = g_strconcat(tmp_out_repo, "/primary.sqlite",
                                         sqlite_compression_suffix, NULL);
        gchar *fil_db_name = g_strconcat(tmp_out_repo, "/filelists.sqlite",
                                         sqlite_compression_suffix, NULL);
        gchar *oth_db_name = g_strconcat(tmp_out_repo, "/other.sqlite",
                                         sqlite_compression_suffix, NULL);

        cr_db_dbinfo_update(pri_db, pri_xml_rec->checksum, &tmp_err);
        if (!tmp_err)
            cr_db_dbinfo_update(fil_db, fil_xml_rec->checksum, &tmp_err);
        if (!tmp_err)
            cr_db_dbinfo_update(oth_db, oth_xml_rec->checksum, &tmp_err);
        if (tmp_err) {
            g_critical("Error updating dbinfo: %s", tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }

        cr_db_close(pri_db, &tmp_err);
        if (!tmp_err)
            cr_db_close(fil_db, &tmp_err);
        if (!tmp_err)
            cr_db_close(oth_db, &tmp_err);
        if (tmp_err) {
            g_critical("Error while closing db: %s", tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }


        // Compress dbs
        GThreadPool *compress_pool =  g_thread_pool_new(cr_compressing_thread,
                                                        NULL, 3, FALSE, NULL);

        cr_CompressionTask *pri_db_task;
        cr_CompressionTask *fil_db_task;
        cr_CompressionTask *oth_db_task;

        pri_db_task = cr_compressiontask_new(pri_db_filename,
                                             pri_db_name,
                                             sqlite_compression,
                                             cmd_options->repomd_checksum_type,
                                             NULL, FALSE, 1, NULL);
        g_thread_pool_push(compress_pool, pri_db_task, NULL);

        fil_db_task = cr_compressiontask_new(fil_db_filename,
                                             fil_db_name,
                                             sqlite_compression,
                                             cmd_options->repomd_checksum_type,
                                             NULL, FALSE, 1, NULL);
        g_thread_pool_push(compress_pool, fil_db_task, NULL);

        oth_db_task = cr_compressiontask_new(oth_db_filename,
                                             oth_db_name,
                                             sqlite_compression,
                                             cmd_options->repomd_checksum_type,
                                             NULL, FALSE, 1, NULL);
        g_thread_pool_push(compress_pool, oth_db_task, NULL);

        g_thread_pool_free(compress_pool, FALSE, TRUE);

        if (!cmd_options->local_sqlite) {
            cr_rm(pri_db_filename, CR_RM_FORCE, NULL, NULL);
            cr_rm(fil_db_filename, CR_RM_FORCE, NULL, NULL);
            cr_rm(oth_db_filename, CR_RM_FORCE, NULL, NULL);
        }

        // Prepare repomd records
        pri_db_rec = cr_repomd_record_new("primary_db", pri_db_name);
        fil_db_rec = cr_repomd_record_new("filelists_db", fil_db_name);
        oth_db_rec = cr_repomd_record_new("other_db", oth_db_name);

        g_free(pri_db_name);
        g_free(fil_db_name);
        g_free(oth_db_name);

        cr_repomd_record_load_contentstat(pri_db_rec, pri_db_task->stat);
        cr_repomd_record_load_contentstat(fil_db_rec, fil_db_task->stat);
        cr_repomd_record_load_contentstat(oth_db_rec, oth_db_task->stat);

        cr_compressiontask_free(pri_db_task, NULL);
        cr_compressiontask_free(fil_db_task, NULL);
        cr_compressiontask_free(oth_db_task, NULL);

        fill_pool = g_thread_pool_new(cr_repomd_record_fill_thread,
                                      NULL, 3, FALSE, NULL);

        cr_RepomdRecordFillTask *pri_db_fill_task;
        cr_RepomdRecordFillTask *fil_db_fill_task;
        cr_RepomdRecordFillTask *oth_db_fill_task;

        pri_db_fill_task = cr_repomdrecordfilltask_new(pri_db_rec,
                                                       cmd_options->repomd_checksum_type,
                                                       NULL);
        g_thread_pool_push(fill_pool, pri_db_fill_task, NULL);

        fil_db_fill_task = cr_repomdrecordfilltask_new(fil_db_rec,
                                                       cmd_options->repomd_checksum_type,
                                                       NULL);
        g_thread_pool_push(fill_pool, fil_db_fill_task, NULL);

        oth_db_fill_task = cr_repomdrecordfilltask_new(oth_db_rec,
                                                       cmd_options->repomd_checksum_type,
                                                       NULL);
        g_thread_pool_push(fill_pool, oth_db_fill_task, NULL);

        g_thread_pool_free(fill_pool, FALSE, TRUE);

        cr_repomdrecordfilltask_free(pri_db_fill_task, NULL);
        cr_repomdrecordfilltask_free(fil_db_fill_task, NULL);
        cr_repomdrecordfilltask_free(oth_db_fill_task, NULL);
    }

    // Zchunk
    if (cmd_options->zck_compression) {
        // Prepare repomd records
        pri_zck_rec = cr_repomd_record_new("primary_zck", pri_zck_filename);
        fil_zck_rec = cr_repomd_record_new("filelists_zck", fil_zck_filename);
        oth_zck_rec = cr_repomd_record_new("other_zck", oth_zck_filename);

        cr_repomd_record_load_zck_contentstat(pri_zck_rec, pri_zck_stat);
        cr_repomd_record_load_zck_contentstat(fil_zck_rec, fil_zck_stat);
        cr_repomd_record_load_zck_contentstat(oth_zck_rec, oth_zck_stat);

        fill_pool = g_thread_pool_new(cr_repomd_record_fill_thread,
                                      NULL, 3, FALSE, NULL);

        cr_RepomdRecordFillTask *pri_zck_fill_task;
        cr_RepomdRecordFillTask *fil_zck_fill_task;
        cr_RepomdRecordFillTask *oth_zck_fill_task;

        pri_zck_fill_task = cr_repomdrecordfilltask_new(pri_zck_rec,
                                                       cmd_options->repomd_checksum_type,
                                                       NULL);
        g_thread_pool_push(fill_pool, pri_zck_fill_task, NULL);

        fil_zck_fill_task = cr_repomdrecordfilltask_new(fil_zck_rec,
                                                       cmd_options->repomd_checksum_type,
                                                       NULL);
        g_thread_pool_push(fill_pool, fil_zck_fill_task, NULL);

        oth_zck_fill_task = cr_repomdrecordfilltask_new(oth_zck_rec,
                                                       cmd_options->repomd_checksum_type,
                                                       NULL);
        g_thread_pool_push(fill_pool, oth_zck_fill_task, NULL);

        g_thread_pool_free(fill_pool, FALSE, TRUE);

        cr_repomdrecordfilltask_free(pri_zck_fill_task, NULL);
        cr_repomdrecordfilltask_free(fil_zck_fill_task, NULL);
        cr_repomdrecordfilltask_free(oth_zck_fill_task, NULL);

        //ZCK for additional metadata
        GSList *element = additional_metadata;
        for (; element; element=g_slist_next(element)) {
            cr_CompressionType com_type = cr_detect_compression(((cr_Metadatum *) element->data)->name, &tmp_err);
            gchar *elem_type = g_strdup(((cr_Metadatum *) element->data)->type);
            gchar *elem_name = g_strdup(((cr_Metadatum *) element->data)->name);
            if (com_type != CR_CW_NO_COMPRESSION){
                const gchar *compression_suffix = cr_compression_suffix(com_type);
                //remove suffixes if present
                if (g_str_has_suffix(elem_name, compression_suffix)){
                    gchar *tmp = elem_name;
                    elem_name = g_strndup(elem_name, (strlen(elem_name) - strlen(compression_suffix)));
                    g_free(tmp);
                }
                gchar *type_compression_suffix = g_strdup(compression_suffix);
                type_compression_suffix[0] = '_'; //replace '.'
                if (g_str_has_suffix(elem_type, type_compression_suffix)){
                    gchar *tmp = elem_type;
                    elem_type = g_strndup(elem_type, (strlen(elem_type) - strlen(type_compression_suffix)));
                    g_free(tmp);
                }
                g_free(type_compression_suffix);
            }
            gchar *additional_metadatum_rec_zck_type = g_strconcat(elem_type, "_zck", NULL);
            gchar *additional_metadatum_rec_zck_name = g_strconcat(elem_name, ".zck", NULL);
            g_free(elem_name);
            g_free(elem_type);
            if (tmp_err) {
                g_critical("Cannot detect compression type of %s: %s",
                       ((cr_Metadatum *) element->data)->name, tmp_err->message);
                g_clear_error(&tmp_err);
                exit(EXIT_FAILURE);
            }
            /* Only create additional_metadata_zck if additional_metadata isn't already zchunk 
             * and its zck version doesn't yet exists */
            if (com_type != CR_CW_ZCK_COMPRESSION && 
                !g_slist_find_custom(additional_metadata_rec, additional_metadatum_rec_zck_type, cr_cmp_repomd_record_type)) {
                GSList *additional_metadatum_rec_elem = g_slist_find_custom(additional_metadata_rec,
                                                                            ((cr_Metadatum *) element->data)->type,
                                                                            cr_cmp_repomd_record_type);

                additional_metadata_rec = g_slist_prepend(additional_metadata_rec,
                                                          cr_repomd_record_new(
                                                              additional_metadatum_rec_zck_type,
                                                              additional_metadatum_rec_zck_name
                                                          ));

                cr_repomd_record_compress_and_fill(additional_metadatum_rec_elem->data,
                                                   additional_metadata_rec->data,
                                                   cmd_options->repomd_checksum_type,
                                                   CR_CW_ZCK_COMPRESSION,
                                                   cmd_options->zck_dict_dir,
                                                   &tmp_err);
                if (tmp_err) {
                    g_critical("Cannot process %s %s: %s",
                            ((cr_Metadatum *) element->data)->type,
                            ((cr_Metadatum *) element->data)->name,
                            tmp_err->message);
                    g_clear_error(&tmp_err);
                    exit(EXIT_FAILURE);
                }
            }
            g_free(additional_metadatum_rec_zck_type);
            g_free(additional_metadatum_rec_zck_name);
        }
    }

    cr_contentstat_free(pri_zck_stat, NULL);
    cr_contentstat_free(fil_zck_stat, NULL);
    cr_contentstat_free(oth_zck_stat, NULL);

#ifdef CR_DELTA_RPM_SUPPORT
    // Delta generation
    if (cmd_options->deltas) {
        gchar *filename, *outdeltadir = NULL;
        gchar *prestodelta_xml_filename = NULL;
        gchar *prestodelta_zck_filename = NULL;

        GHashTable *ht_oldpackagedirs = NULL;
        cr_XmlFile *prestodelta_cr_file = NULL;
        cr_XmlFile *prestodelta_cr_zck_file = NULL;
        cr_ContentStat *prestodelta_stat = NULL;
        cr_ContentStat *prestodelta_zck_stat = NULL;

        filename = g_strconcat("prestodelta.xml",
                               compression_suffix,
                               NULL);
        outdeltadir = g_build_filename(out_dir, OUTDELTADIR, NULL);
        prestodelta_xml_filename = g_build_filename(tmp_out_repo,
                                                    filename,
                                                    NULL);
        g_free(filename);

        // 0) Prepare outdeltadir
        if (g_file_test(outdeltadir, G_FILE_TEST_EXISTS)) {
            if (!g_file_test(outdeltadir, G_FILE_TEST_IS_DIR)) {
                g_critical("The file %s already exists and it is not a directory",
                        outdeltadir);
                goto deltaerror;
            }
        } else if (g_mkdir(outdeltadir, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH)) {
            g_critical("Cannot create %s: %s", outdeltadir, g_strerror(errno));
            goto deltaerror;
        }

        // 1) Scan old package directories
        ht_oldpackagedirs = cr_deltarpms_scan_oldpackagedirs(cmd_options->oldpackagedirs_paths,
                                                   cmd_options->max_delta_rpm_size,
                                                   &tmp_err);
        if (!ht_oldpackagedirs) {
            g_critical("cr_deltarpms_scan_oldpackagedirs failed: %s\n", tmp_err->message);
            g_clear_error(&tmp_err);
            goto deltaerror;
        }

        // 2) Generate drpms in parallel
        ret = cr_deltarpms_parallel_deltas(user_data.deltatargetpackages,
                                 ht_oldpackagedirs,
                                 outdeltadir,
                                 cmd_options->num_deltas,
                                 cmd_options->workers,
                                 cmd_options->max_delta_rpm_size,
                                 cmd_options->max_delta_rpm_size,
                                 &tmp_err);
        if (!ret) {
            g_critical("Parallel generation of drpms failed: %s", tmp_err->message);
            g_clear_error(&tmp_err);
            goto deltaerror;
        }

        // 3) Generate prestodelta.xml file
        prestodelta_stat = cr_contentstat_new(cmd_options->repomd_checksum_type, NULL);
        prestodelta_cr_file = cr_xmlfile_sopen_prestodelta(prestodelta_xml_filename,
                                                           compression,
                                                           prestodelta_stat,
                                                           &tmp_err);
        if (!prestodelta_cr_file) {
            g_critical("Cannot open %s: %s", prestodelta_xml_filename, tmp_err->message);
            g_clear_error(&tmp_err);
            goto deltaerror;
        }
        if (cmd_options->zck_compression && compression != CR_CW_ZCK_COMPRESSION) {
            filename = g_strconcat("prestodelta.xml",
                                   cr_compression_suffix(CR_CW_ZCK_COMPRESSION),
                                   NULL);
            prestodelta_zck_filename = g_build_filename(tmp_out_repo,
                                                        filename,
                                                        NULL);
            g_free(filename);

            prestodelta_zck_stat = cr_contentstat_new(cmd_options->repomd_checksum_type, NULL);
            prestodelta_cr_zck_file = cr_xmlfile_sopen_prestodelta(prestodelta_zck_filename,
                                                                   CR_CW_ZCK_COMPRESSION,
                                                                   prestodelta_zck_stat,
                                                                   &tmp_err);
            if (!prestodelta_cr_zck_file) {
                g_critical("Cannot open %s: %s", prestodelta_zck_filename, tmp_err->message);
                g_clear_error(&tmp_err);
                goto deltaerror;
            }
        }

        ret = cr_deltarpms_generate_prestodelta_file(
                        outdeltadir,
                        prestodelta_cr_file,
                        prestodelta_cr_zck_file,
                        //cmd_options->checksum_type,
                        CR_CHECKSUM_SHA256, // Createrepo always uses SHA256
                        cmd_options->workers,
                        out_dir,
                        &tmp_err);
        if (!ret) {
            g_critical("Cannot generate %s: %s", prestodelta_xml_filename,
                    tmp_err->message);
            g_clear_error(&tmp_err);
            goto deltaerror;
        }

        cr_xmlfile_close(prestodelta_cr_file, NULL);
        prestodelta_cr_file = NULL;
        cr_xmlfile_close(prestodelta_cr_zck_file, NULL);
        prestodelta_cr_zck_file = NULL;

        // 4) Prepare repomd record
        prestodelta_rec = cr_repomd_record_new("prestodelta", prestodelta_xml_filename);
        cr_repomd_record_load_contentstat(prestodelta_rec, prestodelta_stat);
        cr_repomd_record_fill(prestodelta_rec, cmd_options->repomd_checksum_type, NULL);
        if (prestodelta_zck_stat) {
            prestodelta_zck_rec = cr_repomd_record_new("prestodelta_zck", prestodelta_zck_filename);
            cr_repomd_record_load_contentstat(prestodelta_zck_rec, prestodelta_zck_stat);
            cr_repomd_record_fill(prestodelta_zck_rec, cmd_options->repomd_checksum_type, NULL);
        }

deltaerror:
        // 5) Cleanup
        g_hash_table_destroy(ht_oldpackagedirs);
        g_free(outdeltadir);
        g_free(prestodelta_xml_filename);
        g_free(prestodelta_zck_filename);
        cr_xmlfile_close(prestodelta_cr_file, NULL);
        cr_xmlfile_close(prestodelta_cr_zck_file, NULL);
        cr_contentstat_free(prestodelta_stat, NULL);
        cr_contentstat_free(prestodelta_zck_stat, NULL);
        cr_slist_free_full(user_data.deltatargetpackages,
                       (GDestroyNotify) cr_deltatargetpackage_free);
    }
#endif

    // Add checksums into files names
    if (cmd_options->unique_md_filenames) {
        cr_repomd_record_rename_file(pri_xml_rec, NULL);
        cr_repomd_record_rename_file(fil_xml_rec, NULL);
        cr_repomd_record_rename_file(oth_xml_rec, NULL);
        cr_repomd_record_rename_file(pri_db_rec, NULL);
        cr_repomd_record_rename_file(fil_db_rec, NULL);
        cr_repomd_record_rename_file(oth_db_rec, NULL);
        cr_repomd_record_rename_file(pri_zck_rec, NULL);
        cr_repomd_record_rename_file(fil_zck_rec, NULL);
        cr_repomd_record_rename_file(oth_zck_rec, NULL);
        cr_repomd_record_rename_file(prestodelta_rec, NULL);
        cr_repomd_record_rename_file(prestodelta_zck_rec, NULL);
        GSList *element = additional_metadata_rec;
        for (; element; element=g_slist_next(element)) {
            cr_repomd_record_rename_file(element->data, NULL);
        }
    }

    if (cmd_options->set_timestamp_to_revision) {
        // validated already in cmd_parser.c:check_arguments
        gint64 revision = strtoll(cmd_options->revision, NULL, 0);
        cr_repomd_record_set_timestamp(pri_xml_rec, revision);
        cr_repomd_record_set_timestamp(fil_xml_rec, revision);
        cr_repomd_record_set_timestamp(oth_xml_rec, revision);
        cr_repomd_record_set_timestamp(pri_db_rec, revision);
        cr_repomd_record_set_timestamp(fil_db_rec, revision);
        cr_repomd_record_set_timestamp(oth_db_rec, revision);
        cr_repomd_record_set_timestamp(prestodelta_rec, revision);
        GSList *element = additional_metadata_rec;
        for (; element; element=g_slist_next(element)) {
            cr_repomd_record_set_timestamp(element->data, revision);
        }
    }

    // Gen xml
    cr_repomd_set_record(repomd_obj, pri_xml_rec);
    cr_repomd_set_record(repomd_obj, fil_xml_rec);
    cr_repomd_set_record(repomd_obj, oth_xml_rec);
    cr_repomd_set_record(repomd_obj, pri_db_rec);
    cr_repomd_set_record(repomd_obj, fil_db_rec);
    cr_repomd_set_record(repomd_obj, oth_db_rec);
    cr_repomd_set_record(repomd_obj, pri_zck_rec);
    cr_repomd_set_record(repomd_obj, fil_zck_rec);
    cr_repomd_set_record(repomd_obj, oth_zck_rec);
    cr_repomd_set_record(repomd_obj, prestodelta_rec);
    cr_repomd_set_record(repomd_obj, prestodelta_zck_rec);
    GSList *elem = additional_metadata_rec;
    for (; elem; elem=g_slist_next(elem)) {
        cr_repomd_set_record(repomd_obj, elem->data);
    }

    int i = 0;
    while (cmd_options->repo_tags && cmd_options->repo_tags[i])
        cr_repomd_add_repo_tag(repomd_obj, cmd_options->repo_tags[i++]);

    i = 0;
    while (cmd_options->content_tags && cmd_options->content_tags[i])
        cr_repomd_add_content_tag(repomd_obj, cmd_options->content_tags[i++]);

    if (cmd_options->distro_cpeids && cmd_options->distro_values) {
        GSList *cpeid = cmd_options->distro_cpeids;
        GSList *val   = cmd_options->distro_values;
        while (cpeid && val) {
            cr_repomd_add_distro_tag(repomd_obj, cpeid->data, val->data);
            cpeid = g_slist_next(cpeid);
            val   = g_slist_next(val);
        }
    }

    if (cmd_options->revision)
        cr_repomd_set_revision(repomd_obj, cmd_options->revision);

    cr_repomd_sort_records(repomd_obj);

    char *repomd_xml = cr_xml_dump_repomd(repomd_obj, &tmp_err);
    assert(repomd_xml || tmp_err);
    cr_repomd_free(repomd_obj);

    if (!repomd_xml) {
        g_critical("Cannot generate repomd.xml: %s", tmp_err->message);
        g_clear_error(&tmp_err);
        exit(EXIT_FAILURE);
    }

    // Write repomd.xml
    gchar *repomd_path = g_strconcat(tmp_out_repo, "repomd.xml", NULL);
    FILE *frepomd = fopen(repomd_path, "w");
    if (!frepomd) {
        g_critical("Cannot open %s: %s", repomd_path, g_strerror(errno));
        exit(EXIT_FAILURE);
    }
    fputs(repomd_xml, frepomd);
    fclose(frepomd);
    g_free(repomd_xml);
    g_free(repomd_path);


    // Final move
    // Copy selected metadata from the old repository
    cr_RetentionType retentiontype = CR_RETENTION_DEFAULT;
    gint64 retentionval = (gint64) cmd_options->retain_old;

    if (cmd_options->retain_old_md_by_age) {
        retentiontype = CR_RETENTION_BYAGE;
        retentionval = cmd_options->md_max_age;
    } else if (cmd_options->compatibility) {
        retentiontype = CR_RETENTION_COMPATIBILITY;
    }

    ret = cr_old_metadata_retention(out_repo,
                                    tmp_out_repo,
                                    retentiontype,
                                    retentionval,
                                    &tmp_err);
    if (!ret) {
        g_critical("%s", tmp_err->message);
        g_clear_error(&tmp_err);
        exit(EXIT_FAILURE);
    }

    gboolean old_repodata_renamed = FALSE;

    // === This section should be maximally atomic ===

    sigset_t new_mask, old_mask;
    sigemptyset(&old_mask);
    sigfillset(&new_mask);
    sigdelset(&new_mask, SIGKILL);  // These two signals cannot be
    sigdelset(&new_mask, SIGSTOP);  // blocked

    sigprocmask(SIG_BLOCK, &new_mask, &old_mask);

    // Rename out_repo to "repodata.old.pid.date.microsecs"
    gchar *tmp_dirname = cr_append_pid_and_datetime("repodata.old.", NULL);
    gchar *old_repodata_path = g_build_filename(out_dir, tmp_dirname, NULL);
    g_free(tmp_dirname);

    if (g_rename(out_repo, old_repodata_path) == -1) {
        g_debug("Old repodata doesn't exists: Cannot rename %s -> %s: %s",
                out_repo, old_repodata_path, g_strerror(errno));
    } else {
        g_debug("Renamed %s -> %s", out_repo, old_repodata_path);
        old_repodata_renamed = TRUE;
    }

    // Rename tmp_out_repo to out_repo
    if (g_rename(tmp_out_repo, out_repo) == -1) {
        g_critical("Cannot rename %s -> %s: %s", tmp_out_repo, out_repo,
                   g_strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        g_debug("Renamed %s -> %s", tmp_out_repo, out_repo);
    }

    // Remove lock
    if (g_strcmp0(lock_dir, tmp_out_repo))
        // If lock_dir is not same as temporary repo dir then remove it
        cr_remove_dir(lock_dir, NULL);

    // Disable path stored for exit handler
    cr_unset_cleanup_handler(NULL);

    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    // === End of section that has to be maximally atomic ===

    if (old_repodata_renamed) {
        // Remove "metadata.old" dir
        if (cr_rm(old_repodata_path, CR_RM_RECURSIVE, NULL, &tmp_err)) {
            g_debug("Old repo %s removed", old_repodata_path);
        } else {
            g_warning("Cannot remove %s: %s", old_repodata_path, tmp_err->message);
            g_clear_error(&tmp_err);
        }
    }


    // Clean up
    g_debug("Memory cleanup");

    if (old_metadata)
        cr_metadata_free(old_metadata);

    g_free(user_data.prev_srpm);
    g_free(user_data.cur_srpm);
    g_free(old_repodata_path);
    g_free(in_repo);
    g_free(out_repo);
    g_free(tmp_out_repo);
    g_free(in_dir);
    g_free(out_dir);
    g_free(lock_dir);
    g_free(pri_xml_filename);
    g_free(fil_xml_filename);
    g_free(oth_xml_filename);
    g_free(pri_db_filename);
    g_free(fil_db_filename);
    g_free(oth_db_filename);
    g_free(pri_zck_filename);
    g_free(fil_zck_filename);
    g_free(oth_zck_filename);
    g_slist_free_full(additional_metadata, (GDestroyNotify) cr_metadatum_free);
    g_slist_free(additional_metadata_rec);

    free_options(cmd_options);
    cr_package_parser_cleanup();

    g_debug("All done");
    exit(exit_val);
}
