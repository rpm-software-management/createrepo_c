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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include "cmd_parser.h"
#include "compression_wrapper.h"
#include "constants.h"
#include "error.h"
#include "load_metadata.h"
#include "locate_metadata.h"
#include "misc.h"
#include "parsepkg.h"
#include "repomd.h"
#include "sqlite.h"
#include "version.h"
#include "xml_dump.h"
#include "xml_file.h"


#define G_LOG_DOMAIN        ((gchar*) 0)
#define MAX_TASK_BUFFER_LEN 20


struct UserData {
    GThreadPool *pool;              // thread pool
    cr_XmlFile *pri_f;              // Opened compressed primary.xml.*
    cr_XmlFile *fil_f;              // Opened compressed filelists.xml.*
    cr_XmlFile *oth_f;              // Opened compressed other.xml.*
    cr_DbPrimaryStatements pri_statements;  // Opened connection to primary.sqlite
    cr_DbFilelistsStatements fil_statements;// Opened connection to filelists.sqlite
    cr_DbOtherStatements oth_statements;    // Opened connection to other.sqlite
    int changelog_limit;            // Max number of changelogs for a package
    const char *location_base;      // Base location url
    int repodir_name_len;           // Len of path to repo /foo/bar/repodata
                                    //       This part     |<----->|
    const char *checksum_type_str;  // Name of selected checksum
    cr_ChecksumType checksum_type;  // Constant representing selected checksum
    gboolean skip_symlinks;         // Skip symlinks
    long package_count;             // Total number of packages to process

    // Update stuff
    gboolean skip_stat;             // Skip stat() while updating
    cr_Metadata old_metadata;       // Loaded metadata

    // Thread serialization
    GMutex *mutex_pri;              // Mutex for primary metadata
    GMutex *mutex_fil;              // Mutex for filelists metadata
    GMutex *mutex_oth;              // Mutex for other metadata
    GCond *cond_pri;                // Condition for primary metadata
    GCond *cond_fil;                // Condition for filelists metadata
    GCond *cond_oth;                // Condition for other metadata
    volatile long id_pri;           // ID of task on turn (write primary metadata)
    volatile long id_fil;           // ID of task on turn (write filelists metadata)
    volatile long id_oth;           // ID of task on turn (write other metadata)

    // Buffering
    GQueue *buffer;                 // Buffer for done tasks
    GMutex *mutex_buffer;           // Mutex for accessing the buffer
};


struct PoolTask {
    long  id;                       // ID of the task
    char* full_path;                // Complete path - /foo/bar/packages/foo.rpm
    char* filename;                 // Just filename - foo.rpm
    char* path;                     // Just path     - /foo/bar/packages
};


struct BufferedTask {
    long id;                        // ID of the task
    struct cr_XmlStruct res;        // XML for primary, filelists and other
    cr_Package *pkg;                // Package structure
    char *location_href;            // location_href path
    int pkg_from_md;                // If true - package structure if from
                                    // old metadata and must not be freed!
                                    // If false - package is from file and
                                    // it must be freed!
};


// Global variables used by signal handler
char *tmp_repodata_path = NULL;     // Path to temporary dir - /foo/bar/.repodata


// Signal handler
void
sigint_catcher(int sig)
{
    CR_UNUSED(sig);
    g_message("SIGINT catched: Terminating...");
    if (tmp_repodata_path)
        cr_remove_dir(tmp_repodata_path);
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


gint
buf_task_sort_func(gconstpointer a, gconstpointer b, gpointer data)
{
    CR_UNUSED(data);
    const struct BufferedTask *task_a = a;
    const struct BufferedTask *task_b = b;
    if (task_a->id < task_b->id)  return -1;
    if (task_a->id == task_b->id) return 0;
    return 1;
}


void
write_pkg(long id,
          struct cr_XmlStruct res,
          cr_Package *pkg,
          struct UserData *udata)
{

    // Write primary data
    g_mutex_lock(udata->mutex_pri);
    while (udata->id_pri != id)
        g_cond_wait (udata->cond_pri, udata->mutex_pri);
    udata->id_pri++;
    cr_xmlfile_add_chunk(udata->pri_f, (const char *) res.primary, NULL);
    if (udata->pri_statements)
        cr_db_add_primary_pkg(udata->pri_statements, pkg, NULL);
    g_mutex_unlock(udata->mutex_pri);
    g_cond_broadcast(udata->cond_pri);

    // Write fielists data
    g_mutex_lock(udata->mutex_fil);
    while (udata->id_fil != id)
        g_cond_wait (udata->cond_fil, udata->mutex_fil);
    udata->id_fil++;
    cr_xmlfile_add_chunk(udata->fil_f, (const char *) res.filelists, NULL);
    if (udata->fil_statements)
        cr_db_add_filelists_pkg(udata->fil_statements, pkg, NULL);
    g_mutex_unlock(udata->mutex_fil);
    g_cond_broadcast(udata->cond_fil);

    // Write other data
    g_mutex_lock(udata->mutex_oth);
    while (udata->id_oth != id)
        g_cond_wait (udata->cond_oth, udata->mutex_oth);
    udata->id_oth++;
    cr_xmlfile_add_chunk(udata->oth_f, (const char *) res.other, NULL);
    if (udata->oth_statements)
        cr_db_add_other_pkg(udata->oth_statements, pkg, NULL);
    g_mutex_unlock(udata->mutex_oth);
    g_cond_broadcast(udata->cond_oth);
}


void
dumper_thread(gpointer data, gpointer user_data)
{
    gboolean old_used = FALSE;  // To use old metadata?
    cr_Package *md  = NULL;     // Package from loaded MetaData
    cr_Package *pkg = NULL;     // Package from file
    struct stat stat_buf;       // Struct with info from stat() on file
    struct cr_XmlStruct res;    // Structure for generated XML

    struct UserData *udata = (struct UserData *) user_data;
    struct PoolTask *task  = (struct PoolTask *) data;

    // get location_href without leading part of path (path to repo)
    // including '/' char
    const char *location_href = task->full_path + udata->repodir_name_len;
    const char *location_base = udata->location_base;

    // Get stat info about file
    if (udata->old_metadata && !(udata->skip_stat)) {
        if (stat(task->full_path, &stat_buf) == -1) {
            g_critical("Stat() on %s: %s", task->full_path, strerror(errno));
            goto task_cleanup;
        }
    }

    // Update stuff
    if (udata->old_metadata) {
        // We have old metadata
        md = (cr_Package *) g_hash_table_lookup (udata->old_metadata->ht,
                                                 task->filename);

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
                // We have usable old data, but we have to set proper locations
                // WARNING! This two lines destructively modifies content of
                // packages in old metadata.
                md->location_href = (char *) location_href;
                md->location_base = (char *) location_base;
            }
        }
    }

    // Load package and gen XML metadata
    if (!old_used) {
        // Load package from file
        pkg = cr_package_from_rpm(task->full_path, udata->checksum_type,
                                   location_href, udata->location_base,
                                   udata->changelog_limit, NULL, NULL);
        if (!pkg) {
            g_warning("Cannot read package: %s", task->full_path);
            goto task_cleanup;
        }
        res = cr_xml_dump(pkg, NULL);
    } else {
        // Just gen XML from old loaded metadata
        pkg = md;
        res = cr_xml_dump(md, NULL);
    }

    g_mutex_lock(udata->mutex_buffer);
    if (g_queue_get_length(udata->buffer) < MAX_TASK_BUFFER_LEN
        && udata->id_pri != task->id
        && udata->package_count != (task->id + 1))
    {
        // If it isn't our turn and buffer isn't full and this isn't
        // last task -> save task to buffer
        struct BufferedTask *buf_task = malloc(sizeof(struct BufferedTask));
        buf_task->id  = task->id;
        buf_task->res = res;
        buf_task->pkg = pkg;
        buf_task->location_href = NULL;
        buf_task->pkg_from_md = (pkg == md) ? 1 : 0;
        // We MUST store location_href for reused packages who goes to the buffer
        // We don't need to store location_base because it is "alive" in
        // user data during this function calls.
        if (pkg == md) {
            buf_task->location_href = g_strdup(location_href);
            buf_task->pkg->location_href = buf_task->location_href;
        }

        g_queue_insert_sorted(udata->buffer, buf_task, buf_task_sort_func, NULL);
        g_mutex_unlock(udata->mutex_buffer);

        g_free(task->full_path);
        g_free(task->filename);
        g_free(task->path);
        g_free(task);

        return;
    }
    g_mutex_unlock(udata->mutex_buffer);

    // Dump XML and SQLite
    write_pkg(task->id, res, pkg, udata);

    // Clean up
    if (pkg != md)
        cr_package_free(pkg);
    g_free(res.primary);
    g_free(res.filelists);
    g_free(res.other);

    // Try to write all results from buffer which was waiting for us
    while (1) {
        struct BufferedTask *buf_task;
        g_mutex_lock(udata->mutex_buffer);
        buf_task = g_queue_peek_head(udata->buffer);
        if (buf_task && buf_task->id == udata->id_pri) {
            buf_task = g_queue_pop_head (udata->buffer);
            g_mutex_unlock(udata->mutex_buffer);
            // Dump XML and SQLite
            write_pkg(buf_task->id, buf_task->res, buf_task->pkg, udata);
            // Clean up
            if (!buf_task->pkg_from_md)
                cr_package_free(buf_task->pkg);
            g_free(buf_task->res.primary);
            g_free(buf_task->res.filelists);
            g_free(buf_task->res.other);
            g_free(buf_task->location_href);
            g_free(buf_task);
        } else {
            g_mutex_unlock(udata->mutex_buffer);
            break;
        }
    }


task_cleanup:
    if (udata->id_pri <= task->id) {
        // An error was encountered and we have to wait to increment counters
        g_mutex_lock(udata->mutex_pri);
        while (udata->id_pri != task->id)
            g_cond_wait (udata->cond_pri, udata->mutex_pri);
        udata->id_pri++;
        g_mutex_unlock(udata->mutex_pri);
        g_cond_broadcast(udata->cond_pri);

        g_mutex_lock(udata->mutex_fil);
        while (udata->id_fil != task->id)
            g_cond_wait (udata->cond_fil, udata->mutex_fil);
        udata->id_fil++;
        g_mutex_unlock(udata->mutex_fil);
        g_cond_broadcast(udata->cond_fil);

        g_mutex_lock(udata->mutex_oth);
        while (udata->id_oth != task->id)
            g_cond_wait (udata->cond_oth, udata->mutex_oth);
        udata->id_oth++;
        g_mutex_unlock(udata->mutex_oth);
        g_cond_broadcast(udata->cond_oth);
    }

    g_free(task->full_path);
    g_free(task->filename);
    g_free(task->path);
    g_free(task);

    return;
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

    if (!(cmd_options->include_pkgs)) {
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
                    if (!g_file_test(full_path, G_FILE_TEST_IS_REGULAR) &&
                        g_file_test(full_path, G_FILE_TEST_IS_DIR))
                    {
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
                } else
                    g_free(full_path);
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
            gchar *filename;  // foobar.rpm

            // Get index of last '/'
            int rel_path_len = strlen(relative_path);
            int x = rel_path_len;
            for (; x > 0; x--)
                if (relative_path[x] == '/')
                    break;

            if (!x) {
                // There was no '/' in path
                filename = relative_path;
            } else {
                filename = relative_path + x + 1;
            }

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
        package_count++;
    }

    return package_count;
}


int
main(int argc, char **argv)
{
    // Arguments parsing

    struct CmdOptions *cmd_options;
    cmd_options = parse_arguments(&argc, &argv);
    if (!cmd_options) {
        exit(1);
    }


    // Arguments pre-check

    if (cmd_options->version) {
        printf("Version: %d.%d.%d\n", CR_VERSION_MAJOR,
                                      CR_VERSION_MINOR,
                                      CR_VERSION_PATCH);
        free_options(cmd_options);
        exit(0);
    } else if (argc != 2) {
        fprintf(stderr, "Must specify exactly one directory to index.\n");
        fprintf(stderr, "Usage: %s [options] <directory_to_index>\n\n",
                         cr_get_filename(argv[0]));
        free_options(cmd_options);
        exit(1);
    }


    // Dirs

    gchar *in_dir       = NULL;  // path/to/repo/
    gchar *in_repo      = NULL;  // path/to/repo/repodata/
    gchar *out_dir      = NULL;  // path/to/out_repo/
    gchar *out_repo     = NULL;  // path/to/out_repo/repodata/
    gchar *tmp_out_repo = NULL;  // path/to/out_repo/.repodata/

    if (cmd_options->basedir && !g_str_has_prefix(argv[1], "/")) {
        gchar *tmp = cr_normalize_dir_path(argv[1]);
        in_dir = g_build_filename(cmd_options->basedir, tmp, NULL);
        g_free(tmp);
    } else {
        in_dir = cr_normalize_dir_path(argv[1]);
    }

    cmd_options->input_dir = g_strdup(in_dir);


    // Check if inputdir exists

    if (!g_file_test(in_dir, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        g_warning("Directory %s must exist", in_dir);
        g_free(in_dir);
        free_options(cmd_options);
        exit(1);
    }


    // Check parsed arguments

    if (!check_arguments(cmd_options)) {
        g_free(in_dir);
        free_options(cmd_options);
        exit(1);
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
        tmp_out_repo = g_strconcat(out_dir, ".repodata/", NULL);
    } else {
        out_dir  = g_strdup(in_dir);
        out_repo = g_strdup(in_repo);
        tmp_out_repo = g_strconcat(out_dir, ".repodata/", NULL);
    }


    // Check if tmp_out_repo exists & Create tmp_out_repo dir

    if (g_mkdir (tmp_out_repo, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
        if (errno == EEXIST) {
            g_critical("Temporary repodata directory: %s already exists! ("
                       "Another createrepo process is running?)", tmp_out_repo);
        } else {
            g_critical("Error while creating temporary repodata directory %s: %s",
                       tmp_out_repo, strerror(errno));
        }

        exit(1);
    }


    // Set handler for sigint

    tmp_repodata_path = tmp_out_repo;

    g_debug("SIGINT handler setup");
    struct sigaction sigact;
    sigact.sa_handler = sigint_catcher;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    if (sigaction(SIGINT, &sigact, NULL) == -1) {
        g_critical("sigaction(): %s", strerror(errno));
        exit(1);
    }


    // Open package list

    FILE *output_pkg_list = NULL;
    if (cmd_options->read_pkgs_list) {
        output_pkg_list = fopen(cmd_options->read_pkgs_list, "w");
        if (!output_pkg_list) {
            g_critical("Cannot open \"%s\" for writing",
                       cmd_options->read_pkgs_list);
            exit(1);
        }
    }


    // Thread pool - Creation

    struct UserData user_data;
    g_thread_init(NULL);
    GThreadPool *pool = g_thread_pool_new(dumper_thread,
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

    cr_Metadata old_metadata = NULL;
    struct cr_MetadataLocation *old_metadata_location = NULL;

    if (!package_count)
        g_debug("No packages found - skipping metadata loading");

    if (package_count && cmd_options->update) {
        int ret;
        old_metadata = cr_metadata_new(CR_HT_KEY_FILENAME, 1, current_pkglist);

        if (cmd_options->outputdir)
            old_metadata_location = cr_locate_metadata(out_dir, 1, NULL);
        else
            old_metadata_location = cr_locate_metadata(in_dir, 1, NULL);

        if (old_metadata_location) {
            ret = cr_metadata_load_xml(old_metadata, old_metadata_location, NULL);
            if (ret == CRE_OK)
                g_debug("Old metadata from: %s - loaded", out_dir);
            else
                g_debug("Old metadata from %s - loading failed", out_dir);
        }

        // Load repodata from --update-md-path
        GSList *element = cmd_options->l_update_md_paths;
        for (; element; element = g_slist_next(element)) {
            char *path = (char *) element->data;
            g_message("Loading metadata from md-path: %s", path);
            ret = cr_metadata_locate_and_load_xml(old_metadata, path, NULL);
            if (ret == CRE_OK)
                g_debug("Metadata from md-path %s - loaded", path);
            else
                g_warning("Metadata from md-path %s - loading failed", path);
        }

        g_message("Loaded information about %d packages",
                  g_hash_table_size(old_metadata->ht));
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
        ret = cr_better_copy_file(cmd_options->groupfile_fullpath, groupfile);
        if (ret != CR_COPY_OK) {
            g_critical("Error while copy %s -> %s",
                       cmd_options->groupfile_fullpath, groupfile);
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

        if (cr_better_copy_file(src_groupfile, groupfile) != CR_COPY_OK)
            g_critical("Error while copy %s -> %s", src_groupfile, groupfile);
    }


    // Copy update info

    char *updateinfo = NULL;

    if (cmd_options->update && cmd_options->keep_all_metadata &&
        old_metadata_location && old_metadata_location->updateinfo_href)
    {
        gchar *src_updateinfo = old_metadata_location->updateinfo_href;
        updateinfo = g_strconcat(tmp_out_repo,
                                 cr_get_filename(src_updateinfo),
                                 NULL);

        g_debug("Copy updateinfo %s -> %s", src_updateinfo, updateinfo);

        if (cr_better_copy_file(src_updateinfo, updateinfo) != CR_COPY_OK)
            g_critical("Error while copy %s -> %s", src_updateinfo, updateinfo);
    }

    cr_metadatalocation_free(old_metadata_location);
    old_metadata_location = NULL;


    // Setup compression types

    const char *sqlite_compression_suffix = NULL;
    cr_CompressionType sqlite_compression = CR_CW_BZ2_COMPRESSION;
    cr_CompressionType groupfile_compression = CR_CW_GZ_COMPRESSION;

    if (cmd_options->compression_type != CR_CW_UNKNOWN_COMPRESSION) {
        sqlite_compression    = cmd_options->compression_type;
        groupfile_compression = cmd_options->compression_type;
    }

    if (cmd_options->xz_compression) {
        sqlite_compression    = CR_CW_XZ_COMPRESSION;
        groupfile_compression = CR_CW_XZ_COMPRESSION;
    }

    sqlite_compression_suffix = cr_compression_suffix(sqlite_compression);


    // Create and open new compressed files

    cr_XmlFile *pri_cr_file;
    cr_XmlFile *fil_cr_file;
    cr_XmlFile *oth_cr_file;

    gchar *pri_xml_filename;
    gchar *fil_xml_filename;
    gchar *oth_xml_filename;

    g_message("Temporary output repo path: %s", tmp_out_repo);
    g_debug("Creating .xml.gz files");

    pri_xml_filename = g_strconcat(tmp_out_repo, "/primary.xml.gz", NULL);
    fil_xml_filename = g_strconcat(tmp_out_repo, "/filelists.xml.gz", NULL);
    oth_xml_filename = g_strconcat(tmp_out_repo, "/other.xml.gz", NULL);

    pri_cr_file = cr_xmlfile_open_primary(pri_xml_filename,
                                          CR_CW_GZ_COMPRESSION,
                                          NULL);
    if (!pri_cr_file) {
        g_critical("Cannot open file: %s", pri_xml_filename);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        exit(1);
    }

    fil_cr_file = cr_xmlfile_open_filelists(fil_xml_filename,
                                            CR_CW_GZ_COMPRESSION,
                                            NULL);
    if (!fil_cr_file) {
        g_critical("Cannot open file: %s", fil_xml_filename);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        cr_xmlfile_close(pri_cr_file, NULL);
        exit(1);
    }

    oth_cr_file = cr_xmlfile_open_other(oth_xml_filename,
                                        CR_CW_GZ_COMPRESSION,
                                        NULL);
    if (!oth_cr_file) {
        g_critical("Cannot open file: %s", oth_xml_filename);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        cr_xmlfile_close(fil_cr_file, NULL);
        cr_xmlfile_close(pri_cr_file, NULL);
        exit(1);
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
    sqlite3 *pri_db = NULL;
    sqlite3 *fil_db = NULL;
    sqlite3 *oth_db = NULL;
    cr_DbPrimaryStatements pri_statements   = NULL;
    cr_DbFilelistsStatements fil_statements = NULL;
    cr_DbOtherStatements oth_statements     = NULL;

    if (!cmd_options->no_database) {
        g_message("Preparing sqlite DBs");
        g_debug("Creating databases");
        pri_db_filename = g_strconcat(tmp_out_repo, "/primary.sqlite", NULL);
        fil_db_filename = g_strconcat(tmp_out_repo, "/filelists.sqlite", NULL);
        oth_db_filename = g_strconcat(tmp_out_repo, "/other.sqlite", NULL);
        pri_db = cr_db_open_primary(pri_db_filename, NULL);
        fil_db = cr_db_open_filelists(fil_db_filename, NULL);
        oth_db = cr_db_open_other(oth_db_filename, NULL);
        pri_statements = cr_db_prepare_primary_statements(pri_db, NULL);
        fil_statements = cr_db_prepare_filelists_statements(fil_db, NULL);
        oth_statements = cr_db_prepare_other_statements(oth_db, NULL);
    }


    // Init package parser

    cr_package_parser_init();
    cr_xml_dump_init();


    // Thread pool - User data initialization

    user_data.pri_f             = pri_cr_file;
    user_data.fil_f             = fil_cr_file;
    user_data.oth_f             = oth_cr_file;
    user_data.pri_statements    = pri_statements;
    user_data.fil_statements    = fil_statements;
    user_data.oth_statements    = oth_statements;
    user_data.changelog_limit   = cmd_options->changelog_limit;
    user_data.location_base     = cmd_options->location_base;
    user_data.checksum_type_str = cr_checksum_name_str(cmd_options->checksum_type);
    user_data.checksum_type     = cmd_options->checksum_type;
    user_data.skip_symlinks     = cmd_options->skip_symlinks;
    user_data.skip_stat         = cmd_options->skip_stat;
    user_data.old_metadata      = old_metadata;
    user_data.repodir_name_len  = strlen(in_dir);
    user_data.package_count     = package_count;
    user_data.buffer            = g_queue_new();
    user_data.mutex_buffer      = g_mutex_new();
    user_data.mutex_pri         = g_mutex_new();
    user_data.mutex_fil         = g_mutex_new();
    user_data.mutex_oth         = g_mutex_new();
    user_data.cond_pri          = g_cond_new();
    user_data.cond_fil          = g_cond_new();
    user_data.cond_oth          = g_cond_new();
    user_data.id_pri            = 0;
    user_data.id_fil            = 0;
    user_data.id_oth            = 0;

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


    // Create repomd records for each file

    g_debug("Generating repomd.xml");

    cr_Repomd repomd_obj = cr_repomd_new();

    cr_RepomdRecord pri_xml_rec = cr_repomd_record_new(pri_xml_filename);
    cr_RepomdRecord fil_xml_rec = cr_repomd_record_new(fil_xml_filename);
    cr_RepomdRecord oth_xml_rec = cr_repomd_record_new(oth_xml_filename);
    cr_RepomdRecord pri_db_rec               = NULL;
    cr_RepomdRecord fil_db_rec               = NULL;
    cr_RepomdRecord oth_db_rec               = NULL;
    cr_RepomdRecord groupfile_rec            = NULL;
    cr_RepomdRecord compressed_groupfile_rec = NULL;
    cr_RepomdRecord updateinfo_rec           = NULL;


    // XML

    cr_repomd_record_fill(pri_xml_rec, cmd_options->checksum_type, NULL);
    cr_repomd_record_fill(fil_xml_rec, cmd_options->checksum_type, NULL);
    cr_repomd_record_fill(oth_xml_rec, cmd_options->checksum_type, NULL);


    // Groupfile

    if (groupfile) {
        groupfile_rec = cr_repomd_record_new(groupfile);
        compressed_groupfile_rec = cr_repomd_record_new(groupfile);
        cr_repomd_record_compress_and_fill(groupfile_rec,
                                          compressed_groupfile_rec,
                                          cmd_options->checksum_type,
                                          groupfile_compression,
                                          NULL);
    }


    // Updateinfo

    if (updateinfo) {
        updateinfo_rec = cr_repomd_record_new(updateinfo);
        cr_repomd_record_fill(updateinfo_rec, cmd_options->checksum_type, NULL);
    }


    // Sqlite db

    if (!cmd_options->no_database) {

        gchar *pri_db_name = g_strconcat(pri_db_filename,
                                         sqlite_compression_suffix, NULL);
        gchar *fil_db_name = g_strconcat(fil_db_filename,
                                         sqlite_compression_suffix, NULL);
        gchar *oth_db_name = g_strconcat(oth_db_filename,
                                         sqlite_compression_suffix, NULL);

        cr_db_dbinfo_update(pri_db, pri_xml_rec->checksum, NULL);
        cr_db_dbinfo_update(fil_db, fil_xml_rec->checksum, NULL);
        cr_db_dbinfo_update(oth_db, oth_xml_rec->checksum, NULL);

        cr_db_destroy_primary_statements(pri_statements);
        cr_db_destroy_filelists_statements(fil_statements);
        cr_db_destroy_other_statements(oth_statements);

        cr_db_close_primary(pri_db, NULL);
        cr_db_close_filelists(fil_db, NULL);
        cr_db_close_other(oth_db, NULL);


        // Compress dbs

        cr_compress_file(pri_db_filename, NULL, sqlite_compression);
        cr_compress_file(fil_db_filename, NULL, sqlite_compression);
        cr_compress_file(oth_db_filename, NULL, sqlite_compression);

        remove(pri_db_filename);
        remove(fil_db_filename);
        remove(oth_db_filename);


        // Prepare repomd records

        pri_db_rec = cr_repomd_record_new(pri_db_name);
        fil_db_rec = cr_repomd_record_new(fil_db_name);
        oth_db_rec = cr_repomd_record_new(oth_db_name);

        cr_repomd_record_fill(pri_db_rec, cmd_options->checksum_type, NULL);
        cr_repomd_record_fill(fil_db_rec, cmd_options->checksum_type, NULL);
        cr_repomd_record_fill(oth_db_rec, cmd_options->checksum_type, NULL);

        g_free(pri_db_name);
        g_free(fil_db_name);
        g_free(oth_db_name);
    }


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
    }


    // Gen xml

    cr_repomd_set_record(repomd_obj, pri_xml_rec,    "primary");
    cr_repomd_set_record(repomd_obj, fil_xml_rec,    "filelists");
    cr_repomd_set_record(repomd_obj, oth_xml_rec,    "other");
    cr_repomd_set_record(repomd_obj, pri_db_rec,     "primary_db");
    cr_repomd_set_record(repomd_obj, fil_db_rec,     "filelists_db");
    cr_repomd_set_record(repomd_obj, oth_db_rec,     "other_db");
    cr_repomd_set_record(repomd_obj, groupfile_rec,  "group");
    cr_repomd_set_record(repomd_obj, compressed_groupfile_rec, "group_gz");
    cr_repomd_set_record(repomd_obj, updateinfo_rec, "updateinfo");

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

    char *repomd_xml = cr_repomd_xml_dump(repomd_obj);

    cr_repomd_free(repomd_obj);


    // Write repomd.xml

    gchar *repomd_path = g_strconcat(tmp_out_repo, "repomd.xml", NULL);
    FILE *frepomd = fopen(repomd_path, "w");
    if (!frepomd || !repomd_xml) {
        g_critical("Generate of repomd.xml failed");
        return 1;
    }
    fputs(repomd_xml, frepomd);
    fclose(frepomd);
    g_free(repomd_xml);
    g_free(repomd_path);


    // Move files from out_repo into tmp_out_repo

    g_debug("Moving data from %s", out_repo);
    if (g_file_test(out_repo, G_FILE_TEST_EXISTS)) {

        // Delete old metadata
        g_debug("Removing old metadata from %s", out_repo);
        cr_remove_metadata_classic(out_dir, cmd_options->retain_old, NULL);

        // Move files from out_repo to tmp_out_repo
        GDir *dirp;
        dirp = g_dir_open (out_repo, 0, NULL);
        if (!dirp) {
            g_critical("Cannot open directory: %s", out_repo);
            exit(1);
        }

        const gchar *filename;
        while ((filename = g_dir_read_name(dirp))) {
            gchar *full_path = g_strconcat(out_repo, filename, NULL);
            gchar *new_full_path = g_strconcat(tmp_out_repo, filename, NULL);

            // Do not override new file with the old one
            if (g_file_test(new_full_path, G_FILE_TEST_EXISTS)) {
                g_debug("Skip move of: %s -> %s (the destination file already exists)",
                        full_path, new_full_path);
                g_debug("Removing: %s", full_path);
                g_remove(full_path);
                g_free(full_path);
                g_free(new_full_path);
                continue;
            }

            if (g_rename(full_path, new_full_path) == -1)
                g_critical("Cannot move file %s -> %s", full_path, new_full_path);
            else
                g_debug("Moved %s -> %s", full_path, new_full_path);

            g_free(full_path);
            g_free(new_full_path);
        }

        g_dir_close(dirp);

        // Remove out_repo
        if (g_rmdir(out_repo) == -1) {
            g_critical("Cannot remove %s", out_repo);
        } else {
            g_debug("Old out repo %s removed", out_repo);
        }
    }


    // Rename tmp_out_repo to out_repo
    if (g_rename(tmp_out_repo, out_repo) == -1) {
        g_critical("Cannot rename %s -> %s", tmp_out_repo, out_repo);
    } else {
        g_debug("Renamed %s -> %s", tmp_out_repo, out_repo);
    }


    // Clean up

    g_debug("Memory cleanup");

    if (old_metadata)
        cr_metadata_free(old_metadata);

    g_free(in_repo);
    g_free(out_repo);
    tmp_repodata_path = NULL;
    g_free(tmp_out_repo);
    g_free(in_dir);
    g_free(out_dir);
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
    return 0;
}
