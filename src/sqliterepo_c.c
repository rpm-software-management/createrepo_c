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
#include "cleanup.h"
#include "version.h"
#include "compression_wrapper.h"
#include "misc.h"
#include "locate_metadata.h"
#include "load_metadata.h"
#include "package.h"
#include "repomd.h"
#include "sqlite.h"
#include "xml_file.h"
#include "modifyrepo_shared.h"

/**
 * Command line options
 */
typedef struct {

    /* Items filled by cmd option parser */

    gboolean version;           /*!< print program version */
    gboolean quiet;             /*!< quiet mode */
    gboolean verbose;           /*!< verbose mode */
    gboolean force;             /*!< overwrite existing DBs */
    gboolean xz_compression;    /*!< use xz for DBs compression */
    char *compress_type;        /*!< which compression type to use */
    gboolean local_sqlite;      /*!< gen sqlite locally into a directory
                                     temporary files. (For situations when
                                     sqlite has a trouble to gen DBs
                                     on NFS mounts.)*/

    /* Items filled by check_sqliterepo_arguments() */

    cr_CompressionType compression_type;/*!< compression type */

} SqliterepoCmdOptions;

static SqliterepoCmdOptions *
sqliterepocmdoptions_new(void)
{
    SqliterepoCmdOptions *options;

    options = g_new(SqliterepoCmdOptions, 1);
    options->version = FALSE;
    options->quiet = FALSE;
    options->verbose = FALSE;
    options->force = FALSE;
    options->xz_compression = FALSE;
    options->compress_type = NULL;
    options->local_sqlite = FALSE;
    options->compression_type = CR_CW_BZ2_COMPRESSION;

    return options;
}

static void
sqliterepocmdoptions_free(SqliterepoCmdOptions *options)
{
    g_free(options->compress_type);
    g_free(options);
}

CR_DEFINE_CLEANUP_FUNCTION0(SqliterepoCmdOptions*, cr_local_sqliterepocmdoptions_free, sqliterepocmdoptions_free)
#define _cleanup_sqliterepocmdoptions_free_ __attribute__ ((cleanup(cr_local_sqliterepocmdoptions_free)))

/**
 * Parse commandline arguments for sqliterepo utility
 */
static gboolean
parse_sqliterepo_arguments(int *argc,
                           char ***argv,
                           SqliterepoCmdOptions *options,
                           GError **err)
{
    const GOptionEntry cmd_entries[] = {

        { "version", 'V', 0, G_OPTION_ARG_NONE, &(options->version),
          "Show program's version number and exit.", NULL},
        { "quiet", 'q', 0, G_OPTION_ARG_NONE, &(options->quiet),
          "Run quietly.", NULL },
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &(options->verbose),
          "Run verbosely.", NULL },
        { "force", 'f', 0, G_OPTION_ARG_NONE, &(options->force),
          "Overwirte existing DBs.", NULL },
        { "xz", '\0', 0, G_OPTION_ARG_NONE, &(options->xz_compression),
          "Use xz for repodata compression.", NULL },
        { "compress-type", '\0', 0, G_OPTION_ARG_STRING, &(options->compress_type),
          "Which compression type to use.", "<compress_type>" },
        { "local-sqlite", '\0', 0, G_OPTION_ARG_NONE, &(options->local_sqlite),
          "Gen sqlite DBs locally (into a directory for temporary files). "
          "Sometimes, sqlite has a trouble to gen DBs on a NFS mount, "
          "use this option in such cases. "
          "This option could lead to a higher memory consumption "
          "if TMPDIR is set to /tmp or not set at all, because then the /tmp is "
          "used and /tmp dir is often a ramdisk.", NULL },
        { NULL },
    };

    // Parse cmd arguments
    GOptionContext *context;
    context = g_option_context_new("<repo_directory>");
    g_option_context_set_summary(context, "Generate sqlite DBs from XML repodata.");
    g_option_context_add_main_entries(context, cmd_entries, NULL);
    gboolean ret = g_option_context_parse(context, argc, argv, err);
    g_option_context_free(context);
    return ret;
}

/**
 * Check parsed arguments and fill some other attributes
 * of option struct accordingly.
 */
static gboolean
check_arguments(SqliterepoCmdOptions *options, GError **err)
{
    // --xz
    if (options->xz_compression)
        options->compression_type = CR_CW_XZ_COMPRESSION;

    // --compress-type
    if (options->compress_type) {
        options->compression_type = cr_compression_type(options->compress_type);
        if (options->compression_type == CR_CW_UNKNOWN_COMPRESSION) {
            g_set_error(err, CREATEREPO_C_ERROR, CRE_ERROR,
                        "Unknown compression type \"%s\"", options->compress_type);
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
generate_sqlite_from_xml(const gchar *path)
{
    _cleanup_free_ gchar *in_dir       = NULL;  // path/to/repo/
    _cleanup_free_ gchar *in_repo      = NULL;  // path/to/repo/repodata/
    _cleanup_free_ gchar *out_dir      = NULL;  // path/to/out_repo/
    _cleanup_free_ gchar *out_repo     = NULL;  // path/to/out_repo/repodata/
    _cleanup_free_ gchar *tmp_out_repo = NULL;  // usually path/to/out_repo/.repodata/
    _cleanup_free_ gchar *lock_dir     = NULL;  // path/to/out_repo/.repodata/

    // Check if input dir exists
    in_dir = cr_normalize_dir_path(path);
    if (!g_file_test(in_dir, G_FILE_TEST_IS_DIR)) {
        g_printerr("Directory %s must exists\n", in_dir);
        exit(EXIT_FAILURE);
    }

    // Set other paths
    in_repo         = g_build_filename(in_dir, "repodata/", NULL);
    out_dir         = g_strdup(in_dir);
    out_repo        = g_strdup(in_repo);
    lock_dir        = g_build_filename(out_dir, ".repodata/", NULL);
    tmp_out_repo    = g_build_filename(out_dir, ".repodata/", NULL);

    // Lock repodata (createrepo_shared)
}

/**
 * Main
 */
int
main(int argc, char **argv)
{
    gboolean ret = TRUE;
    _cleanup_sqliterepocmdoptions_free_ SqliterepoCmdOptions *options = NULL;
    _cleanup_error_free_ GError *tmp_err = NULL;

    // Parse arguments
    options = sqliterepocmdoptions_new();
    ret = parse_sqliterepo_arguments(&argc, &argv, options, &tmp_err);
    if (!ret) {
        g_printerr("%s\n", tmp_err->message);
        exit(EXIT_FAILURE);
    }

    // Set logging
    g_log_set_default_handler(cr_log_fn, NULL);
    if (options->verbose) {
        // Verbose mode
        GLogLevelFlags levels = G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_WARNING;
        g_log_set_handler(NULL, levels, cr_log_fn, NULL);
        g_log_set_handler("C_CREATEREPOLIB", levels, cr_log_fn, NULL);
    } else {
        // Standard mode
        GLogLevelFlags levels = G_LOG_LEVEL_DEBUG;
        g_log_set_handler(NULL, levels, cr_null_log_fn, NULL);
        g_log_set_handler("C_CREATEREPOLIB", levels, cr_null_log_fn, NULL);
    }

    // Print version if required
    if (options->version) {
        printf("Version: %d.%d.%d\n", CR_VERSION_MAJOR,
                                      CR_VERSION_MINOR,
                                      CR_VERSION_PATCH);
        exit(EXIT_SUCCESS);
    }

    // Check arguments
    ret = check_arguments(options, &tmp_err);
    if (!ret) {
        g_printerr("%s\n", tmp_err->message);
        exit(EXIT_FAILURE);
    }

    if (argc != 2) {
        g_printerr("Must specify exactly one repo directory to work on\n");
        exit(EXIT_FAILURE);
    }

    // TODO: Maybe in future
    //g_thread_init(NULL); // Initialize threading

    ret = generate_sqlite_from_xml(argv[1]);

    exit(EXIT_SUCCESS);
}
