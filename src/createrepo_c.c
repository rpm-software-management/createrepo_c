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
#include "version.h"
#include "constants.h"
#include "parsepkg.h"
#include <fcntl.h>
#include "locate_metadata.h"
#include "load_metadata.h"
#include "repomd.h"
#include "compression_wrapper.h"
#include "misc.h"
#include "cmd_parser.h"
#include "xml_dump.h"
#include "sqlite.h"


#define G_LOG_DOMAIN    ((gchar*) 0)



struct UserData {
    CR_FILE *pri_f;
    CR_FILE *fil_f;
    CR_FILE *oth_f;
    cr_DbPrimaryStatements pri_statements;
    cr_DbFilelistsStatements fil_statements;
    cr_DbOtherStatements oth_statements;
    int changelog_limit;
    const char *location_base;
    int repodir_name_len;
    const char *checksum_type_str;
    cr_ChecksumType checksum_type;
    gboolean quiet;
    gboolean verbose;
    gboolean skip_symlinks;
    int package_count;

    // Update stuff
    gboolean skip_stat;
    cr_Metadata old_metadata;
};


struct PoolTask {
    char* full_path;
    char* filename;
    char* path;
};


// Global variables used by signal handler
char *tmp_repodata_path = NULL;


// Signal handler
void
sigint_catcher(int sig)
{
    CR_UNUSED(sig);
    g_message("SIGINT catched: Terminating...");
    if (tmp_repodata_path) {
        cr_remove_dir(tmp_repodata_path);
    }
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



#define LOCK_PRI        0
#define LOCK_FIL        1
#define LOCK_OTH        2

G_LOCK_DEFINE (LOCK_PRI);
G_LOCK_DEFINE (LOCK_FIL);
G_LOCK_DEFINE (LOCK_OTH);


void
dumper_thread(gpointer data, gpointer user_data)
{
    gboolean old_used = FALSE;
    cr_Package *md = NULL;
    cr_Package *pkg = NULL;
    struct stat stat_buf;
    struct cr_XmlStruct res;

    struct UserData *udata = (struct UserData *) user_data;
    struct PoolTask *task = (struct PoolTask *) data;

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
        }

        if (old_used) {
            // We have usable old data, but we have to set proper locations
            md->location_href = (char *) location_href;
            md->location_base = (char *) location_base;
        }
    }

    if (!old_used) {
        pkg = cr_package_from_file(task->full_path, udata->checksum_type,
                                   location_href, udata->location_base,
                                   udata->changelog_limit, NULL);
        if (!pkg) {
            g_warning("Cannot read package: %s", task->full_path);
            goto task_cleanup;
        }
        res = cr_xml_dump(pkg);
    } else {
        pkg = md;
        res = cr_xml_dump(md);
    }

    // Write primary data
    G_LOCK(LOCK_PRI);
    cr_puts(udata->pri_f, (const char *) res.primary);
    if (udata->pri_statements) {
        cr_add_primary_pkg_db(udata->pri_statements, pkg);
    }
    G_UNLOCK(LOCK_PRI);

    // Write fielists data
    G_LOCK(LOCK_FIL);
    cr_puts(udata->fil_f, (const char *) res.filelists);
    if (udata->fil_statements) {
        cr_add_filelists_pkg_db(udata->fil_statements, pkg);
    }
    G_UNLOCK(LOCK_FIL);

    // Write other data
    G_LOCK(LOCK_OTH);
    cr_puts(udata->oth_f, (const char *) res.other);
    if (udata->oth_statements) {
        cr_add_other_pkg_db(udata->oth_statements, pkg);
    }
    G_UNLOCK(LOCK_OTH);


    // Clean up

    if (pkg != md)
        cr_package_free(pkg);

    g_free(res.primary);
    g_free(res.filelists);
    g_free(res.other);

task_cleanup:
    g_free(task->full_path);
    g_free(task->filename);
    g_free(task->path);
    g_free(task);

    return;
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
        printf("Version: %d.%d.%d\n", CR_MAJOR_VERSION,
                                      CR_MINOR_VERSION,
                                      CR_PATCH_VERSION);
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


    // Recursive walk

    GSList *current_pkglist = NULL;
    /* ^^^ List with basenames of files which will be processed */
    int package_count = 0;

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
                    struct PoolTask *task = g_malloc(sizeof(struct PoolTask));
                    task->full_path = full_path;
                    task->filename = g_strdup(filename);
                    task->path = g_strdup(dirname);
                    if (output_pkg_list)
                        fprintf(output_pkg_list, "%s\n", repo_relative_path);
                    current_pkglist = g_slist_prepend(current_pkglist, task->filename);
                    // TODO: One common path for all tasks with the same path?
                    g_thread_pool_push(pool, task, NULL);
                    package_count++;
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
                struct PoolTask *task = g_malloc(sizeof(struct PoolTask));
                task->full_path = full_path;
                task->filename  = g_strdup(filename);         // foobar.rpm
                task->path      = strndup(relative_path, x);  // packages/i386/
                if (output_pkg_list)
                    fprintf(output_pkg_list, "%s\n", relative_path);
                current_pkglist = g_slist_prepend(current_pkglist, task->filename);
                g_thread_pool_push(pool, task, NULL);
                package_count++;
            }
        }
    }

    g_debug("Package count: %d", package_count);
    g_message("Directory walk done - %d packages", package_count);

    if (output_pkg_list)
        fclose(output_pkg_list);


    // Load old metadata if --update

    cr_Metadata old_metadata = NULL;
    struct cr_MetadataLocation *old_metadata_location = NULL;

    if (!package_count)
        g_debug("No packages found - skipping metadata loading");

    if (package_count && cmd_options->update) {
        int ret;
        old_metadata = cr_new_metadata(CR_HT_KEY_FILENAME, 1);

        if (cmd_options->outputdir)
            old_metadata_location = cr_get_metadata_location(out_dir, 1);
        else
            old_metadata_location = cr_get_metadata_location(in_dir, 1);

        ret = cr_load_xml_metadata(old_metadata,
                                   old_metadata_location,
                                   current_pkglist);
        if (ret == CR_LOAD_METADATA_OK)
            g_debug("Old metadata from: %s - loaded", out_dir);
        else
            g_debug("Old metadata from %s - loading failed", out_dir);

        // Load repodata from --update-md-path
        GSList *element = cmd_options->l_update_md_paths;
        for (; element; element = g_slist_next(element)) {
            char *path = (char *) element->data;
            g_message("Loading metadata from: %s", path);
            ret = cr_locate_and_load_xml_metadata(old_metadata,
                                                  path,
                                                  current_pkglist);
            if (ret == CR_LOAD_METADATA_OK)
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

    cr_free_metadata_location(old_metadata_location);
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

    CR_FILE *pri_cr_file;
    CR_FILE *fil_cr_file;
    CR_FILE *oth_cr_file;

    gchar *pri_xml_filename;
    gchar *fil_xml_filename;
    gchar *oth_xml_filename;

    g_message("Temporary output repo path: %s", tmp_out_repo);
    g_debug("Creating .xml.gz files");

    pri_xml_filename = g_strconcat(tmp_out_repo, "/primary.xml.gz", NULL);
    fil_xml_filename = g_strconcat(tmp_out_repo, "/filelists.xml.gz", NULL);
    oth_xml_filename = g_strconcat(tmp_out_repo, "/other.xml.gz", NULL);

    pri_cr_file = cr_open(pri_xml_filename,
                          CR_CW_MODE_WRITE,
                          CR_CW_GZ_COMPRESSION);
    if (!pri_cr_file) {
        g_critical("Cannot open file: %s", pri_xml_filename);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        exit(1);
    }

    fil_cr_file = cr_open(fil_xml_filename,
                          CR_CW_MODE_WRITE,
                          CR_CW_GZ_COMPRESSION);
    if (!fil_cr_file) {
        g_critical("Cannot open file: %s", fil_xml_filename);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        cr_close(pri_cr_file);
        exit(1);
    }

    oth_cr_file = cr_open(oth_xml_filename,
                          CR_CW_MODE_WRITE,
                          CR_CW_GZ_COMPRESSION);
    if (!oth_cr_file) {
        g_critical("Cannot open file: %s", oth_xml_filename);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        cr_close(fil_cr_file);
        cr_close(pri_cr_file);
        exit(1);
    }


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
        pri_db = cr_open_primary_db(pri_db_filename, NULL);
        fil_db = cr_open_filelists_db(fil_db_filename, NULL);
        oth_db = cr_open_other_db(oth_db_filename, NULL);
        pri_statements = cr_prepare_primary_db_statements(pri_db, NULL);
        fil_statements = cr_prepare_filelists_db_statements(fil_db, NULL);
        oth_statements = cr_prepare_other_db_statements(oth_db, NULL);
    }


    // Init package parser

    cr_package_parser_init();
    cr_dumper_init();


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
    user_data.quiet             = cmd_options->quiet;
    user_data.verbose           = cmd_options->verbose;
    user_data.skip_symlinks     = cmd_options->skip_symlinks;
    user_data.skip_stat         = cmd_options->skip_stat;
    user_data.old_metadata      = old_metadata;
    user_data.repodir_name_len  = strlen(in_dir);
    user_data.package_count = package_count;

    g_debug("Thread pool user data ready");


    // Write XML header

    g_debug("Writing xml headers");

    cr_printf(user_data.pri_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              "<metadata xmlns=\""CR_XML_COMMON_NS"\" xmlns:rpm=\""
              CR_XML_RPM_NS"\" packages=\"%d\">\n", package_count);
    cr_printf(user_data.fil_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              "<filelists xmlns=\""CR_XML_FILELISTS_NS"\" packages=\"%d\">\n",
              package_count);
    cr_printf(user_data.oth_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              "<otherdata xmlns=\""CR_XML_OTHER_NS"\" packages=\"%d\">\n",
              package_count);


    // Start pool

    g_thread_pool_set_max_threads(pool, cmd_options->workers, NULL);
    g_message("Pool started (with %d workers)", cmd_options->workers);


    // Wait until pool is finished

    g_thread_pool_free(pool, FALSE, TRUE);
    g_message("Pool finished");

    cr_dumper_cleanup();

    cr_puts(user_data.pri_f, "</metadata>");
    cr_puts(user_data.fil_f, "</filelists>");
    cr_puts(user_data.oth_f, "</otherdata>");

    cr_close(user_data.pri_f);
    cr_close(user_data.fil_f);
    cr_close(user_data.oth_f);


    // Close db

    cr_destroy_primary_db_statements(user_data.pri_statements);
    cr_destroy_filelists_db_statements(user_data.fil_statements);
    cr_destroy_other_db_statements(user_data.oth_statements);

    cr_close_primary_db(pri_db, NULL);
    cr_close_filelists_db(fil_db, NULL);
    cr_close_other_db(oth_db, NULL);


    // Move files from out_repo into tmp_out_repo

    g_debug("Moving data from %s", out_repo);

    if (g_file_test(out_repo, G_FILE_TEST_EXISTS)) {

        // Delete old metadata
        g_debug("Removing old metadata from %s", out_repo);
        cr_remove_metadata_classic(out_dir, cmd_options->retain_old);

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

            if (g_rename(full_path, new_full_path) == -1) {
                g_critical("Cannot move file %s -> %s", full_path, new_full_path);
            } else {
                g_debug("Moved %s -> %s", full_path, new_full_path);
            }

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


    // Create repomd records for each file

    g_debug("Generating repomd.xml");

    cr_Repomd repomd_obj = cr_new_repomd();

    cr_RepomdRecord pri_xml_rec = cr_new_repomdrecord("repodata/primary.xml.gz");
    cr_RepomdRecord fil_xml_rec = cr_new_repomdrecord("repodata/filelists.xml.gz");
    cr_RepomdRecord oth_xml_rec = cr_new_repomdrecord("repodata/other.xml.gz");
    cr_RepomdRecord pri_db_rec               = NULL;
    cr_RepomdRecord fil_db_rec               = NULL;
    cr_RepomdRecord oth_db_rec               = NULL;
    cr_RepomdRecord groupfile_rec            = NULL;
    cr_RepomdRecord compressed_groupfile_rec = NULL;
    cr_RepomdRecord updateinfo_rec           = NULL;


    // XML

    cr_fill_repomdrecord(out_dir, pri_xml_rec, &(cmd_options->checksum_type));
    cr_fill_repomdrecord(out_dir, fil_xml_rec, &(cmd_options->checksum_type));
    cr_fill_repomdrecord(out_dir, oth_xml_rec, &(cmd_options->checksum_type));


    // Groupfile

    if (groupfile) {
        gchar *groupfile_name;
        groupfile_name = g_strconcat("repodata/",
                                     cr_get_filename(groupfile),
                                     NULL);
        groupfile_rec = cr_new_repomdrecord(groupfile_name);
        compressed_groupfile_rec = cr_new_repomdrecord(groupfile_name);
        cr_process_groupfile_repomdrecord(out_dir,
                                          groupfile_rec,
                                          compressed_groupfile_rec,
                                          &(cmd_options->checksum_type),
                                          groupfile_compression);
        g_free(groupfile_name);
    }


    // Updateinfo

    if (updateinfo) {
        gchar *updateinfo_name;
        updateinfo_name = g_strconcat("repodata/",
                                      cr_get_filename(updateinfo),
                                      NULL);
        updateinfo_rec = cr_new_repomdrecord(updateinfo_name);
        cr_fill_repomdrecord(out_dir, updateinfo_rec, &(cmd_options->checksum_type));
        g_free(updateinfo_name);
    }


    // Sqlite db

    if (!cmd_options->no_database) {
        gchar *pri_db_name = g_strconcat("repodata/primary.sqlite",
                                         sqlite_compression_suffix, NULL);
        gchar *fil_db_name = g_strconcat("repodata/filelists.sqlite",
                                         sqlite_compression_suffix, NULL);
        gchar *oth_db_name = g_strconcat("repodata/other.sqlite",
                                         sqlite_compression_suffix, NULL);

        gchar *tmp_pri_db_path;
        gchar *tmp_fil_db_path;
        gchar *tmp_oth_db_path;


        // Open dbs again (but from the new (final) location)
        // and insert XML checksums

        tmp_pri_db_path = g_strconcat(out_dir, "repodata/primary.sqlite", NULL);
        tmp_fil_db_path = g_strconcat(out_dir, "repodata/filelists.sqlite", NULL);
        tmp_oth_db_path = g_strconcat(out_dir, "repodata/other.sqlite", NULL);

        sqlite3_open(tmp_pri_db_path, &pri_db);
        sqlite3_open(tmp_fil_db_path, &fil_db);
        sqlite3_open(tmp_oth_db_path, &oth_db);

        cr_dbinfo_update(pri_db, pri_xml_rec->checksum, NULL);
        cr_dbinfo_update(fil_db, fil_xml_rec->checksum, NULL);
        cr_dbinfo_update(oth_db, oth_xml_rec->checksum, NULL);

        sqlite3_close(pri_db);
        sqlite3_close(fil_db);
        sqlite3_close(oth_db);


        // Compress dbs

        cr_compress_file(tmp_pri_db_path, NULL, sqlite_compression);
        cr_compress_file(tmp_fil_db_path, NULL, sqlite_compression);
        cr_compress_file(tmp_oth_db_path, NULL, sqlite_compression);

        remove(tmp_pri_db_path);
        remove(tmp_fil_db_path);
        remove(tmp_oth_db_path);

        g_free(tmp_pri_db_path);
        g_free(tmp_fil_db_path);
        g_free(tmp_oth_db_path);


        // Prepare repomd records

        pri_db_rec = cr_new_repomdrecord(pri_db_name);
        fil_db_rec = cr_new_repomdrecord(fil_db_name);
        oth_db_rec = cr_new_repomdrecord(oth_db_name);

        cr_fill_repomdrecord(out_dir, pri_db_rec, &(cmd_options->checksum_type));
        cr_fill_repomdrecord(out_dir, fil_db_rec, &(cmd_options->checksum_type));
        cr_fill_repomdrecord(out_dir, oth_db_rec, &(cmd_options->checksum_type));

        g_free(pri_db_name);
        g_free(fil_db_name);
        g_free(oth_db_name);
    }


    // Add checksums into files names

    if (cmd_options->unique_md_filenames) {
        cr_rename_repomdrecord_file(out_dir, pri_xml_rec);
        cr_rename_repomdrecord_file(out_dir, fil_xml_rec);
        cr_rename_repomdrecord_file(out_dir, oth_xml_rec);
        cr_rename_repomdrecord_file(out_dir, pri_db_rec);
        cr_rename_repomdrecord_file(out_dir, fil_db_rec);
        cr_rename_repomdrecord_file(out_dir, oth_db_rec);
        cr_rename_repomdrecord_file(out_dir, groupfile_rec);
        cr_rename_repomdrecord_file(out_dir, compressed_groupfile_rec);
        cr_rename_repomdrecord_file(out_dir, updateinfo_rec);
    }



    // Gen xml

    cr_repomd_set_record(repomd_obj, pri_xml_rec,    CR_MD_PRIMARY_XML);
    cr_repomd_set_record(repomd_obj, fil_xml_rec,    CR_MD_FILELISTS_XML);
    cr_repomd_set_record(repomd_obj, oth_xml_rec,    CR_MD_OTHER_XML);
    cr_repomd_set_record(repomd_obj, pri_db_rec,     CR_MD_PRIMARY_SQLITE);
    cr_repomd_set_record(repomd_obj, fil_db_rec,     CR_MD_FILELISTS_SQLITE);
    cr_repomd_set_record(repomd_obj, oth_db_rec,     CR_MD_OTHER_SQLITE);
    cr_repomd_set_record(repomd_obj, groupfile_rec,  CR_MD_GROUPFILE);
    cr_repomd_set_record(repomd_obj, compressed_groupfile_rec,
                         CR_MD_COMPRESSED_GROUPFILE);
    cr_repomd_set_record(repomd_obj, updateinfo_rec, CR_MD_UPDATEINFO);

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

    char *repomd_xml = cr_generate_repomd_xml(repomd_obj);

    cr_free_repomd(repomd_obj);

    // Write repomd.xml

    gchar *repomd_path = g_strconcat(out_repo, "repomd.xml", NULL);
    FILE *frepomd = fopen(repomd_path, "w");
    if (!frepomd || !repomd_xml) {
        g_critical("Generate of repomd.xml failed");
        return 1;
    }
    fputs(repomd_xml, frepomd);
    fclose(frepomd);
    g_free(repomd_xml);
    g_free(repomd_path);


    // Clean up

    g_debug("Memory cleanup");

    if (old_metadata)
        cr_destroy_metadata(old_metadata);

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
    cr_package_parser_shutdown();

    g_debug("All done");
    return 0;
}
