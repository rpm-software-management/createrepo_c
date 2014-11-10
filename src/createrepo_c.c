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
#include "deltarpms.h"
#include "dumper_thread.h"
#include "checksum.h"
#include "error.h"
#include "helpers.h"
#include "load_metadata.h"
#include "locate_metadata.h"
#include "misc.h"
#include "parsepkg.h"
#include "repomd.h"
#include "sqlite.h"
#include "threads.h"
#include "version.h"
#include "xml_dump.h"
#include "xml_file.h"

#define OUTDELTADIR "drpms/"

// Global variables used by the signal handler failure_exit_cleanup
char *global_lock_dir     = NULL; // Path to .repodata/ dir that is used as a lock
char *global_tmp_out_repo = NULL; // Path to tmpreodata directory, if NULL
                                  // than it is same as the global_lock_dir

void
failure_exit_cleanup(int exit_status, void *data)
{
    CR_UNUSED(data);
    if (exit_status != EXIT_SUCCESS) {
        if (global_lock_dir) {
            g_debug("Removing %s", global_lock_dir);
            cr_remove_dir(global_lock_dir, NULL);
        }

        if (global_tmp_out_repo) {
            g_debug("Removing %s", global_tmp_out_repo);
            cr_remove_dir(global_tmp_out_repo, NULL);
        }
    }
}


// Signal handler
void
sigint_catcher(int sig)
{
    CR_UNUSED(sig);
    g_message("SIGINT catched: Terminating...");
    exit(1);
}


int
allowed_file(const gchar *filename, struct CmdOptions *options)
{
    // Check file against exclude glob masks
    if (options->exclude_masks) {
        int str_len = strlen(filename);
        gchar *reversed_filename = g_utf8_strreverse(filename, str_len);

        GSList *element = options->exclude_masks;
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



// Function used to sort pool tasks - this function is responsible for
// order of packages in metadata
int
task_cmp(gconstpointer a_p, gconstpointer b_p, gpointer user_data)
{
    int ret;
    const struct PoolTask *a = a_p;
    const struct PoolTask *b = b_p;
    CR_UNUSED(user_data);
    ret = g_strcmp0(a->filename, b->filename);
    if (ret) return ret;
    return g_strcmp0(a->path, b->path);
}


long
fill_pool(GThreadPool *pool,
          gchar *in_dir,
          struct CmdOptions *cmd_options,
          GSList **current_pkglist,
          FILE *output_pkg_list)
{
    GQueue queue = G_QUEUE_INIT;
    struct PoolTask *task;
    long package_count = 0;

    if (cmd_options->pkglist && !cmd_options->include_pkgs) {
        g_warning("Used pkglist doesn't contain any useful items");
    } else if (!(cmd_options->include_pkgs)) {
        // --pkglist (or --includepkg) is not supplied -> do dir walk

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

                gchar *full_path = g_strconcat(dirname, "/", filename, NULL);

                // Non .rpm files
                if (!g_str_has_suffix (filename, ".rpm")) {
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

                // Check filename against exclude glob masks
                const gchar *repo_relative_path = filename;
                if (in_dir_len < strlen(full_path))
                    // This probably should be always true
                    repo_relative_path = full_path + in_dir_len;

                if (allowed_file(repo_relative_path, cmd_options)) {
                    // FINALLY! Add file into pool
                    g_debug("Adding pkg: %s", full_path);
                    task = g_malloc(sizeof(struct PoolTask));
                    task->full_path = full_path;
                    task->filename = g_strdup(filename);
                    task->path = g_strdup(dirname);
                    if (output_pkg_list)
                        fprintf(output_pkg_list, "%s\n", repo_relative_path);
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
            gchar *filename; // foobar.rpm

            // Get index of last '/'
            int x = strlen(relative_path);
            for (; x > 0 && relative_path[x] != '/'; x--)
                ;

            if (!x) // There was no '/' in path
                filename = relative_path;
            else    // Use only a last part of the path
                filename = relative_path + x + 1;

            if (allowed_file(filename, cmd_options)) {
                // Check filename against exclude glob masks
                gchar *full_path = g_strconcat(in_dir, relative_path, NULL);
                //     ^^^ /path/to/in_repo/packages/i386/foobar.rpm
                g_debug("Adding pkg: %s", full_path);
                task = g_malloc(sizeof(struct PoolTask));
                task->full_path = full_path;
                task->filename  = g_strdup(filename);         // foobar.rpm
                task->path      = strndup(relative_path, x);  // packages/i386/
                if (output_pkg_list)
                    fprintf(output_pkg_list, "%s\n", relative_path);
                *current_pkglist = g_slist_prepend(*current_pkglist, task->filename);
                g_queue_insert_sorted(&queue, task, task_cmp, NULL);
            }
        }
    }

    // Push sorted tasks into the thread pool
    while ((task = g_queue_pop_head(&queue)) != NULL) {
        task->id = package_count;
        g_thread_pool_push(pool, task, NULL);
        ++package_count;
    }

    return package_count;
}


gboolean
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
                g_set_error(err, CR_CMD_ERROR, CRE_BADARG,
                            "The %s already exists and it is not a directory!",
                            cmd_options->checksum_cachedir);
                return FALSE;
            }
        } else {
            g_set_error(err, CR_CMD_ERROR, CRE_BADARG,
                        "cannot use cachedir %s: %s",
                        cmd_options->checksum_cachedir, strerror(errno));
            return FALSE;
        }
    }

    g_debug("Cachedir for checksums is %s", cmd_options->checksum_cachedir);
    return TRUE;
}


int
main(int argc, char **argv)
{
    struct CmdOptions *cmd_options;
    GError *tmp_err = NULL;


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
        printf("Version: %d.%d.%d\n", CR_VERSION_MAJOR,
                                      CR_VERSION_MINOR,
                                      CR_VERSION_PATCH);
        free_options(cmd_options);
        exit(EXIT_SUCCESS);
    }

    if (argc != 2) {
        // No mandatory arguments
        g_printerr("Must specify exactly one directory to index.\n");
        g_printerr("Usage: %s [options] <directory_to_index>\n\n",
                         cr_get_filename(argv[0]));
        free_options(cmd_options);
        exit(EXIT_FAILURE);
    }


    // Dirs

    gchar *in_dir       = NULL;  // path/to/repo/
    gchar *in_repo      = NULL;  // path/to/repo/repodata/
    gchar *out_dir      = NULL;  // path/to/out_repo/
    gchar *out_repo     = NULL;  // path/to/out_repo/repodata/
    gchar *tmp_out_repo = NULL;  // usually path/to/out_repo/.repodata/
    gchar *lock_dir     = NULL;  // path/to/out_repo/.repodata/
    int lock_dir_is_tmp_out_repo = 1;

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

    g_log_set_default_handler (cr_log_fn, NULL);

    if (cmd_options->quiet) {
        // Quiet mode
        GLogLevelFlags levels = G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO |
                                G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_WARNING;
        g_log_set_handler(NULL, levels, cr_null_log_fn, NULL);
        g_log_set_handler("C_CREATEREPOLIB", levels, cr_null_log_fn, NULL);
    } else if (cmd_options->verbose) {
        // Verbose mode
        GLogLevelFlags levels = G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO |
                                G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_WARNING;
        g_log_set_handler(NULL, levels, cr_log_fn, NULL);
        g_log_set_handler("C_CREATEREPOLIB", levels, cr_log_fn, NULL);
    } else {
        // Standard mode
        GLogLevelFlags levels = G_LOG_LEVEL_DEBUG;
        g_log_set_handler(NULL, levels, cr_null_log_fn, NULL);
        g_log_set_handler("C_CREATEREPOLIB", levels, cr_null_log_fn, NULL);
    }


    // Set paths of input and output repos

    in_repo = g_strconcat(in_dir, "repodata/", NULL);

    if (cmd_options->outputdir) {
        out_dir = cr_normalize_dir_path(cmd_options->outputdir);
        out_repo = g_strconcat(out_dir, "repodata/", NULL);
    } else {
        out_dir  = g_strdup(in_dir);
        out_repo = g_strdup(in_repo);
    }
    lock_dir = g_strconcat(out_dir, ".repodata/", NULL);
    tmp_out_repo = g_strconcat(out_dir, ".repodata/", NULL);


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

    sigset_t intmask;
    sigemptyset(&intmask);
    sigaddset(&intmask, SIGHUP);
    sigaddset(&intmask, SIGINT);
    sigaddset(&intmask, SIGPIPE);
    sigaddset(&intmask, SIGALRM);
    sigaddset(&intmask, SIGTERM);
    sigaddset(&intmask, SIGUSR1);
    sigaddset(&intmask, SIGUSR2);
    sigaddset(&intmask, SIGPOLL);
    sigaddset(&intmask, SIGPROF);
    sigaddset(&intmask, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &intmask, NULL);

    // Check if lock exists & Create lock dir

    if (g_mkdir(lock_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
        if (errno != EEXIST) {
            g_critical("Error while creating temporary repodata directory %s: %s",
                       lock_dir, strerror(errno));
            exit(EXIT_FAILURE);
        }

        g_critical("Temporary repodata directory: %s already exists! "
                   "(Another createrepo process is running?)", lock_dir);

        if (cmd_options->ignore_lock == FALSE)
            exit(EXIT_FAILURE);

        // The next section takes place only if the --ignore-lock is used
        // Ugly, but user wants it -> it's his fault

        // Remove existing .repodata/
        g_debug("(--ignore-lock enabled) Let's remove the old .repodata/");
        if (cr_rm(lock_dir, CR_RM_RECURSIVE, NULL, &tmp_err)) {
            g_debug("(--ignore-lock enabled) Removed: %s", lock_dir);
        } else {
            g_critical("(--ignore-lock enabled) Cannot remove %s: %s",
                       lock_dir, tmp_err->message);
            exit(EXIT_FAILURE);
        }

        // Try to create own - just as a lock
        if (g_mkdir(lock_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
            g_critical("(--ignore-lock enabled) Cannot create %s: %s",
                       lock_dir, strerror(errno));
            exit(EXIT_FAILURE);
        } else {
            g_debug("(--ignore-lock enabled) Own and empty %s created "
                    "(serves as a lock)", lock_dir);
        }

        global_lock_dir = lock_dir;

        // To data generation use a different one
        tmp_out_repo[strlen(tmp_out_repo)-1] = '.'; // Replace the last '/' with '.'
        gchar * new_tmp_out_repo = cr_append_pid_and_datetime(tmp_out_repo, "/");
        g_free(tmp_out_repo);
        tmp_out_repo = new_tmp_out_repo;
        if (g_mkdir(tmp_out_repo, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
            g_critical("(--ignore-lock enabled) Cannot create %s: %s",
                       tmp_out_repo, strerror(errno));
            exit(EXIT_FAILURE);
        } else {
            g_debug("(--ignore-lock enabled) For data generation is used: %s",
                    tmp_out_repo);
        }

        global_tmp_out_repo = tmp_out_repo;
        lock_dir_is_tmp_out_repo = 0;
    }

    global_lock_dir = lock_dir;

    // Register cleanup function
    if (on_exit(failure_exit_cleanup, NULL))
        g_warning("Cannot set exit cleanup function by atexit()");

    // Set handler that call exit(EXIT_FAILURE) for signals that leads to
    // process termination (list obtained from the "man 7 signal")
    // Signals that are ignored (SIGCHILD) or lead just to stop (SIGSTOP, ..)
    // SHOULDN'T GET THIS HANDLER - these signals do not terminate the process!

    g_debug("Signal handler setup");
    struct sigaction sigact;
    sigact.sa_handler = sigint_catcher;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;

    // Signals that terminate (from the POSIX.1-1990)
    sigaction(SIGHUP, &sigact, NULL);
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGPIPE, &sigact, NULL);
    sigaction(SIGALRM, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGUSR1, &sigact, NULL);
    sigaction(SIGUSR2, &sigact, NULL);
    // Signals that terminate (from the POSIX.1-2001)
    sigaction(SIGPOLL, &sigact, NULL);
    sigaction(SIGPROF, &sigact, NULL);
    sigaction(SIGVTALRM, &sigact, NULL);


    // Unblock the blocked signals

    sigemptyset(&intmask);
    sigaddset(&intmask, SIGHUP);
    sigaddset(&intmask, SIGINT);
    sigaddset(&intmask, SIGPIPE);
    sigaddset(&intmask, SIGALRM);
    sigaddset(&intmask, SIGTERM);
    sigaddset(&intmask, SIGUSR1);
    sigaddset(&intmask, SIGUSR2);
    sigaddset(&intmask, SIGPOLL);
    sigaddset(&intmask, SIGPROF);
    sigaddset(&intmask, SIGVTALRM);
    sigprocmask(SIG_UNBLOCK, &intmask, NULL);


    // Open package list

    FILE *output_pkg_list = NULL;
    if (cmd_options->read_pkgs_list) {
        output_pkg_list = fopen(cmd_options->read_pkgs_list, "w");
        if (!output_pkg_list) {
            g_critical("Cannot open \"%s\" for writing: %s",
                       cmd_options->read_pkgs_list, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }


    // Init package parser

    cr_package_parser_init();
    cr_xml_dump_init();


    // Thread pool - Creation

    struct UserData user_data;
    g_thread_init(NULL);
    GThreadPool *pool = g_thread_pool_new(cr_dumper_thread,
                                          &user_data,
                                          0,
                                          TRUE,
                                          NULL);
    g_debug("Thread pool ready");

    long package_count;
    GSList *current_pkglist = NULL;
    /* ^^^ List with basenames of files which will be processed */


    // Thread pool - Fill with tasks

    package_count = fill_pool(pool,
                              in_dir,
                              cmd_options,
                              &current_pkglist,
                              output_pkg_list);

    g_debug("Package count: %ld", package_count);
    g_message("Directory walk done - %ld packages", package_count);

    if (output_pkg_list)
        fclose(output_pkg_list);


    // Load old metadata if --update

    cr_Metadata *old_metadata = NULL;
    struct cr_MetadataLocation *old_metadata_location = NULL;

    if (!package_count)
        g_debug("No packages found - skipping metadata loading");

    if (package_count && cmd_options->update) {
        int ret;
        old_metadata = cr_metadata_new(CR_HT_KEY_FILENAME, 1, current_pkglist);
        cr_metadata_set_dupaction(old_metadata, CR_HT_DUPACT_REMOVEALL);

        if (cmd_options->outputdir)
            old_metadata_location = cr_locate_metadata(out_dir, 1, NULL);
        else
            old_metadata_location = cr_locate_metadata(in_dir, 1, NULL);

        if (old_metadata_location) {
            ret = cr_metadata_load_xml(old_metadata,
                                       old_metadata_location,
                                       &tmp_err);
            assert(ret == CRE_OK || tmp_err);

            if (ret == CRE_OK) {
                g_debug("Old metadata from: %s - loaded",
                        old_metadata_location->original_url);
            } else {
                g_debug("Old metadata from %s - loading failed: %s",
                        old_metadata_location->original_url, tmp_err->message);
                g_clear_error(&tmp_err);
            }
        }

        // Load repodata from --update-md-path
        GSList *element = cmd_options->l_update_md_paths;
        for (; element; element = g_slist_next(element)) {
            char *path = (char *) element->data;
            g_message("Loading metadata from md-path: %s", path);

            ret = cr_metadata_locate_and_load_xml(old_metadata, path, &tmp_err);
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
                  g_hash_table_size(cr_metadata_hashtable(old_metadata)));
    }

    g_slist_free(current_pkglist);
    current_pkglist = NULL;


    // Copy groupfile

    gchar *groupfile = NULL;

    if (cmd_options->groupfile_fullpath) {
        // Groupfile specified as argument

        groupfile = g_strconcat(tmp_out_repo,
                              cr_get_filename(cmd_options->groupfile_fullpath),
                              NULL);
        g_debug("Copy groupfile %s -> %s",
                cmd_options->groupfile_fullpath, groupfile);

        int ret;
        ret = cr_better_copy_file(cmd_options->groupfile_fullpath,
                                  groupfile,
                                  &tmp_err);
        assert(ret == CRE_OK || tmp_err);

        if (ret != CRE_OK) {
            g_critical("Error while copy %s -> %s: %s",
                       cmd_options->groupfile_fullpath,
                       groupfile,
                       tmp_err->message);
            g_clear_error(&tmp_err);
        }
    } else if (cmd_options->update && cmd_options->keep_all_metadata &&
               old_metadata_location && old_metadata_location->groupfile_href)
    {
        // Old groupfile exists

        g_message("Using groupfile from target repo");

        gchar *src_groupfile = old_metadata_location->groupfile_href;
        groupfile = g_strconcat(tmp_out_repo,
                                cr_get_filename(src_groupfile),
                                NULL);

        g_debug("Copy groupfile %s -> %s", src_groupfile, groupfile);

        int ret = cr_better_copy_file(src_groupfile, groupfile, &tmp_err);
        assert(ret == CRE_OK || tmp_err);

        if (ret != CRE_OK) {
            g_critical("Error while copy %s -> %s: %s",
                       src_groupfile, groupfile, tmp_err->message);
            g_clear_error(&tmp_err);
        }
    }


    // Copy update info

    char *updateinfo = NULL;

    if (cmd_options->update && cmd_options->keep_all_metadata &&
        old_metadata_location && old_metadata_location->updateinfo_href)
    {
        int ret;
        gchar *src_updateinfo = old_metadata_location->updateinfo_href;
        updateinfo = g_strconcat(tmp_out_repo,
                                 cr_get_filename(src_updateinfo),
                                 NULL);

        g_debug("Copy updateinfo %s -> %s", src_updateinfo, updateinfo);

        ret = cr_better_copy_file(src_updateinfo, updateinfo, &tmp_err);
        assert(ret == CRE_OK || tmp_err);

        if (ret != CRE_OK) {
            g_critical("Error while copy %s -> %s: %s",
                       src_updateinfo, updateinfo, tmp_err->message);
            g_clear_error(&tmp_err);
        }
    }

    cr_metadatalocation_free(old_metadata_location);
    old_metadata_location = NULL;


    // Setup compression types

    const char *sqlite_compression_suffix = NULL;
    const char *prestodelta_compression_suffix = NULL;
    cr_CompressionType sqlite_compression = CR_CW_BZ2_COMPRESSION;
    cr_CompressionType groupfile_compression = CR_CW_GZ_COMPRESSION;
    cr_CompressionType prestodelta_compression = CR_CW_GZ_COMPRESSION;

    if (cmd_options->compression_type != CR_CW_UNKNOWN_COMPRESSION) {
        sqlite_compression      = cmd_options->compression_type;
        groupfile_compression   = cmd_options->compression_type;
        prestodelta_compression = cmd_options->compression_type;
    }

    if (cmd_options->xz_compression) {
        sqlite_compression      = CR_CW_XZ_COMPRESSION;
        groupfile_compression   = CR_CW_XZ_COMPRESSION;
        prestodelta_compression = CR_CW_GZ_COMPRESSION;
    }

    sqlite_compression_suffix = cr_compression_suffix(sqlite_compression);
    prestodelta_compression_suffix = cr_compression_suffix(prestodelta_compression);


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

    pri_xml_filename = g_strconcat(tmp_out_repo, "/primary.xml.gz", NULL);
    fil_xml_filename = g_strconcat(tmp_out_repo, "/filelists.xml.gz", NULL);
    oth_xml_filename = g_strconcat(tmp_out_repo, "/other.xml.gz", NULL);

    pri_stat = cr_contentstat_new(cmd_options->checksum_type, NULL);
    pri_cr_file = cr_xmlfile_sopen_primary(pri_xml_filename,
                                           CR_CW_GZ_COMPRESSION,
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

    fil_stat = cr_contentstat_new(cmd_options->checksum_type, NULL);
    fil_cr_file = cr_xmlfile_sopen_filelists(fil_xml_filename,
                                            CR_CW_GZ_COMPRESSION,
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

    oth_stat = cr_contentstat_new(cmd_options->checksum_type, NULL);
    oth_cr_file = cr_xmlfile_sopen_other(oth_xml_filename,
                                        CR_CW_GZ_COMPRESSION,
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
    cr_xmlfile_set_num_of_pkgs(pri_cr_file, package_count, NULL);
    cr_xmlfile_set_num_of_pkgs(fil_cr_file, package_count, NULL);
    cr_xmlfile_set_num_of_pkgs(oth_cr_file, package_count, NULL);


    // Open sqlite databases

    gchar *pri_db_filename = NULL;
    gchar *fil_db_filename = NULL;
    gchar *oth_db_filename = NULL;
    cr_SqliteDb *pri_db = NULL;
    cr_SqliteDb *fil_db = NULL;
    cr_SqliteDb *oth_db = NULL;

    if (!cmd_options->no_database) {
        int pri_db_fd = -1;
        int fil_db_fd = -1;
        int oth_db_fd = -1;

        g_message("Preparing sqlite DBs");
        if (!cmd_options->local_sqlite) {
            g_debug("Creating databases");
            pri_db_filename = g_strconcat(tmp_out_repo, "/primary.sqlite", NULL);
            fil_db_filename = g_strconcat(tmp_out_repo, "/filelists.sqlite", NULL);
            oth_db_filename = g_strconcat(tmp_out_repo, "/other.sqlite", NULL);
        } else {
            g_debug("Creating databases localy");
            pri_db_filename = g_strdup("/tmp/primary.XXXXXX.sqlite");
            fil_db_filename = g_strdup("/tmp/filelists.XXXXXX.sqlite");
            oth_db_filename = g_strdup("/tmp/other.XXXXXXX.sqlite");
            pri_db_fd = g_mkstemp(pri_db_filename);
            g_debug("%s", pri_db_filename);
            if (pri_db_fd == -1) {
                g_critical("Cannot open %s: %s", pri_db_filename, strerror(errno));
                exit(EXIT_FAILURE);
            }
            fil_db_fd = g_mkstemp(fil_db_filename);
            g_debug("%s", fil_db_filename);
            if (fil_db_fd == -1) {
                g_critical("Cannot open %s: %s", fil_db_filename, strerror(errno));
                exit(EXIT_FAILURE);
            }
            oth_db_fd = g_mkstemp(oth_db_filename);
            g_debug("%s", oth_db_filename);
            if (oth_db_fd == -1) {
                g_critical("Cannot open %s: %s", oth_db_filename, strerror(errno));
                exit(EXIT_FAILURE);
            }
        }

        pri_db = cr_db_open_primary(pri_db_filename, &tmp_err);
        assert(pri_db || tmp_err);
        if (pri_db_fd > -1)
            close(pri_db_fd);
        if (!pri_db) {
            g_critical("Cannot open %s: %s",
                       pri_db_filename, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }

        fil_db = cr_db_open_filelists(fil_db_filename, &tmp_err);
        assert(fil_db || tmp_err);
        if (fil_db_fd > -1)
            close(fil_db_fd);
        if (!fil_db) {
            g_critical("Cannot open %s: %s",
                       fil_db_filename, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }

        oth_db = cr_db_open_other(oth_db_filename, &tmp_err);
        assert(oth_db || tmp_err);
        if (oth_db_fd > -1)
            close(oth_db_fd);
        if (!oth_db) {
            g_critical("Cannot open %s: %s",
                       oth_db_filename, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }
    }

    // Thread pool - User data initialization

    user_data.pri_f             = pri_cr_file;
    user_data.fil_f             = fil_cr_file;
    user_data.oth_f             = oth_cr_file;
    user_data.pri_db            = pri_db;
    user_data.fil_db            = fil_db;
    user_data.oth_db            = oth_db;
    user_data.changelog_limit   = cmd_options->changelog_limit;
    user_data.location_base     = cmd_options->location_base;
    user_data.checksum_type_str = cr_checksum_name_str(cmd_options->checksum_type);
    user_data.checksum_type     = cmd_options->checksum_type;
    user_data.checksum_cachedir = cmd_options->checksum_cachedir;
    user_data.skip_symlinks     = cmd_options->skip_symlinks;
    user_data.repodir_name_len  = strlen(in_dir);
    user_data.package_count     = package_count;
    user_data.skip_stat         = cmd_options->skip_stat;
    user_data.old_metadata      = old_metadata;
    user_data.mutex_pri         = g_mutex_new();
    user_data.mutex_fil         = g_mutex_new();
    user_data.mutex_oth         = g_mutex_new();
    user_data.cond_pri          = g_cond_new();
    user_data.cond_fil          = g_cond_new();
    user_data.cond_oth          = g_cond_new();
    user_data.id_pri            = 0;
    user_data.id_fil            = 0;
    user_data.id_oth            = 0;
    user_data.buffer            = g_queue_new();
    user_data.mutex_buffer      = g_mutex_new();
    user_data.deltas            = cmd_options->deltas;
    user_data.max_delta_rpm_size= cmd_options->max_delta_rpm_size;
    user_data.mutex_deltatargetpackages = g_mutex_new();
    user_data.deltatargetpackages = NULL;

    g_debug("Thread pool user data ready");


    // Start pool

    g_thread_pool_set_max_threads(pool, cmd_options->workers, NULL);
    g_message("Pool started (with %d workers)", cmd_options->workers);


    // Wait until pool is finished

    g_thread_pool_free(pool, FALSE, TRUE);
    g_message("Pool finished");

    cr_xml_dump_cleanup();

    cr_xmlfile_close(pri_cr_file, NULL);
    cr_xmlfile_close(fil_cr_file, NULL);
    cr_xmlfile_close(oth_cr_file, NULL);

    g_queue_free(user_data.buffer);
    g_mutex_free(user_data.mutex_buffer);
    g_cond_free(user_data.cond_pri);
    g_cond_free(user_data.cond_fil);
    g_cond_free(user_data.cond_oth);
    g_mutex_free(user_data.mutex_pri);
    g_mutex_free(user_data.mutex_fil);
    g_mutex_free(user_data.mutex_oth);
    g_mutex_free(user_data.mutex_deltatargetpackages);


    // Create repomd records for each file

    g_debug("Generating repomd.xml");

    cr_Repomd *repomd_obj = cr_repomd_new();

    cr_RepomdRecord *pri_xml_rec = cr_repomd_record_new("primary", pri_xml_filename);
    cr_RepomdRecord *fil_xml_rec = cr_repomd_record_new("filelists", fil_xml_filename);
    cr_RepomdRecord *oth_xml_rec = cr_repomd_record_new("other", oth_xml_filename);
    cr_RepomdRecord *pri_db_rec               = NULL;
    cr_RepomdRecord *fil_db_rec               = NULL;
    cr_RepomdRecord *oth_db_rec               = NULL;
    cr_RepomdRecord *groupfile_rec            = NULL;
    cr_RepomdRecord *compressed_groupfile_rec = NULL;
    cr_RepomdRecord *updateinfo_rec           = NULL;
    cr_RepomdRecord *prestodelta_rec          = NULL;


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
                                                cmd_options->checksum_type,
                                                NULL);
    g_thread_pool_push(fill_pool, pri_fill_task, NULL);

    fil_fill_task = cr_repomdrecordfilltask_new(fil_xml_rec,
                                                cmd_options->checksum_type,
                                                NULL);
    g_thread_pool_push(fill_pool, fil_fill_task, NULL);

    oth_fill_task = cr_repomdrecordfilltask_new(oth_xml_rec,
                                                cmd_options->checksum_type,
                                                NULL);
    g_thread_pool_push(fill_pool, oth_fill_task, NULL);

    // Groupfile

    if (groupfile) {
        groupfile_rec = cr_repomd_record_new("group", groupfile);
        compressed_groupfile_rec = cr_repomd_record_new("group_gz", groupfile);
        cr_repomd_record_compress_and_fill(groupfile_rec,
                                          compressed_groupfile_rec,
                                          cmd_options->checksum_type,
                                          groupfile_compression,
                                          &tmp_err);
        if (tmp_err) {
            g_critical("Cannot process groupfile %s: %s",
                       groupfile, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }
    }


    // Updateinfo

    if (updateinfo) {
        updateinfo_rec = cr_repomd_record_new("updateinfo", updateinfo);
        cr_repomd_record_fill(updateinfo_rec,
                              cmd_options->checksum_type,
                              &tmp_err);
        if (tmp_err) {
            g_critical("Cannot process updateinfo %s: %s",
                       updateinfo, tmp_err->message);
            g_clear_error(&tmp_err);
            exit(EXIT_FAILURE);
        }
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

        cr_db_dbinfo_update(pri_db, pri_xml_rec->checksum, NULL);
        cr_db_dbinfo_update(fil_db, fil_xml_rec->checksum, NULL);
        cr_db_dbinfo_update(oth_db, oth_xml_rec->checksum, NULL);

        cr_db_close(pri_db, NULL);
        cr_db_close(fil_db, NULL);
        cr_db_close(oth_db, NULL);


        // Compress dbs

        GThreadPool *compress_pool =  g_thread_pool_new(cr_compressing_thread,
                                                        NULL, 3, FALSE, NULL);

        cr_CompressionTask *pri_db_task;
        cr_CompressionTask *fil_db_task;
        cr_CompressionTask *oth_db_task;

        pri_db_task = cr_compressiontask_new(pri_db_filename,
                                             pri_db_name,
                                             sqlite_compression,
                                             cmd_options->checksum_type,
                                             1, NULL);
        g_thread_pool_push(compress_pool, pri_db_task, NULL);

        fil_db_task = cr_compressiontask_new(fil_db_filename,
                                             fil_db_name,
                                             sqlite_compression,
                                             cmd_options->checksum_type,
                                             1, NULL);
        g_thread_pool_push(compress_pool, fil_db_task, NULL);

        oth_db_task = cr_compressiontask_new(oth_db_filename,
                                             oth_db_name,
                                             sqlite_compression,
                                             cmd_options->checksum_type,
                                             1, NULL);
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
                                                       cmd_options->checksum_type,
                                                       NULL);
        g_thread_pool_push(fill_pool, pri_db_fill_task, NULL);

        fil_db_fill_task = cr_repomdrecordfilltask_new(fil_db_rec,
                                                       cmd_options->checksum_type,
                                                       NULL);
        g_thread_pool_push(fill_pool, fil_db_fill_task, NULL);

        oth_db_fill_task = cr_repomdrecordfilltask_new(oth_db_rec,
                                                       cmd_options->checksum_type,
                                                       NULL);
        g_thread_pool_push(fill_pool, oth_db_fill_task, NULL);

        g_thread_pool_free(fill_pool, FALSE, TRUE);

        cr_repomdrecordfilltask_free(pri_db_fill_task, NULL);
        cr_repomdrecordfilltask_free(fil_db_fill_task, NULL);
        cr_repomdrecordfilltask_free(oth_db_fill_task, NULL);
    }

#ifdef CR_DELTA_RPM_SUPPORT
    // Delta generation
    if (cmd_options->deltas) {
        gboolean ret;
        gchar *filename, *outdeltadir = NULL;
        gchar *prestodelta_xml_filename = NULL;
        GHashTable *ht_oldpackagedirs = NULL;
        cr_XmlFile *prestodelta_cr_file = NULL;
        cr_ContentStat *prestodelta_stat = NULL;

        filename = g_strconcat("prestodelta.xml",
                               prestodelta_compression_suffix,
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
            g_critical("Cannot create %s: %s", outdeltadir, strerror(errno));
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
        prestodelta_stat = cr_contentstat_new(cmd_options->checksum_type, NULL);
        prestodelta_cr_file = cr_xmlfile_sopen_prestodelta(prestodelta_xml_filename,
                                                           prestodelta_compression,
                                                           prestodelta_stat,
                                                           &tmp_err);
        if (!prestodelta_cr_file) {
            g_critical("Cannot open %s: %s", prestodelta_xml_filename, tmp_err->message);
            g_clear_error(&tmp_err);
            goto deltaerror;
        }

        ret = cr_deltarpms_generate_prestodelta_file(
                        outdeltadir,
                        prestodelta_cr_file,
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

        // 4) Prepare repomd record
        prestodelta_rec = cr_repomd_record_new("prestodelta", prestodelta_xml_filename);
        cr_repomd_record_load_contentstat(prestodelta_rec, prestodelta_stat);
        cr_repomd_record_fill(prestodelta_rec, cmd_options->checksum_type, NULL);

deltaerror:
        // 5) Cleanup
        g_hash_table_destroy(ht_oldpackagedirs);
        g_free(outdeltadir);
        g_free(prestodelta_xml_filename);
        cr_xmlfile_close(prestodelta_cr_file, NULL);
        cr_contentstat_free(prestodelta_stat, NULL);
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
        cr_repomd_record_rename_file(groupfile_rec, NULL);
        cr_repomd_record_rename_file(compressed_groupfile_rec, NULL);
        cr_repomd_record_rename_file(updateinfo_rec, NULL);
        cr_repomd_record_rename_file(prestodelta_rec, NULL);
    }


    // Gen xml

    cr_repomd_set_record(repomd_obj, pri_xml_rec);
    cr_repomd_set_record(repomd_obj, fil_xml_rec);
    cr_repomd_set_record(repomd_obj, oth_xml_rec);
    cr_repomd_set_record(repomd_obj, pri_db_rec);
    cr_repomd_set_record(repomd_obj, fil_db_rec);
    cr_repomd_set_record(repomd_obj, oth_db_rec);
    cr_repomd_set_record(repomd_obj, groupfile_rec);
    cr_repomd_set_record(repomd_obj, compressed_groupfile_rec);
    cr_repomd_set_record(repomd_obj, updateinfo_rec);
    cr_repomd_set_record(repomd_obj, prestodelta_rec);

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
        g_critical("Cannot open %s: %s", repomd_path, strerror(errno));
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
    gboolean ret;

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
                out_repo, old_repodata_path, strerror(errno));
    } else {
        g_debug("Renamed %s -> %s", out_repo, old_repodata_path);
        old_repodata_renamed = TRUE;
    }

    // Rename tmp_out_repo to out_repo
    if (g_rename(tmp_out_repo, out_repo) == -1) {
        g_critical("Cannot rename %s -> %s: %s", tmp_out_repo, out_repo,
                   strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        g_debug("Renamed %s -> %s", tmp_out_repo, out_repo);
    }

    // Remove lock
    if (!lock_dir_is_tmp_out_repo)
        cr_remove_dir(lock_dir, NULL);

    // Disable path stored for exit handler
    global_lock_dir = NULL;
    global_tmp_out_repo = NULL;

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
    g_free(groupfile);
    g_free(updateinfo);

    free_options(cmd_options);
    cr_package_parser_cleanup();

    g_debug("All done");
    exit(EXIT_SUCCESS);
}
