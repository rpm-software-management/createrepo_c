/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
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


#define VERSION         "0.1"
#define G_LOG_DOMAIN    ((gchar*) 0)



struct UserData {
    CW_FILE *pri_f;
    CW_FILE *fil_f;
    CW_FILE *oth_f;
    int changelog_limit;
    char *location_base;
    int repodir_name_len;
    char *checksum_type_str;
    ChecksumType checksum_type;
    gboolean quiet;
    gboolean verbose;
    gboolean skip_symlinks;
    int package_count;

    // Update stuff
    gboolean skip_stat;
    GHashTable *old_metadata;
};


struct PoolTask {
    char* full_path;
    char* filename;
    char* path;
};



void black_hole_log_function (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
    UNUSED(log_domain);
    UNUSED(log_level);
    UNUSED(message);
    UNUSED(user_data);
    return;
}


// Global variables used by signal handler
char *tmp_repodata_path = NULL;


// Signal handler
void sigint_catcher(int sig)
{
    UNUSED(sig);
    g_message("SIGINT catched: Terminating...");
    if (tmp_repodata_path) {
        remove_dir(tmp_repodata_path);
    }
    exit(1);
}



int allowed_file(const gchar *filename, struct CmdOptions *options)
{
    // Check file against exclude glob masks
    if (options->exclude_masks) {
        int str_len = strlen(filename);
        gchar *reversed_filename = g_utf8_strreverse(filename, str_len);

        GSList *element;
        for (element=options->exclude_masks; element; element=g_slist_next(element)) {
            if (g_pattern_match((GPatternSpec *) element->data, str_len, filename, reversed_filename)) {
                g_free(reversed_filename);
                g_debug("Exclude masks hit - skipping: %s", filename);
                return FALSE;
            }
            g_free(reversed_filename);
        }
    }
    return TRUE;
}



#define LOCK_PRI        0
#define LOCK_FIL        1
#define LOCK_OTH        2

G_LOCK_DEFINE (LOCK_PRI);
G_LOCK_DEFINE (LOCK_FIL);
G_LOCK_DEFINE (LOCK_OTH);

void dumper_thread(gpointer data, gpointer user_data) {
    struct UserData *udata = (struct UserData *) user_data;

//    if (udata->verbose) {
//        printf("Processing: %s\n", data);
//    }

    struct PoolTask *task = (struct PoolTask *) data;

    // get location_href without leading part of path (path to repo) including '/' char
    char *location_href = (gchar *) task->full_path + udata->repodir_name_len;

    char *location_base = udata->location_base;

    // Get stat info about file
    struct stat stat_buf;
    if (!(udata->old_metadata) || !(udata->skip_stat)) {
        if (stat(task->full_path, &stat_buf) == -1) {
            g_critical("Stat() on %s: %s", task->full_path, strerror(errno));
            return;
        }
    }

    struct XmlStruct res;

    // Update stuff
    gboolean modified_primary_xml_used = FALSE;
    gchar *modified_primary_xml = NULL;

    gboolean old_used = FALSE;
    Package *md;

    if (udata->old_metadata) {
        // We have old metadata
        md = (Package *) g_hash_table_lookup (udata->old_metadata, task->filename);
        if (md) {
            // CACHE HIT!

            g_debug("CACHE HIT %s", task->filename);

            if (udata->skip_stat) {
                old_used = TRUE;
            } else if (stat_buf.st_mtime == md->time_file
                       && stat_buf.st_size == md->size_package
                       && !strcmp(udata->checksum_type_str, md->checksum_type))
            {
                old_used = TRUE;
            } else {
                g_debug("%s metadata are obsolete -> generating new", task->filename);
            }
        }

        if (old_used) {
            // We have usable old data, but we have to set locations (href and base)
            md->location_href = location_href;
            md->location_base = location_base;
        }
    }

    if (!old_used) {
        res = xml_from_package_file(task->full_path, udata->checksum_type, location_href,
                                    udata->location_base, udata->changelog_limit, NULL);
    } else {
        res = xml_dump(md);
    }

    // Write primary data
    G_LOCK(LOCK_PRI);
    cw_puts(udata->pri_f, (const char *) res.primary);
    G_UNLOCK(LOCK_PRI);

    // Write fielists data
    G_LOCK(LOCK_FIL);
    cw_puts(udata->fil_f, (const char *) res.filelists);
    G_UNLOCK(LOCK_FIL);

    // Write other data
    G_LOCK(LOCK_OTH);
    cw_puts(udata->oth_f, (const char *) res.other);
    G_UNLOCK(LOCK_OTH);

    // Clean up
    free(res.primary);
    free(res.filelists);
    free(res.other);

    if (modified_primary_xml_used) {
        g_free(modified_primary_xml);
    }

    g_free(task->full_path);
    g_free(task->filename);
    g_free(task->path);
    g_free(task);

    return;
}



int main(int argc, char **argv) {


    // Arguments parsing

    struct CmdOptions *cmd_options;
    cmd_options = parse_arguments(&argc, &argv);
    if (!cmd_options) {
        exit(1);
    }


    // Arguments pre-check

    if (cmd_options->version) {
        puts("Version: "VERSION);
        free_options(cmd_options);
        exit(0);
    } else if (argc != 2) {
        fprintf(stderr, "Must specify exactly one directory to index.\n");
        fprintf(stderr, "Usage: %s [options] <directory_to_index>\n\n", get_filename(argv[0]));
        free_options(cmd_options);
        exit(1);
    }


    // Dirs

    gchar *in_dir   = NULL;  // path/to/repo/
    gchar *in_repo  = NULL;  // path/to/repo/repodata/
    gchar *out_dir  = NULL;  // path/to/out_repo/
    gchar *out_repo = NULL;  // path/to/out_repo/repodata/
    gchar *tmp_out_repo = NULL; // path/to/out_repo/.repodata/

    in_dir = normalize_dir_path(argv[1]);
    cmd_options->input_dir = g_strdup(in_dir);


    // Check parsed arguments

    if (!check_arguments(cmd_options)) {
        g_free(in_dir);
        free_options(cmd_options);
        exit(1);
    }


    // Set logging stuff

    if (cmd_options->quiet) {
        // Quiet mode
        GLogLevelFlags levels = G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_WARNING;
        g_log_set_handler(NULL, levels, black_hole_log_function, NULL);
        g_log_set_handler("C_CREATEREPOLIB", levels, black_hole_log_function, NULL);
    } else if (cmd_options->verbose) {
        // Verbose mode
        ;
    } else {
        // Standard mode
        GLogLevelFlags levels = G_LOG_LEVEL_DEBUG;
        g_log_set_handler(NULL, levels, black_hole_log_function, NULL);
        g_log_set_handler("C_CREATEREPOLIB", levels, black_hole_log_function, NULL);
    }


    // Set paths of input and output repos

    in_repo = g_strconcat(in_dir, "repodata/", NULL);

    if (cmd_options->outputdir) {
        out_dir = normalize_dir_path(cmd_options->outputdir);
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
            g_critical("Temporary repodata directory: %s already exists! (Another createrepo process is running?)", tmp_out_repo);
        } else {
            g_critical("Error while creating temporary repodata directory %s: %s", tmp_out_repo, strerror(errno));
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



    // Copy groupfile

    gchar *groupfile = NULL;
    if (cmd_options->groupfile_fullpath) {
        groupfile = g_strconcat(tmp_out_repo, get_filename(cmd_options->groupfile_fullpath), NULL);
        g_debug("Copy groupfile %s -> %s", cmd_options->groupfile_fullpath, groupfile);
        if (better_copy_file(cmd_options->groupfile_fullpath, groupfile) != CR_COPY_OK) {
            g_critical("Error while copy %s -> %s", cmd_options->groupfile_fullpath, groupfile);
        }
    }


    // Load old metadata if --update

    GHashTable *old_metadata = NULL;
    if (cmd_options->update) {

        // Load local repodata
        old_metadata = new_metadata_hashtable();
        int ret = locate_and_load_xml_metadata(old_metadata, in_dir, HT_KEY_FILENAME);
        if (ret == LOAD_METADATA_OK) {
            g_debug("Old metadata from: %s - loaded", in_dir);
        } else {
            g_warning("Old metadata from %s - loading failed", in_dir);
        }

        // Load repodata from --update-md-path
        GSList *element;
        for (element = cmd_options->l_update_md_paths; element; element = g_slist_next(element)) {
            char *path = (char *) element->data;
            g_message("Loading metadata from: %s", path);
            int ret = locate_and_load_xml_metadata(old_metadata, path, HT_KEY_FILENAME);
            if (ret == LOAD_METADATA_OK) {
                g_debug("Old metadata from md-path %s - loaded", path);
            } else {
                g_warning("Old metadata from md-path %s - loading failed", path);
            }
        }

        g_message("Loaded information about %d packages", g_hash_table_size(old_metadata));
    }


    // Create and open new compressed files

    CW_FILE *pri_cw_file;
    CW_FILE *fil_cw_file;
    CW_FILE *oth_cw_file;

    g_message("Temporary output repo path: %s", tmp_out_repo);
    g_debug("Opening/Creating .xml.gz files");

    gchar *pri_xml_filename = g_strconcat(tmp_out_repo, "/primary.xml.gz", NULL);
    gchar *fil_xml_filename = g_strconcat(tmp_out_repo, "/filelists.xml.gz", NULL);
    gchar *oth_xml_filename = g_strconcat(tmp_out_repo, "/other.xml.gz", NULL);

    if ((pri_cw_file = cw_open(pri_xml_filename, CW_MODE_WRITE, GZ_COMPRESSION)) == NULL) {
        g_critical("Cannot open file: %s", pri_xml_filename);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        exit(1);
    }

    if ((fil_cw_file = cw_open(fil_xml_filename, CW_MODE_WRITE, GZ_COMPRESSION)) == NULL) {
        g_critical("Cannot open file: %s", fil_xml_filename);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        cw_close(pri_cw_file);
        exit(1);
    }

    if ((oth_cw_file = cw_open(oth_xml_filename, CW_MODE_WRITE, GZ_COMPRESSION)) == NULL) {
        g_critical("Cannot open file: %s", oth_xml_filename);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        cw_close(fil_cw_file);
        cw_close(pri_cw_file);
        exit(1);
    }


    // Init package parser

    init_package_parser();


    // Thread pool - User data initialization

    struct UserData user_data;
    user_data.pri_f             = pri_cw_file;
    user_data.fil_f             = fil_cw_file;
    user_data.oth_f             = oth_cw_file;
    user_data.changelog_limit   = cmd_options->changelog_limit;
    user_data.location_base     = cmd_options->location_base;
    user_data.checksum_type_str = cmd_options->checksum;
    user_data.checksum_type     = cmd_options->checksum_type;
    user_data.quiet             = cmd_options->quiet;
    user_data.verbose           = cmd_options->verbose;
    user_data.skip_symlinks     = cmd_options->skip_symlinks;
    user_data.skip_stat         = cmd_options->skip_stat;
    user_data.old_metadata      = old_metadata;
    user_data.repodir_name_len  = strlen(in_dir);

    g_debug("Thread pool user data ready");


    // Thread pool - Creation

    g_thread_init(NULL);
    GThreadPool *pool = g_thread_pool_new(dumper_thread, &user_data, 0, TRUE, NULL);

    g_debug("Thread pool ready");


    // Recursive walk

    int package_count = 0;

    if (!(cmd_options->include_pkgs)) {
        // --pkglist (or --includepkg) is not supplied -> do dir walk

        g_message("Directory walk started");

        GStringChunk *sub_dirs_chunk = g_string_chunk_new(1024);
        GQueue *sub_dirs = g_queue_new();
        gchar *input_dir_stripped = g_string_chunk_insert_len(sub_dirs_chunk, in_dir, strlen(in_dir)-1);
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
                        gchar *sub_dir_in_chunk = g_string_chunk_insert (sub_dirs_chunk, full_path);
                        g_queue_push_head(sub_dirs, sub_dir_in_chunk);
                        g_debug("Dir to scan: %s", sub_dir_in_chunk);
                    }
                    g_free(full_path);
                    continue;
                }

                // Skip symbolic links if --skip-symlinks arg is used
                if (cmd_options->skip_symlinks && g_file_test(full_path, G_FILE_TEST_IS_SYMLINK)) {
                    g_debug("Skipped symlink: %s", full_path);
                    g_free(full_path);
                    continue;
                }

                // Check filename against exclude glob masks
                if (allowed_file(filename, cmd_options)) {
                    // FINALLY! Add file into pool
                    g_debug("Adding pkg: %s", full_path);
                    struct PoolTask *task = g_malloc(sizeof(struct PoolTask));
                    task->full_path = full_path;
                    task->filename = g_strdup(filename);
                    task->path = g_strdup(dirname);  // TODO: One common path for all tasks with the same path??
                    g_thread_pool_push(pool, task, NULL);
                    package_count++;
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

        GSList *element;
        for (element=cmd_options->include_pkgs; element; element=g_slist_next(element)) {
            gchar *relative_path = (gchar *) element->data;   // path from pkglist e.g. packages/i386/foobar.rpm
            gchar *full_path = g_strconcat(in_dir, relative_path, NULL);   // /path/to/in_repo/packages/i386/foobar.rpm
            gchar *dirname;  // packages/i386/
            gchar *filename;  // foobar.rpm

            // Get index of last '/'
            int rel_path_len = strlen(relative_path);
            int x = rel_path_len;
            for (; x > 0; x--) {
                if (relative_path[x] == '/') {
                    break;
                }
            }

            if (!x) {
                // There was no '/' in path
                filename = relative_path;
            } else {
                filename = relative_path + x + 1;
            }
            dirname  = strndup(relative_path, x);

            if (allowed_file(filename, cmd_options)) {
                // Check filename against exclude glob masks
                g_debug("Adding pkg: %s", full_path);
                struct PoolTask *task = g_malloc(sizeof(struct PoolTask));
                task->full_path = full_path;
                task->filename = g_strdup(filename);
                task->path = dirname;
                g_thread_pool_push(pool, task, NULL);
                package_count++;
            }
        }
    }

    g_debug("Package count: %d", package_count);
    g_message("Directory walk done");


    // Write XML header

    g_debug("Writing xml headers");

    cw_printf(user_data.pri_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<metadata xmlns=\""XML_COMMON_NS"\" xmlns:rpm=\""XML_RPM_NS"\" packages=\"%d\">\n", package_count);
    cw_printf(user_data.fil_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<filelists xmlns=\""XML_FILELISTS_NS"\" packages=\"%d\">\n", package_count);
    cw_printf(user_data.oth_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<otherdata xmlns=\""XML_OTHER_NS"\" packages=\"%d\">\n", package_count);


    // Start pool

    user_data.package_count = package_count;
    g_thread_pool_set_max_threads(pool, cmd_options->workers, NULL);
    g_message("Pool started (with %d workers)", cmd_options->workers);


    // Wait until pool is finished

    g_thread_pool_free(pool, FALSE, TRUE);
    g_message("Pool finished");

    cw_puts(user_data.pri_f, "</metadata>");
    cw_puts(user_data.fil_f, "</filelists>");
    cw_puts(user_data.oth_f, "</otherdata>");

    cw_close(user_data.pri_f);
    cw_close(user_data.fil_f);
    cw_close(user_data.oth_f);


    // Move files from out_repo into tmp_out_repo

    g_debug("Moving data from %s", out_repo);
    if (g_file_test(out_repo, G_FILE_TEST_EXISTS)) {

        // Delete old metadata
        g_debug("Removing old metadata from %s", out_repo);
        remove_metadata(out_dir);

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


    // Create repomd.xml

    g_debug("Generating repomd.xml");

    gchar *pri_xml_name = g_strconcat("repodata/", "primary.xml.gz", NULL);
    gchar *fil_xml_name = g_strconcat("repodata/", "filelists.xml.gz", NULL);
    gchar *oth_xml_name = g_strconcat("repodata/", "other.xml.gz", NULL);
    gchar *groupfile_name = NULL;
    if (groupfile) {
        groupfile_name = g_strconcat("repodata/", get_filename(groupfile), NULL);
    }

    struct repomdResult *repomd_res = xml_repomd(out_dir, cmd_options->unique_md_filenames,
                                                 pri_xml_name, fil_xml_name, oth_xml_name,
                                                 NULL, NULL, NULL, groupfile_name, NULL,
                                                 &(cmd_options->checksum_type));
    gchar *repomd_path = g_strconcat(out_repo, "repomd.xml", NULL);

    FILE *frepomd = fopen(repomd_path, "w");
    if (frepomd && repomd_res->repomd_xml) {
        fputs(repomd_res->repomd_xml, frepomd);
        fclose(frepomd);
    } else {
        g_critical("Generate of repomd.xml failed");
    }

    free_repomdresult(repomd_res);
    g_free(repomd_path);
    g_free(pri_xml_name);
    g_free(fil_xml_name);
    g_free(oth_xml_name);
    g_free(groupfile_name);


    // Clean up

    g_debug("Memory cleanup");

    if (old_metadata) {
        destroy_metadata_hashtable(old_metadata);
    }

    g_free(in_repo);
    g_free(out_repo);
    tmp_repodata_path = NULL;
    g_free(tmp_out_repo);
    g_free(in_dir);
    g_free(out_dir);
    g_free(pri_xml_filename);
    g_free(fil_xml_filename);
    g_free(oth_xml_filename);
    g_free(groupfile);

    free_options(cmd_options);
    free_package_parser();

    g_debug("All done");
    return 0;
}
