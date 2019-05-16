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
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "error.h"
#include "version.h"
#include "compression_wrapper.h"
#include "createrepo_shared.h"
#include "misc.h"
#include "locate_metadata.h"
#include "load_metadata.h"
#include "package.h"
#include "repomd.h"
#include "sqlite.h"
#include "xml_file.h"
#include "modifyrepo_shared.h"

#define ERR_DOMAIN      CREATEREPO_C_ERROR

typedef struct {

    gboolean version;
    gchar *mdtype;
    gchar *remove;
    gboolean compress;
    gboolean no_compress;
    gchar *compress_type;
    gchar *checksum;
    gboolean unique_md_filenames;
    gboolean simple_md_filenames;
    gboolean verbose;
    gchar *batchfile;
    gchar *new_name;
    gboolean zck;
    gchar *zck_dict_dir;

} RawCmdOptions;

static gboolean
parse_arguments(int *argc, char ***argv, RawCmdOptions *options, GError **err)
{
    const GOptionEntry cmd_entries[] = {

        { "version", 0, 0, G_OPTION_ARG_NONE, &(options->version),
          "Show program's version number and exit.", NULL },
        { "mdtype", 0, 0, G_OPTION_ARG_STRING, &(options->mdtype),
          "Specific datatype of the metadata, will be derived from "
          "the filename if not specified.", "MDTYPE" },
        { "remove", 0, 0, G_OPTION_ARG_STRING, &(options->remove),
          "Remove specified file from repodata.", NULL },
        { "compress", 0, 0, G_OPTION_ARG_NONE, &(options->compress),
          "Compress the new repodata before adding it to the repo. "
          "(default)", NULL },
        { "no-compress", 0, 0, G_OPTION_ARG_NONE, &(options->no_compress),
          "Do not compress the new repodata before adding it to the repo.",
          NULL },
        { "compress-type", 0, 0, G_OPTION_ARG_STRING, &(options->compress_type),
          "Compression format to use.", "COMPRESS_TYPE" },
        { "checksum", 's', 0, G_OPTION_ARG_STRING, &(options->checksum),
          "Specify the checksum type to use. (default: sha256)", "SUMTYPE" },
        { "unique-md-filenames", 0, 0, G_OPTION_ARG_NONE,
          &(options->unique_md_filenames),
          "Include the file's checksum in the filename, helps with proxies. "
          "(default)", NULL },
        { "simple-md-filenames", 0, 0, G_OPTION_ARG_NONE,
          &(options->simple_md_filenames),
          "Do not include the file's checksum in the filename.", NULL },
        { "verbose", 0, 0, G_OPTION_ARG_NONE, &(options->verbose),
          "Verbose output.", NULL},
        { "batchfile", 'f', 0, G_OPTION_ARG_STRING, &(options->batchfile),
          "Batch file.", "BATCHFILE" },
        { "new-name", 0, 0, G_OPTION_ARG_STRING, &(options->new_name),
          "New filename for the file", "NEWFILENAME"},
#ifdef WITH_ZCHUNK
        { "zck", 0, 0, G_OPTION_ARG_NONE, &(options->zck),
          "Generate zchunk files as well as the standard repodata.", NULL },
        { "zck-dict-dir", 0, 0, G_OPTION_ARG_FILENAME, &(options->zck_dict_dir),
          "Directory containing compression dictionaries for use by zchunk", "ZCK_DICT_DIR" },
#endif
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL },

    };

    // Frstly, set default values
    options->version = FALSE;
    options->mdtype = NULL;
    options->remove = NULL;
    options->compress = TRUE;
    options->no_compress = FALSE;
    options->compress_type = NULL;
    options->checksum = NULL;
    options->unique_md_filenames = TRUE;
    options->simple_md_filenames = FALSE;
    options->verbose = FALSE;
    options->batchfile = NULL;
    options->new_name = NULL;
    options->zck = FALSE;
    options->zck_dict_dir = NULL;

    GOptionContext *context;
    context = g_option_context_new("<input metadata> <output repodata>\n"
            "  modifyrepo_c --remove <metadata type> <output repodata>\n"
            "  modifyrepo_c [OPTION...] --batchfile <batch file> <output repodata>");
    g_option_context_set_summary(context, "Modify a repository's repomd.xml");
    g_option_context_add_main_entries(context, cmd_entries, NULL);
    gboolean ret = g_option_context_parse(context, argc, argv, err);
    g_option_context_free(context);
    return ret;
}

static gboolean
check_arguments(RawCmdOptions *options, GError **err)
{
    // --no-compress
    if (options->no_compress) {
        options->compress = FALSE;
        if (options->compress_type) {
            g_warning("Use --compress-type simultaneously with --no-compress "
                      "doesn't make a sense");
        }
    }

    // --compress-type
    if (options->compress_type
        && cr_compression_type(options->compress_type) == \
           CR_CW_UNKNOWN_COMPRESSION)
    {
        g_set_error(err, ERR_DOMAIN, CRE_ERROR,
                    "Unknown compression type \"%s\"", options->compress_type);
        return FALSE;
    }

    // -s/--checksum
    if (options->checksum
        && cr_checksum_type(options->checksum) == CR_CHECKSUM_UNKNOWN)
    {
        g_set_error(err, ERR_DOMAIN, CRE_ERROR,
                    "Unknown checksum type \"%s\"", options->checksum);
        return FALSE;
    }

    // --unique_md_filenames && --simple_md_filenames
    if (options->simple_md_filenames) {
        options->unique_md_filenames = FALSE;
    }

    // -f/--batchfile
    if (options->batchfile
        && !g_file_test(options->batchfile, G_FILE_TEST_IS_REGULAR)) {
        g_set_error(err, ERR_DOMAIN, CRE_ERROR,
                    "File \"%s\" doesn't exist", options->batchfile);
        return FALSE;
    }

    // Zchunk options
    if (options->zck_dict_dir && !options->zck) {
        g_set_error(err, ERR_DOMAIN, CRE_ERROR,
                    "Cannot use --zck-dict-dir without setting --zck");
        return FALSE;
    }
    if (options->zck_dict_dir)
        options->zck_dict_dir = cr_normalize_dir_path(options->zck_dict_dir);

    return TRUE;
}

static void
print_usage(void)
{
    g_printerr(
        "Usage: modifyrepo_c [options] <input metadata> <output repodata>\n"
        "Usage: modifyrepo_c --remove <metadata type> <output repodata>\n"
        "Usage: modifyrepo_c [options] --batchfile "
        "<batch file> <output repodata>\n");
}

static gboolean
cmd_options_to_task(GSList **modifyrepotasks,
                    RawCmdOptions *options,
                    gchar *metadatapath,
                    GError **err)
{
    assert(modifyrepotasks);
    assert(!err || *err == NULL);

    if (!options)
        return TRUE;

    //assert(metadatapath || options->remove);

    if (options->remove)
        g_debug("Preparing remove-task for: %s", options->remove);
    else
        g_debug("Preparing task for: %s", metadatapath);

    if (metadatapath && !g_file_test(metadatapath, G_FILE_TEST_IS_REGULAR)) {
        g_set_error(err, ERR_DOMAIN, CRE_ERROR,
                    "File \"%s\" is not regular file or doesn't exists",
                    metadatapath);
        return FALSE;
    }

    if (options->remove)
        metadatapath = options->remove;

    cr_ModifyRepoTask *task = cr_modifyrepotask_new();
    task->path = cr_safe_string_chunk_insert_null(task->chunk, metadatapath);
    task->type = cr_safe_string_chunk_insert_null(task->chunk, options->mdtype);
    task->remove = (options->remove) ? TRUE : FALSE;
    task->compress = options->compress;
    task->compress_type = cr_compression_type(options->compress_type);
    task->unique_md_filenames = options->unique_md_filenames;
    task->checksum_type = cr_checksum_type(options->checksum);
    task->new_name = cr_safe_string_chunk_insert_null(task->chunk,
                                                      options->new_name);
    task->zck = options->zck;
    task->zck_dict_dir = options->zck_dict_dir;

    *modifyrepotasks = g_slist_append(*modifyrepotasks, task);

    g_debug("Task: [path: %s, type: %s, remove: %d, compress: %d, "
            "compress_type: %d (%s), unique_md_filenames: %d, "
            "checksum_type: %d (%s), new_name: %s]",
            task->path, task->type, task->remove, task->compress,
            task->compress_type, cr_compression_suffix(task->compress_type),
            task->unique_md_filenames, task->checksum_type,
            cr_checksum_name_str(task->checksum_type), task->new_name);

    return TRUE;
}

int
main(int argc, char **argv)
{
    gboolean ret = TRUE;
    RawCmdOptions options;
    GError *err = NULL;

    // Parse arguments

    parse_arguments(&argc, &argv, &options, &err);
    if (err) {
        g_printerr("%s\n", err->message);
        print_usage();
        g_error_free(err);
        exit(EXIT_FAILURE);
    }

    // Set logging

    cr_setup_logging(FALSE, options.verbose);


    // Print version if required

    if (options.version) {
        printf("Version: %s\n", cr_version_string_with_features());
        exit(EXIT_SUCCESS);
    }

    // Check arguments

    check_arguments(&options, &err);
    if (err) {
        g_printerr("%s\n", err->message);
        print_usage();
        g_error_free(err);
        exit(EXIT_FAILURE);
    }

    // Emit debug message with version

    g_debug("Version: %s", cr_version_string_with_features());

    // Prepare list of tasks to do

    gchar *repodatadir = NULL;
    GSList *modifyrepotasks = NULL;

    if (!options.batchfile && !options.remove && argc == 3) {
        // three arguments (prog, metadata, repodata_dir)
        repodatadir = argv[2];
        ret = cmd_options_to_task(&modifyrepotasks,
                                  &options,
                                  argv[1],
                                  &err);
    } else if (options.batchfile && argc == 2) {
        // two arguments (prog, repodata_dir)
        repodatadir = argv[1];
        ret = cr_modifyrepo_parse_batchfile(options.batchfile,
                                            &modifyrepotasks,
                                            &err);
    } else if (!options.batchfile && options.remove && argc == 2) {
        // two arguments (prog, repodata_dir)
        repodatadir = argv[1];
        ret = cmd_options_to_task(&modifyrepotasks,
                                  &options,
                                  NULL,
                                  &err);
    } else {
        // Bad arguments
        print_usage();
        exit(EXIT_FAILURE);
    }

    if (!ret) {
        g_printerr("%s\n", err->message);
        g_error_free(err);
        exit(EXIT_FAILURE);
    }

    // Process the tasks

    ret = cr_modifyrepo(modifyrepotasks, repodatadir, &err);
    cr_slist_free_full(modifyrepotasks, (GDestroyNotify)cr_modifyrepotask_free);

    if (!ret) {
        g_printerr("%s\n", err->message);
        g_error_free(err);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
