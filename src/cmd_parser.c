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

#include <string.h>
#include "cmd_parser.h"
#include "compression_wrapper.h"
#include "misc.h"


#define DEFAULT_CHANGELOG_LIMIT         10
#define DEFAULT_CHECKSUM                "sha256"
#define DEFAULT_WORKERS                 5
#define DEFAULT_UNIQUE_MD_FILENAMES     TRUE


struct CmdOptions _cmd_options = {
        .changelog_limit     = DEFAULT_CHANGELOG_LIMIT,
        .checksum            = NULL,
        .workers             = DEFAULT_WORKERS,
        .unique_md_filenames = DEFAULT_UNIQUE_MD_FILENAMES,
        .checksum_type       = CR_CHECKSUM_SHA256,
        .compression_type    = CR_CW_UNKNOWN_COMPRESSION
    };



// Command line params

static GOptionEntry cmd_entries[] =
{
    { "baseurl", 'u', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.location_base),
      "Optional base URL location for all files.", "<URL>" },
    { "outputdir", 'o', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.outputdir),
      "Optional output directory.", "<URL>" },
    { "excludes", 'x', 0, G_OPTION_ARG_FILENAME_ARRAY, &(_cmd_options.excludes),
      "File globs to exclude, can be specified multiple times.", "<packages>" },
    { "pkglist", 'i', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.pkglist),
      "Specify a text file which contains the complete list of files to include"
      " in the repository from the set found in the directory. File format is"
      " one package per line, no wildcards or globs.", "<filename>" },
    { "includepkg", 'n', 0, G_OPTION_ARG_FILENAME_ARRAY, &(_cmd_options.includepkg),
      "Specify pkgs to include on the command line. Takes urls as well as local paths.",
      "<packages>" },
    { "groupfile", 'g', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.groupfile),
      "Path to groupfile to include in metadata.",
      "GROUPFILE" },
    { "quiet", 'q', 0, G_OPTION_ARG_NONE, &(_cmd_options.quiet),
      "Run quietly.", NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &(_cmd_options.verbose),
      "Run verbosely.", NULL },
    { "update", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.update),
      "If metadata already exists in the outputdir and an rpm is unchanged (based on file size "
      "and mtime) since the metadata was generated, reuse the existing metadata rather than "
      "recalculating it. In the case of a large repository with only a few new or modified rpms"
      "this can significantly reduce I/O and processing time.", NULL },
    { "update-md-path", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &(_cmd_options.update_md_paths),
      "Use the existing repodata for --update from this path.", NULL },
    { "skip-stat", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.skip_stat),
      "Skip the stat() call on a --update, assumes if the filename is the same then the file is"
      "still the same (only use this if you're fairly trusting or gullible).", NULL },
    { "version", 'V', 0, G_OPTION_ARG_NONE, &(_cmd_options.version),
      "Output version.", NULL},
    { "database", 'd', 0, G_OPTION_ARG_NONE, &(_cmd_options.database),
      "Generate sqlite databases for use with yum.", NULL },
    { "no-database", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.no_database),
      "Do not generate sqlite databases in the repository.", NULL },
    { "skip-symlinks", 'S', 0, G_OPTION_ARG_NONE, &(_cmd_options.skip_symlinks),
      "Ignore symlinks of packages.", NULL},
    { "checksum", 's', 0, G_OPTION_ARG_STRING, &(_cmd_options.checksum),
      "Choose the checksum type used in repomd.xml and for packages in the metadata."
      "The default is now \"sha256\".", "<checksum_type>" },
    { "changelog-limit", 0, 0, G_OPTION_ARG_INT, &(_cmd_options.changelog_limit),
      "Only import the last N changelog entries, from each rpm, into the metadata.",
      "<number>" },
    { "unique-md-filenames", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.unique_md_filenames),
      "Include the file's checksum in the metadata filename, helps HTTP caching (default).",
      NULL },
    { "simple-md-filenames", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.simple_md_filenames),
      "Do not include the file's checksum in the metadata filename.", NULL },
    { "workers", 0, 0, G_OPTION_ARG_INT, &(_cmd_options.workers),
      "Number of workers to spawn to read rpms.", NULL },
    { "xz", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.xz_compression),
      "Use xz for repodata compression.", NULL },
    { "compress-type", 0, 0, G_OPTION_ARG_STRING, &(_cmd_options.compress_type),
      "Which compression type to use.", "<compress_type>" },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};



struct CmdOptions *parse_arguments(int *argc, char ***argv)
{
    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new("- program that creates a repomd (xml-based rpm metadata) repository from a set of rpms.");
    g_option_context_add_main_entries(context, cmd_entries, NULL);

    gboolean ret = g_option_context_parse(context, argc, argv, &error);
    g_option_context_free(context);
    if (!ret) {
        g_print("Option parsing failed: %s\n", error->message);
        g_error_free(error);
        return NULL;
    }

    return &(_cmd_options);
}



gboolean
check_arguments(struct CmdOptions *options)
{
    // Check outputdir
    if (options->outputdir && !g_file_test(options->outputdir, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        g_warning("Specified outputdir \"%s\" doesn't exists", options->outputdir);
        return FALSE;
    }

    // Check workers
    if ((options->workers < 1) || (options->workers > 100)) {
        g_warning("Wrong number of workers - Using 5 workers.");
        options->workers = DEFAULT_WORKERS;
    }

    // Check changelog_limit
    if ((options->changelog_limit < 0) || (options->changelog_limit > 100)) {
        g_warning("Wrong changelog limit \"%d\" - Using 10", options->changelog_limit);
        options->changelog_limit = DEFAULT_CHANGELOG_LIMIT;
    }

    // Check simple filenames
    if (options->simple_md_filenames) {
        options->unique_md_filenames = FALSE;
    }

    // Check and set checksum type
    if (options->checksum) {
        GString *checksum_str = g_string_ascii_down(g_string_new(options->checksum));
        if (!strcmp(checksum_str->str, "sha256")) {
            options->checksum_type = CR_CHECKSUM_SHA256;
        } else if (!strcmp(checksum_str->str, "sha1")) {
            options->checksum_type = CR_CHECKSUM_SHA1;
        } else if (!strcmp(checksum_str->str, "md5")) {
            options->checksum_type = CR_CHECKSUM_MD5;
        } else {
            g_string_free(checksum_str, TRUE);
            g_critical("Unknown/Unsupported checksum type \"%s\"", options->checksum);
            return FALSE;
        }
        g_string_free(checksum_str, TRUE);
    }

    // Check and set compression type
    if (options->compress_type) {
        GString *compress_str = g_string_ascii_down(g_string_new(options->compress_type));
        if (!strcmp(compress_str->str, "gz")) {
            options->compression_type = CR_CW_GZ_COMPRESSION;
        } else if (!strcmp(compress_str->str, "bz2")) {
            options->compression_type = CR_CW_BZ2_COMPRESSION;
        } else if (!strcmp(compress_str->str, "xz")) {
            options->compression_type = CR_CW_XZ_COMPRESSION;
        } else {
            g_string_free(compress_str, TRUE);
            g_critical("Unknown/Unsupported compression type \"%s\"", options->compress_type);
            return FALSE;
        }
        g_string_free(compress_str, TRUE);
    }

    int x;

    // Process exclude glob masks
    x = 0;
    while (options->excludes && options->excludes[x] != NULL) {
        GPatternSpec *pattern = g_pattern_spec_new(options->excludes[x]);
        options->exclude_masks = g_slist_prepend(options->exclude_masks, (gpointer) pattern);
        x++;
    }

    // Process includepkgs
    x = 0;
    while (options->includepkg && options->includepkg[x] != NULL) {
        options->include_pkgs = g_slist_prepend(options->include_pkgs, (gpointer) options->includepkg[x]);
        x++;
    }

    // Check groupfile
    options->groupfile_fullpath = NULL;
    if (options->groupfile) {
        gboolean remote = FALSE;

        if (g_str_has_prefix(options->groupfile, "/")) {
            // Absolute local path
            options->groupfile_fullpath = g_strdup(options->groupfile);
        } else if (strstr(options->groupfile, "://")) {
            // Remote groupfile
            remote = TRUE;
            options->groupfile_fullpath = g_strdup(options->groupfile);
        } else {
            // Relative path (from intput_dir)
            options->groupfile_fullpath = g_strconcat(options->input_dir, options->groupfile, NULL);
        }

        if (!remote && !g_file_test(options->groupfile_fullpath, G_FILE_TEST_IS_REGULAR|G_FILE_TEST_EXISTS)) {
            g_warning("groupfile %s doesn't exists", options->groupfile_fullpath);
            return FALSE;
        }
    }

    // Process pkglist file
    if (options->pkglist) {
        if (!g_file_test(options->pkglist, G_FILE_TEST_IS_REGULAR|G_FILE_TEST_EXISTS)) {
            g_warning("pkglist file \"%s\" doesn't exists", options->pkglist);
        } else {
            char *content = NULL;
            GError *err;
            if (!g_file_get_contents(options->pkglist, &content, NULL, &err)) {
                g_warning("Error while reading pkglist file: %s", err->message);
                g_error_free(err);
                g_free(content);
            } else {
                x = 0;
                char **pkgs = g_strsplit(content, "\n", 0);
                while (pkgs && pkgs[x] != NULL) {
                    if (strlen(pkgs[x])) {
                        options->include_pkgs = g_slist_prepend(options->include_pkgs, (gpointer) pkgs[x]);
                    }
                    x++;
                }

                g_free(pkgs);  // Free pkgs array, pointers from array are already stored in include_pkgs list
                g_free(content);
            }
        }
    }

    // Process update_md_paths
    x = 0;
    while (options->update_md_paths && options->update_md_paths[x] != NULL) {
        char *path = options->update_md_paths[x];
        options->l_update_md_paths = g_slist_prepend(options->l_update_md_paths, (gpointer) path);
        x++;
    }

    return TRUE;
}



void
free_options(struct CmdOptions *options)
{
    g_free(options->input_dir);
    g_free(options->location_base);
    g_free(options->outputdir);
    g_free(options->pkglist);
    g_free(options->checksum);
    g_free(options->compress_type);
    g_free(options->groupfile);
    g_free(options->groupfile_fullpath);

    // Free excludes string list
    g_strfreev(options->excludes);
    g_strfreev(options->includepkg);

    GSList *element = NULL;

    // Items of include_pkgs are referenced from options->includepkg
    g_slist_free(options->include_pkgs);

    // Free glob exclude masks GSList
    for (element = options->exclude_masks; element; element = g_slist_next(element)) {
        g_pattern_spec_free( (GPatternSpec *) element->data );
    }
    g_slist_free(options->exclude_masks);

    // Free l_update_md_paths GSList
    for (element = options->l_update_md_paths; element; element = g_slist_next(element)) {
        g_free( (gchar *) element->data );
    }
    g_slist_free(options->l_update_md_paths);
}
