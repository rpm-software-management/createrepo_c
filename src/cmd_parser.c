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
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "cmd_parser.h"
#include "deltarpms.h"
#include "error.h"
#include "compression_wrapper.h"
#include "misc.h"
#include "cleanup.h"


#define ERR_DOMAIN                      CREATEREPO_C_ERROR
#define DEFAULT_CHECKSUM                "sha256"
#define DEFAULT_WORKERS                 5
#define DEFAULT_UNIQUE_MD_FILENAMES     TRUE
#define DEFAULT_IGNORE_LOCK             FALSE
#define DEFAULT_LOCAL_SQLITE            FALSE

struct CmdOptions _cmd_options = {
        .changelog_limit            = DEFAULT_CHANGELOG_LIMIT,
        .checksum                   = NULL,
        .workers                    = DEFAULT_WORKERS,
        .unique_md_filenames        = DEFAULT_UNIQUE_MD_FILENAMES,
        .checksum_type              = CR_CHECKSUM_SHA256,
        .retain_old                 = 0,
        .compression_type           = CR_CW_UNKNOWN_COMPRESSION,
        .general_compression_type   = CR_CW_UNKNOWN_COMPRESSION,
        .ignore_lock                = DEFAULT_IGNORE_LOCK,
        .md_max_age                 = G_GINT64_CONSTANT(0),
        .cachedir                   = NULL,
        .local_sqlite               = DEFAULT_LOCAL_SQLITE,
        .cut_dirs                   = 0,
        .location_prefix            = NULL,
        .repomd_checksum            = NULL,

        .deltas                     = FALSE,
        .oldpackagedirs             = NULL,
        .num_deltas                 = 1,
        .max_delta_rpm_size         = CR_DEFAULT_MAX_DELTA_RPM_SIZE,

        .checksum_cachedir          = NULL,
        .repomd_checksum_type       = CR_CHECKSUM_SHA256,

        .zck_compression            = FALSE,
        .zck_dict_dir               = NULL,
        .recycle_pkglist            = FALSE,
    };



// Command line params

static GOptionEntry cmd_entries[] =
{
    { "version", 'V', 0, G_OPTION_ARG_NONE, &(_cmd_options.version),
      "Show program's version number and exit.", NULL},
    { "quiet", 'q', 0, G_OPTION_ARG_NONE, &(_cmd_options.quiet),
      "Run quietly.", NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &(_cmd_options.verbose),
      "Run verbosely.", NULL },
    { "excludes", 'x', 0, G_OPTION_ARG_FILENAME_ARRAY, &(_cmd_options.excludes),
      "Path patterns to exclude, can be specified multiple times.", "PACKAGE_NAME_GLOB" },
    { "basedir", 0, 0, G_OPTION_ARG_FILENAME, &(_cmd_options.basedir),
      "Basedir for path to directories.", "BASEDIR" },
    { "baseurl", 'u', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.location_base),
      "Optional base URL location for all files.", "URL" },
    { "groupfile", 'g', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.groupfile),
      "Path to groupfile to include in metadata.",
      "GROUPFILE" },
    { "checksum", 's', 0, G_OPTION_ARG_STRING, &(_cmd_options.checksum),
      "Choose the checksum type used in repomd.xml and for packages in the "
      "metadata. The default is now \"sha256\".", "CHECKSUM_TYPE" },
    { "pretty", 'p', 0, G_OPTION_ARG_NONE, &(_cmd_options.pretty),
      "Make sure all xml generated is formatted (default)", NULL },
    { "database", 'd', 0, G_OPTION_ARG_NONE, &(_cmd_options.database),
      "Generate sqlite databases for use with yum.", NULL },
    { "no-database", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.no_database),
      "Do not generate sqlite databases in the repository.", NULL },
    { "update", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.update),
      "If metadata already exists in the outputdir and an rpm is unchanged "
      "(based on file size and mtime) since the metadata was generated, reuse "
      "the existing metadata rather than recalculating it. In the case of a "
      "large repository with only a few new or modified rpms "
      "this can significantly reduce I/O and processing time.", NULL },
    { "update-md-path", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &(_cmd_options.update_md_paths),
      "Existing metadata from this path are loaded and reused in addition to those "
      "present in the outputdir (works only with --update). Can be specified multiple times.", NULL },
    { "skip-stat", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.skip_stat),
      "Skip the stat() call on a --update, assumes if the filename is the same "
      "then the file is still the same (only use this if you're fairly "
      "trusting or gullible).", NULL },
    { "split", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.split),
      "Run in split media mode. Rather than pass a single directory, take a set of"
      "directories corresponding to different volumes in a media set. "
      "Meta data is created in the first given directory", NULL },
    { "pkglist", 'i', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.pkglist),
      "Specify a text file which contains the complete list of files to "
      "include in the repository from the set found in the directory. File "
      "format is one package per line, no wildcards or globs.", "FILENAME" },
    { "includepkg", 'n', 0, G_OPTION_ARG_FILENAME_ARRAY, &(_cmd_options.includepkg),
      "Specify pkgs to include on the command line. Takes urls as well as local paths.",
      "PACKAGE" },
    { "outputdir", 'o', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.outputdir),
      "Optional output directory.", "URL" },
    { "skip-symlinks", 'S', 0, G_OPTION_ARG_NONE, &(_cmd_options.skip_symlinks),
      "Ignore symlinks of packages.", NULL},
    { "changelog-limit", 0, 0, G_OPTION_ARG_INT, &(_cmd_options.changelog_limit),
      "Only import the last N changelog entries, from each rpm, into the metadata.",
      "NUM" },
    { "unique-md-filenames", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.unique_md_filenames),
      "Include the file's checksum in the metadata filename, helps HTTP caching (default).",
      NULL },
    { "simple-md-filenames", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.simple_md_filenames),
      "Do not include the file's checksum in the metadata filename.", NULL },
    { "retain-old-md", 0, 0, G_OPTION_ARG_INT, &(_cmd_options.retain_old),
      "Specify NUM to 0 to remove all repodata present in old repomd.xml or any other positive number to keep all old repodata. "
      "Use --compatibility flag to get the behavior of original createrepo: "
      "Keep around the latest (by timestamp) NUM copies of the old repodata (works only for primary, filelists, other and their DB variants).", "NUM" },
    { "distro", 0, 0, G_OPTION_ARG_STRING_ARRAY, &(_cmd_options.distro_tags),
      "Distro tag and optional cpeid: --distro'cpeid,textname'.", "DISTRO" },
    { "content", 0, 0, G_OPTION_ARG_STRING_ARRAY, &(_cmd_options.content_tags),
      "Tags for the content in the repository.", "CONTENT_TAGS" },
    { "repo", 0, 0, G_OPTION_ARG_STRING_ARRAY, &(_cmd_options.repo_tags),
      "Tags to describe the repository itself.", "REPO_TAGS" },
    { "revision", 0, 0, G_OPTION_ARG_STRING, &(_cmd_options.revision),
      "User-specified revision for this repository.", "REVISION" },
    { "set-timestamp-to-revision", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.set_timestamp_to_revision),
      "Set timestamp fields in repomd.xml and last modification times of created repodata to a value given with --revision. "
      "This requires --revision to be a timestamp formatted in 'date +%s' format.", NULL },
    { "read-pkgs-list", 0, 0, G_OPTION_ARG_FILENAME, &(_cmd_options.read_pkgs_list),
      "Output the paths to the pkgs actually read useful with --update.",
      "READ_PKGS_LIST" },
    { "workers", 0, 0, G_OPTION_ARG_INT, &(_cmd_options.workers),
      "Number of workers to spawn to read rpms.", NULL },
    { "xz", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.xz_compression),
      "Use xz for repodata compression.", NULL },
    { "compress-type", 0, 0, G_OPTION_ARG_STRING, &(_cmd_options.compress_type),
      "Which compression type to use.", "COMPRESSION_TYPE" },
    { "general-compress-type", 0, 0, G_OPTION_ARG_STRING, &(_cmd_options.general_compress_type),
      "Which compression type to use (even for primary, filelists and other xml).",
      "COMPRESSION_TYPE" },
#ifdef WITH_ZCHUNK
    { "zck", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.zck_compression),
      "Generate zchunk files as well as the standard repodata.", NULL },
    { "zck-dict-dir", 0, 0, G_OPTION_ARG_FILENAME, &(_cmd_options.zck_dict_dir),
      "Directory containing compression dictionaries for use by zchunk", "ZCK_DICT_DIR" },
#endif
    { "keep-all-metadata", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.keep_all_metadata),
      "Keep all additional metadata (not primary, filelists and other xml or sqlite files, "
      "nor their compressed variants) from source repository during update.", NULL },
    { "compatibility", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.compatibility),
      "Enforce maximal compatibility with classical createrepo (Affects only: --retain-old-md).", NULL },
    { "retain-old-md-by-age", 0, 0, G_OPTION_ARG_STRING, &(_cmd_options.retain_old_md_by_age),
      "During --update, remove all files in repodata/ which are older "
      "then the specified period of time. (e.g. '2h', '30d', ...). "
      "Available units (m - minutes, h - hours, d - days)", "AGE" },
    { "cachedir", 'c', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.cachedir),
      "Set path to cache dir", "CACHEDIR." },
#ifdef CR_DELTA_RPM_SUPPORT
    { "deltas", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.deltas),
      "Tells createrepo to generate deltarpms and the delta metadata.", NULL },
    { "oldpackagedirs", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &(_cmd_options.oldpackagedirs),
      "Paths to look for older pkgs to delta against. Can be specified "
      "multiple times.", "PATH" },
    { "num-deltas", 0, 0, G_OPTION_ARG_INT, &(_cmd_options.num_deltas),
      "The number of older versions to make deltas against. Defaults to 1.", "INT" },
    { "max-delta-rpm-size", 0, 0, G_OPTION_ARG_INT64, &(_cmd_options.max_delta_rpm_size),
      "Max size of an rpm that to run deltarpm against (in bytes).", "MAX_DELTA_RPM_SIZE" },
#endif
    { "local-sqlite", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.local_sqlite),
      "Gen sqlite DBs locally (into a directory for temporary files). "
      "Sometimes, sqlite has a trouble to gen DBs on a NFS mount, "
      "use this option in such cases. "
      "This option could lead to a higher memory consumption "
      "if TMPDIR is set to /tmp or not set at all, because then the /tmp is "
      "used and /tmp dir is often a ramdisk.", NULL },
    { "cut-dirs", 0, 0, G_OPTION_ARG_INT, &(_cmd_options.cut_dirs),
      "Ignore NUM of directory components in location_href during repodata "
      "generation", "NUM" },
    { "location-prefix", 0, 0, G_OPTION_ARG_FILENAME, &(_cmd_options.location_prefix),
      "Append this prefix before location_href in output repodata", "PREFIX" },
    { "repomd-checksum", 0, 0, G_OPTION_ARG_STRING, &(_cmd_options.repomd_checksum),
      "Checksum type to be used in repomd.xml", "CHECKSUM_TYPE"},
    { "error-exit-val", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.error_exit_val),
      "Exit with retval 2 if there were any errors during processing", NULL },
    { "recycle-pkglist", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.recycle_pkglist),
      "Read the list of packages from old metadata directory and re-use it.  This "
      "option is only useful with --update (complements --pkglist and friends).",
      NULL },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL },
};


static GOptionEntry expert_entries[] =
{
    { "ignore-lock", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.ignore_lock),
      "Expert (risky) option: Ignore an existing .repodata/. "
      "(Remove the existing .repodata/ and create an empty new one "
      "to serve as a lock for other createrepo instances. For the repodata "
      "generation, a different temporary dir with the name in format "
      "\".repodata.time.microseconds.pid/\" will be used). "
      "NOTE: Use this option on your "
      "own risk! If two createrepos run simultaneously, then the state of the "
      "generated metadata is not guaranteed - it can be inconsistent and wrong.",
      NULL },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL },
};


struct CmdOptions *parse_arguments(int *argc, char ***argv, GError **err)
{
    gboolean ret;
    GOptionContext *context;
    GOptionGroup *group_expert;

    assert(!err || *err == NULL);

    context = g_option_context_new("<directory_to_index>");
    g_option_context_set_summary(context, "Program that creates a repomd "
            "(xml-based rpm metadata) repository from a set of rpms.");
    g_option_context_add_main_entries(context, cmd_entries, NULL);

    group_expert = g_option_group_new("expert",
                                      "Expert (risky) options",
                                      "Expert (risky) options",
                                      NULL,
                                      NULL);
    g_option_group_add_entries(group_expert, expert_entries);
    g_option_context_add_group(context, group_expert);

    ret = g_option_context_parse(context, argc, argv, err);
    g_option_context_free(context);

    if (!ret)
        return NULL;

    return &(_cmd_options);
}

/** Convert string to compression type set an error if failed.
 * @param type_str      String with compression type (e.g. "gz")
 * @param type          Pointer to cr_CompressionType variable
 * @param err           Err that will be set in case of error
 */
static gboolean
check_and_set_compression_type(const char *type_str,
                               cr_CompressionType *type,
                               GError **err)
{
    assert(!err || *err == NULL);

    _cleanup_string_free_ GString *compress_str = NULL;
    compress_str = g_string_ascii_down(g_string_new(type_str));

    if (!strcmp(compress_str->str, "gz")) {
        *type = CR_CW_GZ_COMPRESSION;
    } else if (!strcmp(compress_str->str, "bz2")) {
        *type = CR_CW_BZ2_COMPRESSION;
    } else if (!strcmp(compress_str->str, "xz")) {
        *type = CR_CW_XZ_COMPRESSION;
    } else {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Unknown/Unsupported compression type \"%s\"", type_str);
        return FALSE;
    }

    return TRUE;
}

/** Convert a time period to seconds (gint64 value)
 * Format: "[0-9]+[mhd]?"
 * Units: m - minutes, h - hours, d - days, ...
 * @param timeperiod    Time period
 * @param time          Time period converted to gint64 will be stored here
 */
static gboolean
parse_period_of_time(const gchar *timeperiod, gint64 *time, GError **err)
{
    assert(!err || *err == NULL);

    gchar *endptr = NULL;
    gint64 val = g_ascii_strtoll(timeperiod, &endptr, 0);

    *time = G_GINT64_CONSTANT(0);

    // Check the state of the conversion
    if (val == 0 && endptr == timeperiod) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Bad time period \"%s\"", timeperiod);
        return FALSE;
    }

    if (val == G_MAXINT64) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Time period \"%s\" is too high", timeperiod);
        return FALSE;
    }

    if (val == G_MININT64) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Time period \"%s\" is too low", timeperiod);
        return FALSE;
    }

    if (!endptr || endptr[0] == '\0') // Secs
        *time = (gint64) val;
    else if (!strcmp(endptr, "m"))    // Minutes
        *time = (gint64) val*60;
    else if (!strcmp(endptr, "h"))    // Hours
        *time = (gint64) val*60*60;
    else if (!strcmp(endptr, "d"))    // Days
        *time = (gint64) val*24*60*60;
    else {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Bad time unit \"%s\"", endptr);
        return FALSE;
    }

    return TRUE;
}

gboolean
check_arguments(struct CmdOptions *options,
                const char *input_dir,
                GError **err)
{
    assert(!err || *err == NULL);

    // Check outputdir
    if (options->outputdir && !g_file_test(options->outputdir, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Specified outputdir \"%s\" doesn't exists",
                    options->outputdir);
        return FALSE;
    }

    // Check workers
    if ((options->workers < 1) || (options->workers > 100)) {
        g_warning("Wrong number of workers - Using 5 workers.");
        options->workers = DEFAULT_WORKERS;
    }

    // Check changelog_limit
    if ((options->changelog_limit < -1)) {
        g_warning("Wrong changelog limit \"%d\" - Using 10", options->changelog_limit);
        options->changelog_limit = DEFAULT_CHANGELOG_LIMIT;
    }

    // Check simple filenames
    if (options->simple_md_filenames) {
        options->unique_md_filenames = FALSE;
    }

    // Check and set checksum type
    if (options->checksum) {
        cr_ChecksumType type;
        type = cr_checksum_type(options->checksum);
        if (type == CR_CHECKSUM_UNKNOWN) {
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                        "Unknown/Unsupported checksum type \"%s\"",
                        options->checksum);
            return FALSE;
        }
        options->checksum_type = type;
    }

    // Check and set checksum type for repomd
    if (options->repomd_checksum) {
        cr_ChecksumType type;
        type = cr_checksum_type(options->repomd_checksum);
        if (type == CR_CHECKSUM_UNKNOWN) {
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                        "Unknown/Unsupported checksum type \"%s\"",
                        options->repomd_checksum);
            return FALSE;
        }
        options->repomd_checksum_type = type;
    } else {
        options->repomd_checksum_type = options->checksum_type;
    }

    // Check and set compression type
    if (options->compress_type) {
        if (!check_and_set_compression_type(options->compress_type,
                                            &(options->compression_type),
                                            err)) {
            return FALSE;
        }
    }
    //options --xz has priority over compress_type, but not over general_compress_type
    if (options->xz_compression) {
        options->compression_type = CR_CW_XZ_COMPRESSION;
    }

    // Check and set general compression type
    if (options->general_compress_type) {
        if (!check_and_set_compression_type(options->general_compress_type,
                                            &(options->general_compression_type),
                                            err)) {
            return FALSE;
        }
    }

    int x;

    // Process exclude glob masks
    x = 0;
    while (options->excludes && options->excludes[x] != NULL) {
        GPatternSpec *pattern = g_pattern_spec_new(options->excludes[x]);
        options->exclude_masks = g_slist_prepend(options->exclude_masks,
                                                 (gpointer) pattern);
        x++;
    }

    // Process includepkgs
    x = 0;
    while (options->includepkg && options->includepkg[x] != NULL) {
        options->include_pkgs = g_slist_prepend(options->include_pkgs,
                                  (gpointer) g_strdup(options->includepkg[x]));
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
            options->groupfile_fullpath = g_strconcat(input_dir,
                                                      options->groupfile,
                                                      NULL);
        }

        if (!remote && !g_file_test(options->groupfile_fullpath, G_FILE_TEST_IS_REGULAR)) {
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                        "groupfile %s doesn't exists",
                        options->groupfile_fullpath);
            return FALSE;
        }
    }

    // Process pkglist file
    if (options->pkglist) {
        if (!g_file_test(options->pkglist, G_FILE_TEST_IS_REGULAR)) {
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                        "pkglist file \"%s\" doesn't exists", options->pkglist);
            return FALSE;
        } else {
            char *content = NULL;
            GError *tmp_err = NULL;
            if (!g_file_get_contents(options->pkglist, &content, NULL, &tmp_err)) {
                g_warning("Error while reading pkglist file: %s", tmp_err->message);
                g_error_free(tmp_err);
                g_free(content);
            } else {
                x = 0;
                char **pkgs = g_strsplit(content, "\n", 0);
                while (pkgs && pkgs[x] != NULL) {
                    if (strlen(pkgs[x])) {
                        options->include_pkgs = g_slist_prepend(options->include_pkgs,
                                                 (gpointer) g_strdup(pkgs[x]));
                    }
                    x++;
                }

                g_strfreev(pkgs);
                g_free(content);
            }
        }
    }

    // Process update_md_paths
    if (options->update_md_paths && !options->update)
        g_warning("Usage of --update-md-path without --update has no effect!");

    x = 0;
    while (options->update_md_paths && options->update_md_paths[x] != NULL) {
        char *path = options->update_md_paths[x];
        options->l_update_md_paths = g_slist_prepend(options->l_update_md_paths,
                                                     (gpointer) path);
        x++;
    }

    // Check keep-all-metadata
    if (options->keep_all_metadata && !options->update) {
        g_warning("--keep-all-metadata has no effect (--update is not used)");
    }

    // Process --distro tags
    x = 0;
    while (options->distro_tags && options->distro_tags[x]) {
        if (!strchr(options->distro_tags[x], ',')) {
            options->distro_cpeids = g_slist_append(options->distro_cpeids,
                                                    NULL);
            options->distro_values = g_slist_append(options->distro_values,
                                        g_strdup(options->distro_tags[x]));
            x++;
            continue;
        }

        gchar **items = g_strsplit(options->distro_tags[x++], ",", 2);
        if (!items) continue;
        if (!items[0] || !items[1] || items[1][0] == '\0') {
            g_strfreev(items);
            continue;
        }

        if (items[0][0] != '\0')
            options->distro_cpeids = g_slist_append(options->distro_cpeids,
                                                    g_strdup(items[0]));
        else
            options->distro_cpeids = g_slist_append(options->distro_cpeids,
                                                    NULL);
        options->distro_values = g_slist_append(options->distro_values,
                                                g_strdup(items[1]));
        g_strfreev(items);
    }

    // Check retain-old-md-by-age
    if (options->retain_old_md_by_age) {
        if (options->retain_old) {
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                        "--retain-old-md-by-age cannot be combined "
                        "with --retain-old-md");
            return FALSE;
        }

        // Parse argument
        if (!parse_period_of_time(options->retain_old_md_by_age,
                                  &options->md_max_age,
                                  err))
            return FALSE;
    }

    // check if --revision is numeric, when --set-timestamp-to-revision is given
    if (options->set_timestamp_to_revision) {
        char *endptr;
        gint64 revision = strtoll(options->revision, &endptr, 0);
        if (endptr == options->revision || *endptr != '\0') {
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "--set-timestamp-to-revision require numeric value for --revision");
            return FALSE;
        }
        if ((errno == ERANGE && revision == LLONG_MAX) || revision < 0) {
            g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "--revision value out of range");
            return FALSE;
        }
    }

    // Check oldpackagedirs
    x = 0;
    while (options->oldpackagedirs && options->oldpackagedirs[x]) {
        char *path = options->oldpackagedirs[x];
        options->oldpackagedirs_paths = g_slist_prepend(
                                            options->oldpackagedirs_paths,
                                            (gpointer) path);
        x++;
    }

    // Check cut_dirs
    if (options->cut_dirs < 0) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "--cur-dirs value must be positive integer");
        return FALSE;
    }

    // Zchunk options
    if (options->zck_dict_dir && !options->zck_compression) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
                    "Cannot use --zck-dict-dir without setting --zck");
        return FALSE;
    }
    if (options->zck_dict_dir)
        options->zck_dict_dir = cr_normalize_dir_path(options->zck_dict_dir);

    return TRUE;
}



void
free_options(struct CmdOptions *options)
{
    g_free(options->basedir);
    g_free(options->location_base);
    g_free(options->outputdir);
    g_free(options->pkglist);
    g_free(options->checksum);
    g_free(options->compress_type);
    g_free(options->groupfile);
    g_free(options->groupfile_fullpath);
    g_free(options->revision);
    g_free(options->retain_old_md_by_age);
    g_free(options->cachedir);
    g_free(options->checksum_cachedir);

    g_strfreev(options->excludes);
    g_strfreev(options->includepkg);
    g_strfreev(options->distro_tags);
    g_strfreev(options->content_tags);
    g_strfreev(options->repo_tags);
    g_strfreev(options->oldpackagedirs);

    cr_slist_free_full(options->include_pkgs, g_free);
    cr_slist_free_full(options->exclude_masks,
                      (GDestroyNotify) g_pattern_spec_free);
    cr_slist_free_full(options->l_update_md_paths, g_free);
    cr_slist_free_full(options->distro_cpeids, g_free);
    cr_slist_free_full(options->distro_values, g_free);
    cr_slist_free_full(options->modulemd_metadata, g_free);
    g_slist_free(options->oldpackagedirs_paths);
}
