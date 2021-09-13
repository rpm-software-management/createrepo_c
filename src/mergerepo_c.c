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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#ifdef WITH_LIBMODULEMD
#include <modulemd.h>
#endif /* WITH_LIBMODULEMD */
#include "error.h"
#include "createrepo_shared.h"
#include "version.h"
#include "helpers.h"
#include "metadata_internal.h"
#include "misc.h"
#include "locate_metadata.h"
#include "load_metadata.h"
#include "package.h"
#include "xml_dump.h"
#include "repomd.h"
#include "sqlite.h"
#include "threads.h"
#include "xml_file.h"
#include "cleanup.h"
#include "koji.h"

#define DEFAULT_OUTPUTDIR               "merged_repo/"

#include "mergerepo_c.h"

struct CmdOptions _cmd_options = {
        .db_compression_type = DEFAULT_DB_COMPRESSION_TYPE,
        .groupfile_compression_type = DEFAULT_GROUPFILE_COMPRESSION_TYPE,
        .merge_method = MM_DEFAULT,
        .unique_md_filenames = TRUE,
        .simple_md_filenames = FALSE,

        .zck_compression = FALSE,
        .zck_dict_dir = NULL,
    };

// TODO:
//  - rozvijet architekturu na listy tak jak to dela mergedrepo

static GOptionEntry cmd_entries[] =
{
    { "version", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.version),
      "Show program's version number and exit", NULL },
    { "repo", 'r', 0, G_OPTION_ARG_FILENAME_ARRAY, &(_cmd_options.repos),
      "Repo url", "REPOS" },
    { "repo-prefix-search", 0, 0, G_OPTION_ARG_STRING, &(_cmd_options.repo_prefix_search),
      "Repository prefix to be replaced by NEW_PREFIX.", "OLD_PREFIX" },
    { "repo-prefix-replace", 0, 0, G_OPTION_ARG_STRING, &(_cmd_options.repo_prefix_replace),
      "Repository prefix URL by which the OLD_PREFIX is replaced.", "NEW_PREFIX" },
    { "archlist", 'a', 0, G_OPTION_ARG_STRING, &(_cmd_options.archlist),
      "Defaults to all arches - otherwise specify arches", "ARCHLIST" },
    { "database", 'd', 0, G_OPTION_ARG_NONE, &(_cmd_options.database),
      "", NULL },
    { "no-database", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.no_database),
      "", NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &(_cmd_options.verbose),
      "", NULL },
    { "outputdir", 'o', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.outputdir),
      "Location to create the repository", "OUTPUTDIR" },
    { "nogroups", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.nogroups),
      "Do not merge group (comps) metadata", NULL },
    { "noupdateinfo", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.noupdateinfo),
      "Do not merge updateinfo metadata", NULL },
    { "compress-type", 0, 0, G_OPTION_ARG_STRING, &(_cmd_options.compress_type),
      "Which compression type to use", "COMPRESS_TYPE" },
#ifdef WITH_ZCHUNK
    { "zck", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.zck_compression),
      "Generate zchunk files as well as the standard repodata.", NULL },
    { "zck-dict-dir", 0, 0, G_OPTION_ARG_FILENAME, &(_cmd_options.zck_dict_dir),
      "Directory containing compression dictionaries for use by zchunk", "ZCK_DICT_DIR" },
#endif
    { "method", 0, 0, G_OPTION_ARG_STRING, &(_cmd_options.merge_method_str),
      "Specify merge method for packages with the same name and arch (available"
      " merge methods: repo (default), ts, nvr)", "MERGE_METHOD" },
    { "all", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.all),
      "Include all packages with the same name and arch if version or release "
      "is different. If used --method argument is ignored!", NULL },
    { "noarch-repo", 0, 0, G_OPTION_ARG_FILENAME, &(_cmd_options.noarch_repo_url),
      "Packages with noarch architecture will be replaced by package from this "
      "repo if exists in it.", "URL" },
    { "unique-md-filenames", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.unique_md_filenames),
      "Include the file's checksum in the metadata filename, helps HTTP caching (default).",
      NULL },
    { "simple-md-filenames", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.simple_md_filenames),
      "Do not include the file's checksum in the metadata filename.", NULL },
    { "omit-baseurl", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.omit_baseurl),
      "Don't add a baseurl to packages that don't have one before." , NULL},

    // -- Options related to Koji-mergerepos behaviour
    { "koji", 'k', 0, G_OPTION_ARG_NONE, &(_cmd_options.koji),
       "Enable koji mergerepos behaviour. (Optionally select simple mode with: --simple)", NULL},
    { "simple", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.koji_simple),
       "Enable koji specific simple merge mode where we keep even packages with "
       "identical NEVRAs. Only works with combination with --koji/-k.", NULL},
    { "pkgorigins", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.pkgorigins),
        "Enable standard mergerepos behavior while also providing the "
        "pkgorigins file for koji.",
        NULL},
    { "arch-expand", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.arch_expand),
        "Add multilib architectures for specified archlist and expand all of them. "
        "Only works with combination with --archlist.",
        NULL},
    { "groupfile", 'g', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.groupfile),
      "Path to groupfile to include in metadata.", "GROUPFILE" },
    { "blocked", 'b', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.blocked),
      "A file containing a list of srpm names to exclude from the merged repo. "
      "Only works with combination with --koji/-k.", "FILE" },
    // -- Options related to Koji-mergerepos behaviour - end

    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};


GSList *
append_arch(GSList *list, gchar *arch, gboolean expand)
{
    // Try to find arch in the list
    for (GSList *elem = list; elem; elem = g_slist_next(elem)) {
        if (!g_strcmp0(elem->data, arch))
            return list;  // Arch already exists
    }

    list = g_slist_prepend(list, g_strdup(arch));

    if (expand) {
        // expand arch
        if (!g_strcmp0(arch, "i386")) {
            list = append_arch(list, "i486", FALSE);
            list = append_arch(list, "i586", FALSE);
            list = append_arch(list, "geode", FALSE);
            list = append_arch(list, "i686", FALSE);
            list = append_arch(list, "athlon", FALSE);
        } else if (!g_strcmp0(arch, "x86_64")) {
            list = append_arch(list, "ia32e", FALSE);
            list = append_arch(list, "amd64", FALSE);
        } else if (!g_strcmp0(arch, "ppc64")) {
            list = append_arch(list, "ppc64pseries", FALSE);
            list = append_arch(list, "ppc64iseries", FALSE);
        } else if (!g_strcmp0(arch, "sparc64")) {
            list = append_arch(list, "sparc64v", FALSE);
            list = append_arch(list, "sparc64v2", FALSE);
        } else if (!g_strcmp0(arch, "sparc")) {
            list = append_arch(list, "sparcv8", FALSE);
            list = append_arch(list, "sparcv9", FALSE);
            list = append_arch(list, "sparcv9v", FALSE);
            list = append_arch(list, "sparcv9v2", FALSE);
        } else if (!g_strcmp0(arch, "alpha")) {
            list = append_arch(list, "alphaev4", FALSE);
            list = append_arch(list, "alphaev45", FALSE);
            list = append_arch(list, "alphaev5", FALSE);
            list = append_arch(list, "alphaev56", FALSE);
            list = append_arch(list, "alphapca56", FALSE);
            list = append_arch(list, "alphaev6", FALSE);
            list = append_arch(list, "alphaev67", FALSE);
            list = append_arch(list, "alphaev68", FALSE);
            list = append_arch(list, "alphaev7", FALSE);
        } else if (!g_strcmp0(arch, "armhfp")) {
            list = append_arch(list, "armv7hl", FALSE);
            list = append_arch(list, "armv7hnl", FALSE);
        } else if (!g_strcmp0(arch, "arm")) {
            list = append_arch(list, "rmv5tel", FALSE);
            list = append_arch(list, "armv5tejl", FALSE);
            list = append_arch(list, "armv6l", FALSE);
            list = append_arch(list, "armv7l", FALSE);
        } else if (!g_strcmp0(arch, "sh4")) {
            list = append_arch(list, "sh4a", FALSE);
        }
    }

    // Always include noarch
    list = append_arch(list, "noarch", FALSE);

    return list;
}


GSList *
append_multilib_arch(GSList *list, gchar *arch)
{
    if (!g_strcmp0(arch, "x86_64"))
        list = append_arch(list, "i386", TRUE);
    else if (!g_strcmp0(arch, "ppc64"))
        list = append_arch(list, "ppc", TRUE);
    else if (!g_strcmp0(arch, "s390x"))
        list = append_arch(list, "s390", TRUE);

    return list;
}

gboolean
check_arguments(struct CmdOptions *options)
{
    int x;
    gboolean ret = TRUE;

    if (options->outputdir){
        options->out_dir = cr_normalize_dir_path(options->outputdir);
    } else {
        options->out_dir = g_strdup(DEFAULT_OUTPUTDIR);
    }

    options->out_repo = g_strconcat(options->out_dir, "repodata/", NULL);
    options->tmp_out_repo = g_strconcat(options->out_dir, ".repodata/", NULL);

    // Process repos
    x = 0;
    options->repo_list = NULL;
    while (options->repos && options->repos[x] != NULL) {
        char *normalized = cr_normalize_dir_path(options->repos[x]);
        if (normalized) {
            options->repo_list = g_slist_prepend(options->repo_list, normalized);
        }
        x++;
    }
    // Reverse come with downloading repos
    // options->repo_list = g_slist_reverse (options->repo_list);

    // Process archlist
    options->arch_list = NULL;
    if (options->archlist) {
        x = 0;
        gchar **arch_set = g_strsplit_set(options->archlist, ",;", -1);
        while (arch_set && arch_set[x] != NULL) {
            gchar *arch = arch_set[x];
            if (arch[0] != '\0') {
                // Append (and expand) the arch
                options->arch_list = append_arch(options->arch_list,
                                                 arch,
                                                 options->koji || options->arch_expand);
                // Support multilib repos
                if (options->koji || options->arch_expand)
                    options->arch_list = append_multilib_arch(options->arch_list,
                                                              arch);
            }
            x++;
        }
        g_strfreev(arch_set);
    } else if (options->koji) {
        // Work only with noarch packages if --koji and no archlist specified
        options->arch_list = append_arch(options->arch_list, "noarch", TRUE);
    }

    if (!options->archlist && options->arch_expand){
        g_critical("--arch-expand cannot be used without -a/--archlist argument");
        ret = FALSE;
    }

    // Compress type
    if (options->compress_type) {

        cr_CompressionType type;
        type = cr_compression_type(options->compress_type);

        if (type == CR_CW_UNKNOWN_COMPRESSION) {
            g_critical("Compression %s not available: Please choose from: "
                       "gz or bz2 or xz", options->compress_type);
            ret = FALSE;
        } else {
            options->db_compression_type = type;
            options->groupfile_compression_type = type;
        }
    }

    // Merge method
    if (options->merge_method_str) {
        if (options->koji) {
            g_warning("With -k/--koji argument merge method is ignored (--all is implicitly used).");
        } else if (!g_strcmp0(options->merge_method_str, "repo")) {
            options->merge_method = MM_FIRST_FROM_IDENTICAL_NA;
        } else if (!g_strcmp0(options->merge_method_str, "ts")) {
            options->merge_method = MM_NEWEST_FROM_IDENTICAL_NA;
        } else if (!g_strcmp0(options->merge_method_str, "nvr")) {
            options->merge_method = MM_WITH_HIGHEST_NEVRA;
        } else {
            g_critical("Unknown merge method %s", options->merge_method_str);
            ret = FALSE;
        }
    }

    // Check simple filenames
    if (options->simple_md_filenames) {
        options->unique_md_filenames = FALSE;
    }

    if (options->all)
        options->merge_method = MM_FIRST_FROM_IDENTICAL_NEVRA;

    // Koji arguments
    if (options->koji) {
        options->all = TRUE;
        if (options->koji_simple) {
            options->merge_method = MM_ALL_WITH_IDENTICAL_NEVRA;
        }else{
            options->merge_method = MM_FIRST_FROM_IDENTICAL_NEVRA;
        }
    }

    if (options->blocked) {
        if (!options->koji) {
            g_critical("-b/--blocked cannot be used without -k/--koji argument");
            ret = FALSE;
        }
        if (!g_file_test(options->blocked, G_FILE_TEST_EXISTS)) {
            g_critical("File %s doesn't exists", options->blocked);
            ret = FALSE;
        }
    }

    if (options->repo_prefix_search || options->repo_prefix_replace) {
        if (options->repo_prefix_search == NULL) {
            g_critical("--repo-prefix-replace must be used together with --repo-prefix-search");
            ret = FALSE;
        }
        else if (options->repo_prefix_replace == NULL) {
            g_critical("--repo-prefix-search must be used together with --repo-prefix-replace");
            ret = FALSE;
        }
        else if (*options->repo_prefix_search == '\0') {
            g_critical("--repo-prefix-search cannot be an empty string.");
            ret = FALSE;
        }
    }

    // Zchunk options
    if (options->zck_dict_dir && !options->zck_compression) {
        g_critical("Cannot use --zck-dict-dir without setting --zck");
        ret = FALSE;
    }
    if (options->zck_dict_dir)
        options->zck_dict_dir = cr_normalize_dir_path(options->zck_dict_dir);

    return ret;
}


struct CmdOptions *
parse_arguments(int *argc, char ***argv)
{
    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new("--repo=url --repo=url");
    g_option_context_set_summary(context, "Take one or more repositories and "
                                 "merge their metadata into a new repo");
    g_option_context_add_main_entries(context, cmd_entries, NULL);

    gboolean ret = g_option_context_parse(context, argc, argv, &error);
    if (!ret) {
        g_print("Option parsing failed: %s\n", error->message);
        g_option_context_free(context);
        g_error_free(error);
        return NULL;
    }

    if (*argc > 1) {
        g_printerr("Argument parsing failed.\n");
        g_print("%s", g_option_context_get_help(context, TRUE, NULL));
        return NULL;
    }

    g_option_context_free(context);
    return &(_cmd_options);
}



void
free_options(struct CmdOptions *options)
{
    g_free(options->outputdir);
    g_free(options->archlist);
    g_free(options->compress_type);
    g_free(options->merge_method_str);
    g_free(options->noarch_repo_url);

    g_free(options->groupfile);
    g_free(options->blocked);

    g_strfreev(options->repos);
    g_free(options->out_dir);
    g_free(options->out_repo);
    g_free(options->tmp_out_repo);

    GSList *element = NULL;

    // Free include_pkgs GSList
    for (element = options->repo_list; element; element = g_slist_next(element)) {
        g_free( (gchar *) element->data );
    }
    g_slist_free(options->repo_list);

    // Free include_pkgs GSList
    for (element = options->arch_list; element; element = g_slist_next(element)) {
        g_free( (gchar *) element->data );
    }
    g_slist_free(options->arch_list);
}



void
free_merged_values(gpointer data)
{
    GSList *element = (GSList *) data;
    for (; element; element=g_slist_next(element)) {
        cr_Package *pkg = (cr_Package *) element->data;
        cr_package_free(pkg);
    }
    g_slist_free((GSList *) data);
}



GHashTable *
new_merged_metadata_hashtable()
{
    GHashTable *hashtable = g_hash_table_new_full(g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  free_merged_values);
    return hashtable;
}



void
destroy_merged_metadata_hashtable(GHashTable *hashtable)
{
    if (hashtable) {
        g_hash_table_destroy(hashtable);
    }
}



// Merged table structure: {"package_name": [pkg, pkg, pkg, ...], ...}
// Return codes:
//  0 = Package was not added
//  1 = Package was added
//  2 = Package replaced old package
//  3 = Package was added as a duplicate
static int
add_package(cr_Package *pkg,
            gchar *repopath,
            GHashTable *merged,
            GSList *arch_list,
            MergeMethod merge_method,
            struct KojiMergedReposStuff *koji_stuff,
            gboolean omit_baseurl,
            int repoid)
{
    GSList *list, *element;
    int ret = 1;


    if (omit_baseurl)
        repopath = NULL;

    // Check if the package meet the command line architecture constraints

    if (arch_list) {
        gboolean right_arch = FALSE;
        for (element=arch_list; element; element=g_slist_next(element)) {
            if (!g_strcmp0(pkg->arch, (gchar *) element->data)) {
                right_arch = TRUE;
            }
        }

        if (!right_arch) {
            g_debug("Skip - %s (Bad arch: %s)", pkg->name, pkg->arch);
            return 0;
        }
    }


    // Koji-mergerepos specific behaviour -----------------------
    if (koji_stuff) {
        if (!koji_allowed(pkg, koji_stuff))
            return 0;
        // For first repo (with --koji) ignore baseURL (RhBug: 1220082)
        if (repoid == 0)
            repopath = NULL;
    }
    // Koji-mergerepos specific behaviour end --------------------

    // Lookup package in the merged
    list = (GSList *) g_hash_table_lookup(merged, pkg->name);

    // Key doesn't exist yet
    if (!list) {
        list = g_slist_prepend(list, pkg);
        if ((!pkg->location_base || *pkg->location_base == '\0') && repopath) {
            pkg->location_base = cr_safe_string_chunk_insert(pkg->chunk, repopath);
        }
        g_hash_table_insert (merged, (gpointer) g_strdup(pkg->name), (gpointer) list);
        return 1;
    }

    // Check if package with the architecture isn't in the list already
    for (element=list; element; element=g_slist_next(element)) {
        cr_Package *c_pkg = (cr_Package *) element->data;
        if (!g_strcmp0(pkg->arch, c_pkg->arch)) {

            // Two packages have same name and arch
            // Use selected merge method to determine which package should
            // be included

            switch(merge_method) {
                // REPO merge method
                case MM_FIRST_FROM_IDENTICAL_NA:
                    // Package with the same arch already exists
                    g_debug("Package %s (%s) already exists",
                            pkg->name, pkg->arch);
                    return 0;

                // TS merge method
                case MM_NEWEST_FROM_IDENTICAL_NA:
                    if (pkg->time_file > c_pkg->time_file) {
                        // Remove older package
                        cr_package_free(c_pkg);
                        // Replace package in element
                        if (!pkg->location_base)
                            pkg->location_base = cr_safe_string_chunk_insert(pkg->chunk,
                                                                       repopath);
                        element->data = pkg;
                        return 2;
                    } else {
                        g_debug("Newer package %s (%s) already exists",
                                pkg->name, pkg->arch);
                        return 0;
                    }
                    break;

                // NVR merge method
                case MM_WITH_HIGHEST_NEVRA: {
                    gboolean pkg_is_newer = FALSE;

                    int epoch_cmp   = cr_cmp_version_str(pkg->epoch, c_pkg->epoch);
                    int version_cmp = cr_cmp_version_str(pkg->version, c_pkg->version);
                    int release_cmp = cr_cmp_version_str(pkg->release, c_pkg->release);


                    if (epoch_cmp == 1)
                        pkg_is_newer = TRUE;
                    else if (epoch_cmp == 0 && version_cmp == 1)
                        pkg_is_newer = TRUE;
                    else if (epoch_cmp == 0 && version_cmp == 0 && release_cmp == 1)
                        pkg_is_newer = TRUE;

                    if (pkg_is_newer) {
                        // Remove older package
                        cr_package_free(c_pkg);
                        // Replace package in element
                        if (!pkg->location_base)
                            pkg->location_base = cr_safe_string_chunk_insert(pkg->chunk, repopath);
                        element->data = pkg;
                        return 2;
                    } else {
                        g_debug("Newer version of package %s.%s "
                                "(epoch: %s) (ver: %s) (rel: %s) already exists",
                                pkg->name, pkg->arch,
                                pkg->epoch   ? pkg->epoch   : "0",
                                pkg->version ? pkg->version : "N/A",
                                pkg->release ? pkg->release : "N/A");
                        return 0;
                    }
                    break;
                }
                case MM_FIRST_FROM_IDENTICAL_NEVRA:
                    // Two packages have same name and arch but all param is used

                    // We want to check if two packages are the same.
                    // We already know that name and arch matches.
                    // We need to check version and release and epoch
                    if ((cr_cmp_version_str(pkg->epoch, c_pkg->epoch) == 0)
                        && (cr_cmp_version_str(pkg->version, c_pkg->version) == 0)
                        && (cr_cmp_version_str(pkg->release, c_pkg->release) == 0))
                    {
                        // Both packages are the same (at least by NEVRA values)
                        g_debug("Same version of package %s.%s "
                                "(epoch: %s) (ver: %s) (rel: %s) already exists",
                                pkg->name, pkg->arch,
                                pkg->epoch   ? pkg->epoch   : "0",
                                pkg->version ? pkg->version : "N/A",
                                pkg->release ? pkg->release : "N/A");
                        return 0;
                    }
                    break;
                case MM_ALL_WITH_IDENTICAL_NEVRA:
                    // We want even duplicates with exact NEVRAs
                    if ((cr_cmp_version_str(pkg->epoch, c_pkg->epoch) == 0)
                        && (cr_cmp_version_str(pkg->version, c_pkg->version) == 0)
                        && (cr_cmp_version_str(pkg->release, c_pkg->release) == 0))
                    {
                        // Both packages are the same (at least by NEVRA values)
                        // We warn, but do not omit it
                        g_debug("Duplicate rpm %s.%s "
                                "(epoch: %s) (ver: %s) (rel: %s)",
                                pkg->name, pkg->arch,
                                pkg->epoch   ? pkg->epoch   : "0",
                                pkg->version ? pkg->version : "N/A",
                                pkg->release ? pkg->release : "N/A");
                        ret = 3;
                        goto add_package;
                    }
                    break;
            }
        }
    }

add_package:
    if (!pkg->location_base) {
        pkg->location_base = cr_safe_string_chunk_insert(pkg->chunk, repopath);
    }

    // Add package
    // XXX: The first list element (pointed from hashtable) must stay first!
    // g_slist_append() is suitable but non effective, insert a new element
    // right after first element is optimal (at least for now)
    if (g_slist_insert(list, pkg, 1) != list) {
        assert(0);
    }

    return ret;
}


/**
 * @return Number of loaded packages or -1 on error
 */
long
merge_repos(GHashTable *merged,
#ifdef WITH_LIBMODULEMD
            ModulemdModuleIndex **module_index,
#endif
            GSList *repo_list,
            GSList *arch_list,
            MergeMethod merge_method,
            GHashTable *noarch_hashtable,
            struct KojiMergedReposStuff *koji_stuff,
            gboolean omit_baseurl,
            gchar *repo_prefix_search,
            gchar *repo_prefix_replace)
{
    long loaded_packages = 0;
    GSList *used_noarch_keys = NULL;
    GError *err = NULL;

#ifdef WITH_LIBMODULEMD
    g_autoptr(ModulemdModuleIndexMerger) merger = NULL;

    merger = modulemd_module_index_merger_new();
#endif /* WITH_LIBMODULEMD */

    // Load all repos

    int repoid = 0;
    GSList *element = NULL;
    for (element = repo_list; element; element = g_slist_next(element), repoid++) {
        gchar *repopath;                    // base url of current repodata
        cr_Metadata *metadata;              // current repodata
        struct cr_MetadataLocation *ml;     // location of current repodata

        ml = (struct cr_MetadataLocation *) element->data;
        if (!ml) {
            g_critical("Bad location!");
            break;
        }

        metadata = cr_metadata_new(CR_HT_KEY_HASH, 0, NULL);
        repopath = cr_normalize_dir_path(ml->original_url);

        // Base paths in output of original createrepo doesn't have trailing '/'
        if (repopath && strlen(repopath) > 1)
            repopath[strlen(repopath)-1] = '\0';

        // If repo_prefix_search and repo_prefix_replace is set, replace
        // repo_prefix_search in the repopath by repo_prefix_replace.
        if (repo_prefix_search && *repo_prefix_search &&
                repo_prefix_replace &&
                g_str_has_prefix(repopath, repo_prefix_search)) {
            gchar *repo_suffix = repopath + strlen(repo_prefix_search);
            gchar *new_repopath = g_strconcat(repo_prefix_replace, repo_suffix, NULL);
            g_free(repopath);
            repopath = new_repopath;
        }

        g_debug("Processing: %s", repopath);

        if (cr_metadata_load_xml(metadata, ml, &err) != CRE_OK) {
            cr_metadata_free(metadata);
            g_critical("Cannot load repo: \"%s\" : %s", ml->original_url, err->message);
            g_error_free(err);
            g_free(repopath);
            return -1;
        }

#ifdef WITH_LIBMODULEMD
        if (cr_metadata_modulemd(metadata)) {
            modulemd_module_index_merger_associate_index (
                merger, cr_metadata_modulemd (metadata), 0);
        }
#endif /* WITH_LIBMODULEMD */

        GHashTableIter iter;
        gpointer key, value;
        guint original_size;
        long repo_loaded_packages = 0;

        original_size = g_hash_table_size(cr_metadata_hashtable(metadata));

        g_hash_table_iter_init (&iter, cr_metadata_hashtable(metadata));
        while (g_hash_table_iter_next (&iter, &key, &value)) {
            int ret;
            cr_Package *pkg = (cr_Package *) value;

            // Lookup a package in the noarch_hashtable
            gboolean noarch_pkg_used = FALSE;
            if (noarch_hashtable && !g_strcmp0(pkg->arch, "noarch")) {
                cr_Package *noarch_pkg;
                noarch_pkg = g_hash_table_lookup(noarch_hashtable, pkg->location_href);
                if (noarch_pkg) {
                    pkg = noarch_pkg;
                    noarch_pkg_used = TRUE;
                }
            }

            g_debug("Reading metadata for %s (%s-%s.%s)",
                    pkg->name, pkg->version, pkg->release, pkg->arch);

            // Add package
            ret = add_package(pkg,
                              repopath,
                              merged,
                              arch_list,
                              merge_method,
                              koji_stuff,
                              omit_baseurl,
                              repoid);

            if (ret > 0) {
                if (!noarch_pkg_used) {
                    // Original package was added
                    // => remove only record from hashtable
                    g_hash_table_iter_steal(&iter);
                } else {
                    // Package from noarch repo was added
                    // => do not remove record, just make note
                    used_noarch_keys = g_slist_prepend(used_noarch_keys,
                                                       pkg->location_href);
                    g_debug("Package: %s (from: %s) has been replaced by noarch package",
                            pkg->location_href, repopath);
                }

                if (ret == 1) {
                    repo_loaded_packages++;
                    // Koji-mergerepos specific behaviour -----------
                    if (koji_stuff && koji_stuff->pkgorigins) {
                        _cleanup_free_ gchar *nvra = cr_package_nvra(pkg);
                        _cleanup_free_ gchar *url = cr_prepend_protocol(ml->original_url);

                        cr_printf(NULL,
                                  koji_stuff->pkgorigins,
                                  "%s\t%s\n",
                                  nvra, url);
                    }
                    // Koji-mergerepos specific behaviour - end -----
                }
            }
        }

        loaded_packages += repo_loaded_packages;
        cr_metadata_free(metadata);
        g_debug("Repo: %s (Loaded: %ld Used: %ld)", repopath,
                (unsigned long) original_size, repo_loaded_packages);
        g_free(repopath);
    }

#ifdef WITH_LIBMODULEMD
    g_autoptr(ModulemdModuleIndex) moduleindex =
        modulemd_module_index_merger_resolve (merger, &err);
    g_auto (GStrv) module_names =
        modulemd_module_index_get_module_names_as_strv (moduleindex);

    if (moduleindex && g_strv_length(module_names) == 0) {
        /* If the final module index is empty, free it so it won't get
         * output in dump_merged_metadata()
         */
        g_clear_pointer (&moduleindex, g_object_unref);
    }

    *module_index = g_steal_pointer(&moduleindex);
#endif


    // Steal used keys from noarch_hashtable

    for (element = used_noarch_keys; element; element = g_slist_next(element))
        g_hash_table_steal(noarch_hashtable, (gconstpointer) element->data);
    g_slist_free(used_noarch_keys);

    return loaded_packages;
}



int
package_cmp(gconstpointer a_p, gconstpointer b_p)
{
    int ret;
    const cr_Package *pkg_a = a_p;
    const cr_Package *pkg_b = b_p;
    ret = g_strcmp0(pkg_a->location_href, pkg_b->location_href);
    if (ret) return ret;
    return g_strcmp0(pkg_a->location_base, pkg_b->location_base);
}


#ifdef WITH_LIBMODULEMD
static gint
modulemd_write_handler (void          *data,
                        unsigned char *buffer,
                        size_t         size)
{
    int ret;
    CR_FILE *cr_file = (CR_FILE *)data;
    g_autoptr (GError) err = NULL;

    ret = cr_write (cr_file, buffer, size, &err);
    if (ret < 1) {
        g_warning ("Could not write modulemd: %s", err->message);
        return 0;
    }

    return 1;
}
#endif /* WITH_LIBMODULEMD */

int
dump_merged_metadata(GHashTable *merged_hashtable,
                     long packages,
                     gchar *groupfile,
#ifdef WITH_LIBMODULEMD
                     ModulemdModuleIndex *module_index,
#endif /* WITH_LIBMODULEMD */
                     struct CmdOptions *cmd_options)
{
    GError *tmp_err = NULL;

    // Create/Open output xml files

    cr_ContentStat *pri_stat = cr_contentstat_new(CR_CHECKSUM_SHA256, NULL);
    cr_ContentStat *fil_stat = cr_contentstat_new(CR_CHECKSUM_SHA256, NULL);
    cr_ContentStat *oth_stat = cr_contentstat_new(CR_CHECKSUM_SHA256, NULL);

    cr_XmlFile *pri_f;
    cr_XmlFile *fil_f;
    cr_XmlFile *oth_f;

    gchar *pri_zck_filename = NULL;
    gchar *fil_zck_filename = NULL;
    gchar *oth_zck_filename = NULL;
    cr_XmlFile *pri_cr_zck = NULL;
    cr_XmlFile *fil_cr_zck = NULL;
    cr_XmlFile *oth_cr_zck = NULL;
    cr_ContentStat *pri_zck_stat = cr_contentstat_new(CR_CHECKSUM_SHA256, NULL);
    cr_ContentStat *fil_zck_stat = cr_contentstat_new(CR_CHECKSUM_SHA256, NULL);
    cr_ContentStat *oth_zck_stat = cr_contentstat_new(CR_CHECKSUM_SHA256, NULL);
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

    const char *groupfile_suffix = cr_compression_suffix(
                                    cmd_options->groupfile_compression_type);

    gchar *pri_xml_filename = g_strconcat(cmd_options->tmp_out_repo,
                                          "/primary.xml.gz", NULL);
    gchar *fil_xml_filename = g_strconcat(cmd_options->tmp_out_repo,
                                          "/filelists.xml.gz", NULL);
    gchar *oth_xml_filename = g_strconcat(cmd_options->tmp_out_repo,
                                          "/other.xml.gz", NULL);

    gchar *update_info_filename = NULL;
    if (!cmd_options->noupdateinfo)
        update_info_filename  = g_strconcat(cmd_options->tmp_out_repo,
                                            "/updateinfo.xml",
                                            groupfile_suffix, NULL);

    pri_f = cr_xmlfile_sopen_primary(pri_xml_filename,
                                     CR_CW_GZ_COMPRESSION,
                                     pri_stat,
                                     &tmp_err);
    if (tmp_err) {
        g_critical("Cannot open %s: %s", pri_xml_filename, tmp_err->message);
        cr_contentstat_free(pri_stat, NULL);
        cr_contentstat_free(fil_stat, NULL);
        cr_contentstat_free(oth_stat, NULL);
        cr_contentstat_free(pri_zck_stat, NULL);
        cr_contentstat_free(fil_zck_stat, NULL);
        cr_contentstat_free(oth_zck_stat, NULL);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        g_free(update_info_filename);
        g_error_free(tmp_err);
        g_free(pri_dict);
        g_free(fil_dict);
        g_free(oth_dict);
        cr_xmlfile_close(pri_f, NULL);
        return 0;
    }

    fil_f = cr_xmlfile_sopen_filelists(fil_xml_filename,
                                       CR_CW_GZ_COMPRESSION,
                                       fil_stat,
                                       &tmp_err);
    if (tmp_err) {
        g_critical("Cannot open %s: %s", fil_xml_filename, tmp_err->message);
        cr_contentstat_free(pri_stat, NULL);
        cr_contentstat_free(fil_stat, NULL);
        cr_contentstat_free(oth_stat, NULL);
        cr_contentstat_free(pri_zck_stat, NULL);
        cr_contentstat_free(fil_zck_stat, NULL);
        cr_contentstat_free(oth_zck_stat, NULL);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        g_free(update_info_filename);
        cr_xmlfile_close(fil_f, NULL);
        cr_xmlfile_close(pri_f, NULL);
        g_error_free(tmp_err);
        g_free(pri_dict);
        g_free(fil_dict);
        g_free(oth_dict);
        return 0;
    }

    oth_f = cr_xmlfile_sopen_other(oth_xml_filename,
                                   CR_CW_GZ_COMPRESSION,
                                   oth_stat,
                                   &tmp_err);
    if (tmp_err) {
        g_critical("Cannot open %s: %s", oth_xml_filename, tmp_err->message);
        cr_contentstat_free(pri_stat, NULL);
        cr_contentstat_free(fil_stat, NULL);
        cr_contentstat_free(oth_stat, NULL);
        cr_contentstat_free(pri_zck_stat, NULL);
        cr_contentstat_free(fil_zck_stat, NULL);
        cr_contentstat_free(oth_zck_stat, NULL);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        g_free(update_info_filename);
        cr_xmlfile_close(oth_f, NULL);
        cr_xmlfile_close(fil_f, NULL);
        cr_xmlfile_close(pri_f, NULL);
        g_error_free(tmp_err);
        g_free(pri_dict);
        g_free(fil_dict);
        g_free(oth_dict);
        return 0;
    }


    cr_xmlfile_set_num_of_pkgs(pri_f, packages, NULL);
    cr_xmlfile_set_num_of_pkgs(fil_f, packages, NULL);
    cr_xmlfile_set_num_of_pkgs(oth_f, packages, NULL);

    if (cmd_options->zck_compression) {
        g_debug("Creating .xml.zck files");

        pri_zck_filename = g_strconcat(cmd_options->tmp_out_repo,
                                       "/primary.xml.zck", NULL);
        fil_zck_filename = g_strconcat(cmd_options->tmp_out_repo,
                                       "/filelists.xml.zck", NULL);
        oth_zck_filename = g_strconcat(cmd_options->tmp_out_repo,
                                       "/other.xml.zck", NULL);

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
        cr_xmlfile_set_num_of_pkgs(pri_cr_zck, packages, NULL);
        cr_xmlfile_set_num_of_pkgs(fil_cr_zck, packages, NULL);
        cr_xmlfile_set_num_of_pkgs(oth_cr_zck, packages, NULL);
    }

    // Prepare sqlite if needed

    cr_SqliteDb *pri_db = NULL;
    cr_SqliteDb *fil_db = NULL;
    cr_SqliteDb *oth_db = NULL;

    if (!cmd_options->no_database) {
        gchar *pri_db_filename = NULL;
        gchar *fil_db_filename = NULL;
        gchar *oth_db_filename = NULL;

        pri_db_filename = g_strconcat(cmd_options->tmp_out_repo,
                                      "/primary.sqlite", NULL);
        fil_db_filename = g_strconcat(cmd_options->tmp_out_repo,
                                      "/filelists.sqlite", NULL);
        oth_db_filename = g_strconcat(cmd_options->tmp_out_repo,
                                      "/other.sqlite", NULL);

        pri_db = cr_db_open_primary(pri_db_filename, NULL);
        fil_db = cr_db_open_filelists(fil_db_filename, NULL);
        oth_db = cr_db_open_other(oth_db_filename, NULL);

        g_free(pri_db_filename);
        g_free(fil_db_filename);
        g_free(oth_db_filename);
    }


    // Dump hashtable

    GList *keys, *key;
    keys = g_hash_table_get_keys(merged_hashtable);
    keys = g_list_sort(keys, (GCompareFunc) g_strcmp0);

    char *prev_srpm = NULL;

    for (key = keys; key; key = g_list_next(key)) {
        gpointer value = g_hash_table_lookup(merged_hashtable, key->data);
        GSList *element = (GSList *) value;
        element = g_slist_sort(element, package_cmp);
        for (; element; element=g_slist_next(element)) {
            struct cr_XmlStruct res;
            cr_Package *pkg;

            pkg = (cr_Package *) element->data;
            res = cr_xml_dump(pkg, NULL);

            g_debug("Writing metadata for %s (%s-%s.%s)",
                    pkg->name, pkg->version, pkg->release, pkg->arch);

            if (cmd_options->zck_compression &&
               (!prev_srpm || !pkg->rpm_sourcerpm ||
                strlen(prev_srpm) != strlen(pkg->rpm_sourcerpm) ||
                strncmp(pkg->rpm_sourcerpm, prev_srpm, strlen(prev_srpm)) != 0)) {
                cr_end_chunk(pri_cr_zck->f, NULL);
                cr_end_chunk(fil_cr_zck->f, NULL);
                cr_end_chunk(oth_cr_zck->f, NULL);
                g_free(prev_srpm);
                if (pkg->rpm_sourcerpm)
                    prev_srpm = g_strdup(pkg->rpm_sourcerpm);
                else
                    prev_srpm = NULL;
            }
            cr_xmlfile_add_chunk(pri_f, (const char *) res.primary, NULL);
            cr_xmlfile_add_chunk(fil_f, (const char *) res.filelists, NULL);
            cr_xmlfile_add_chunk(oth_f, (const char *) res.other, NULL);
            if (cmd_options->zck_compression) {
                cr_xmlfile_add_chunk(pri_cr_zck, (const char *) res.primary, NULL);
                cr_xmlfile_add_chunk(fil_cr_zck, (const char *) res.filelists, NULL);
                cr_xmlfile_add_chunk(oth_cr_zck, (const char *) res.other, NULL);
            }

            if (!cmd_options->no_database) {
                cr_db_add_pkg(pri_db, pkg, NULL);
                cr_db_add_pkg(fil_db, pkg, NULL);
                cr_db_add_pkg(oth_db, pkg, NULL);
            }

            free(res.primary);
            free(res.filelists);
            free(res.other);
        }
    }
    g_free(prev_srpm);
    g_list_free(keys);


    // Close files

    cr_xmlfile_close(pri_f, NULL);
    cr_xmlfile_close(fil_f, NULL);
    cr_xmlfile_close(oth_f, NULL);
    if (cmd_options->zck_compression) {
        cr_xmlfile_close(pri_cr_zck, NULL);
        cr_xmlfile_close(fil_cr_zck, NULL);
        cr_xmlfile_close(oth_cr_zck, NULL);
    }


    // Write updateinfo.xml
    // TODO

    if (!cmd_options->noupdateinfo) {
        CR_FILE *update_info = cr_open(update_info_filename,
                                       CR_CW_MODE_WRITE,
                                       cmd_options->groupfile_compression_type,
                                       &tmp_err);
        if (update_info) {
            cr_puts(update_info,
                    "<?xml version=\"1.0\"?>\n<updates></updates>\n",
                    NULL);
            cr_close(update_info, NULL);
        } else {
            g_warning("Cannot open %s: %s",
                      update_info_filename,
                      tmp_err->message);
            g_error_free(tmp_err);
        }
    }


#ifdef WITH_LIBMODULEMD
    // Write modulemd
    g_autofree gchar *modulemd_filename = NULL;

    if (module_index) {
        gboolean ret;
        modulemd_filename =
            g_strconcat(cmd_options->tmp_out_repo, "/modules.yaml.gz", NULL);
        CR_FILE *modulemd = cr_open(modulemd_filename,
                                    CR_CW_MODE_WRITE,
                                    CR_CW_GZ_COMPRESSION,
                                    &tmp_err);
        if (modulemd) {
            ret = modulemd_module_index_dump_to_custom(module_index,
                                                       modulemd_write_handler,
                                                       modulemd,
                                                       &tmp_err);
            if (!ret) {
                g_warning("Could not write module metadata: %s",
                          tmp_err->message);
            }
            cr_close(modulemd, NULL);
        } else {
            g_warning("Cannot open %s: %s",
                      modulemd_filename,
                      tmp_err->message);
            g_error_free(tmp_err);
        }
    }
#endif



    // Prepare repomd records

    cr_RepomdRecord *pri_xml_rec = cr_repomd_record_new("primary", pri_xml_filename);
    cr_RepomdRecord *fil_xml_rec = cr_repomd_record_new("filelists", fil_xml_filename);
    cr_RepomdRecord *oth_xml_rec = cr_repomd_record_new("other", oth_xml_filename);
    cr_RepomdRecord *pri_db_rec               = NULL;
    cr_RepomdRecord *fil_db_rec               = NULL;
    cr_RepomdRecord *oth_db_rec               = NULL;
    cr_RepomdRecord *pri_zck_rec              = NULL;
    cr_RepomdRecord *fil_zck_rec              = NULL;
    cr_RepomdRecord *oth_zck_rec              = NULL;
    cr_RepomdRecord *groupfile_rec            = NULL;
    cr_RepomdRecord *compressed_groupfile_rec = NULL;
    cr_RepomdRecord *groupfile_zck_rec        = NULL;
    cr_RepomdRecord *update_info_rec          = NULL;
    cr_RepomdRecord *update_info_zck_rec      = NULL;
    cr_RepomdRecord *pkgorigins_rec           = NULL;
    cr_RepomdRecord *pkgorigins_zck_rec       = NULL;

#ifdef WITH_LIBMODULEMD
    cr_RepomdRecord *modulemd_rec = NULL;
    cr_RepomdRecord *modulemd_zck_rec = NULL;

    if (module_index) {
        modulemd_rec =
          cr_repomd_record_new("modules", modulemd_filename);
    }
#endif /* WITH_LIBMODULEMD */


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
                                                CR_CHECKSUM_SHA256,
                                                NULL);
    g_thread_pool_push(fill_pool, pri_fill_task, NULL);

    fil_fill_task = cr_repomdrecordfilltask_new(fil_xml_rec,
                                                CR_CHECKSUM_SHA256,
                                                NULL);
    g_thread_pool_push(fill_pool, fil_fill_task, NULL);

    oth_fill_task = cr_repomdrecordfilltask_new(oth_xml_rec,
                                                CR_CHECKSUM_SHA256,
                                                NULL);
    g_thread_pool_push(fill_pool, oth_fill_task, NULL);

#ifdef WITH_LIBMODULEMD
    cr_RepomdRecordFillTask *mmd_fill_task;
    if (module_index) {
        mmd_fill_task = cr_repomdrecordfilltask_new(modulemd_rec,
                                                    CR_CHECKSUM_SHA256,
                                                    NULL);
        g_thread_pool_push(fill_pool, mmd_fill_task, NULL);

        if (cmd_options->zck_compression) {
            modulemd_zck_rec = cr_repomd_record_new("modules_zck",
                                                    NULL);
            cr_repomd_record_compress_and_fill(modulemd_rec,
                                               modulemd_zck_rec,
                                               CR_CHECKSUM_SHA256,
                                               CR_CW_ZCK_COMPRESSION,
                                               NULL, NULL);
        }
    }
#endif /* WITH_LIBMODULEMD */

    // Groupfile

    if (groupfile) {
        groupfile_rec = cr_repomd_record_new("group", groupfile);
        compressed_groupfile_rec = cr_repomd_record_new("group_gz", NULL);
        cr_repomd_record_compress_and_fill(groupfile_rec,
                                           compressed_groupfile_rec,
                                           CR_CHECKSUM_SHA256,
                                           cmd_options->groupfile_compression_type,
                                           NULL, NULL);
        if (cmd_options->zck_compression) {
            groupfile_zck_rec = cr_repomd_record_new("group_zck", NULL);
            cr_repomd_record_compress_and_fill(groupfile_rec,
                                               groupfile_zck_rec,
                                               CR_CHECKSUM_SHA256,
                                               CR_CW_ZCK_COMPRESSION,
                                               NULL, NULL);
        }
    }


    // Update info

    if (!cmd_options->noupdateinfo) {
        update_info_rec = cr_repomd_record_new("updateinfo", update_info_filename);
        cr_repomd_record_fill(update_info_rec, CR_CHECKSUM_SHA256, NULL);
        if (cmd_options->zck_compression) {
            update_info_zck_rec = cr_repomd_record_new("updateinfo_zck", NULL);
            cr_repomd_record_compress_and_fill(update_info_rec,
                                               update_info_zck_rec,
                                               CR_CHECKSUM_SHA256,
                                               CR_CW_ZCK_COMPRESSION,
                                               NULL, NULL);
        }
    }


    // Pkgorigins

    if (cmd_options->koji || cmd_options->pkgorigins) {
        gchar *pkgorigins_path = g_strconcat(cmd_options->tmp_out_repo, "pkgorigins.gz", NULL);
        pkgorigins_rec = cr_repomd_record_new("origin", pkgorigins_path);
        cr_repomd_record_fill(pkgorigins_rec, CR_CHECKSUM_SHA256, NULL);
        if (cmd_options->zck_compression) {
            pkgorigins_zck_rec = cr_repomd_record_new("origin_zck", NULL);
            cr_repomd_record_compress_and_fill(pkgorigins_rec,
                                               pkgorigins_zck_rec,
                                               CR_CHECKSUM_SHA256,
                                               CR_CW_ZCK_COMPRESSION,
                                               NULL, NULL);
        }
        g_free(pkgorigins_path);
    }

    // Wait till repomd record fill task of xml files ends.

    g_thread_pool_free(fill_pool, FALSE, TRUE);

    cr_repomdrecordfilltask_free(pri_fill_task, NULL);
    cr_repomdrecordfilltask_free(fil_fill_task, NULL);
    cr_repomdrecordfilltask_free(oth_fill_task, NULL);
#ifdef WITH_LIBMODULEMD
    if (module_index) {
      cr_repomdrecordfilltask_free(mmd_fill_task, NULL);
    }
#endif


    // Sqlite db

    if (!cmd_options->no_database) {
        const char *db_suffix = cr_compression_suffix(cmd_options->db_compression_type);

        // Insert XML checksums into the dbs
        cr_db_dbinfo_update(pri_db, pri_xml_rec->checksum, NULL);
        cr_db_dbinfo_update(fil_db, fil_xml_rec->checksum, NULL);
        cr_db_dbinfo_update(oth_db, oth_xml_rec->checksum, NULL);

        cr_db_close(pri_db, NULL);
        cr_db_close(fil_db, NULL);
        cr_db_close(oth_db, NULL);

        // Compress dbs
        gchar *pri_db_filename = g_strconcat(cmd_options->tmp_out_repo,
                                             "/primary.sqlite", NULL);
        gchar *fil_db_filename = g_strconcat(cmd_options->tmp_out_repo,
                                             "/filelists.sqlite", NULL);
        gchar *oth_db_filename = g_strconcat(cmd_options->tmp_out_repo,
                                             "/other.sqlite", NULL);

        gchar *pri_db_c_filename = g_strconcat(pri_db_filename, db_suffix, NULL);
        gchar *fil_db_c_filename = g_strconcat(fil_db_filename, db_suffix, NULL);
        gchar *oth_db_c_filename = g_strconcat(oth_db_filename, db_suffix, NULL);

        GThreadPool *compress_pool =  g_thread_pool_new(cr_compressing_thread,
                                                        NULL, 3, FALSE, NULL);

        cr_CompressionTask *pri_db_task;
        cr_CompressionTask *fil_db_task;
        cr_CompressionTask *oth_db_task;

        pri_db_task = cr_compressiontask_new(pri_db_filename,
                                             pri_db_c_filename,
                                             cmd_options->db_compression_type,
                                             CR_CHECKSUM_SHA256,
                                             NULL, FALSE, 1, NULL);
        g_thread_pool_push(compress_pool, pri_db_task, NULL);

        fil_db_task = cr_compressiontask_new(fil_db_filename,
                                             fil_db_c_filename,
                                             cmd_options->db_compression_type,
                                             CR_CHECKSUM_SHA256,
                                             NULL, FALSE, 1, NULL);
        g_thread_pool_push(compress_pool, fil_db_task, NULL);

        oth_db_task = cr_compressiontask_new(oth_db_filename,
                                             oth_db_c_filename,
                                             cmd_options->db_compression_type,
                                             CR_CHECKSUM_SHA256,
                                             NULL, FALSE, 1, NULL);
        g_thread_pool_push(compress_pool, oth_db_task, NULL);

        g_thread_pool_free(compress_pool, FALSE, TRUE);

        // Prepare repomd records
        pri_db_rec = cr_repomd_record_new("primary_db", pri_db_c_filename);
        fil_db_rec = cr_repomd_record_new("filelists_db", fil_db_c_filename);
        oth_db_rec = cr_repomd_record_new("other_db", oth_db_c_filename);

        g_free(pri_db_filename);
        g_free(fil_db_filename);
        g_free(oth_db_filename);

        g_free(pri_db_c_filename);
        g_free(fil_db_c_filename);
        g_free(oth_db_c_filename);

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
                                                       CR_CHECKSUM_SHA256,
                                                       NULL);
        g_thread_pool_push(fill_pool, pri_db_fill_task, NULL);

        fil_db_fill_task = cr_repomdrecordfilltask_new(fil_db_rec,
                                                       CR_CHECKSUM_SHA256,
                                                       NULL);
        g_thread_pool_push(fill_pool, fil_db_fill_task, NULL);

        oth_db_fill_task = cr_repomdrecordfilltask_new(oth_db_rec,
                                                       CR_CHECKSUM_SHA256,
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

        g_free(pri_zck_filename);
        g_free(fil_zck_filename);
        g_free(oth_zck_filename);

        cr_repomd_record_load_zck_contentstat(pri_zck_rec, pri_zck_stat);
        cr_repomd_record_load_zck_contentstat(fil_zck_rec, fil_zck_stat);
        cr_repomd_record_load_zck_contentstat(oth_zck_rec, oth_zck_stat);

        fill_pool = g_thread_pool_new(cr_repomd_record_fill_thread,
                                      NULL, 3, FALSE, NULL);

        cr_RepomdRecordFillTask *pri_zck_fill_task;
        cr_RepomdRecordFillTask *fil_zck_fill_task;
        cr_RepomdRecordFillTask *oth_zck_fill_task;

        pri_zck_fill_task = cr_repomdrecordfilltask_new(pri_zck_rec,
                                                        CR_CHECKSUM_SHA256,
                                                        NULL);
        g_thread_pool_push(fill_pool, pri_zck_fill_task, NULL);

        fil_zck_fill_task = cr_repomdrecordfilltask_new(fil_zck_rec,
                                                        CR_CHECKSUM_SHA256,
                                                        NULL);
        g_thread_pool_push(fill_pool, fil_zck_fill_task, NULL);

        oth_zck_fill_task = cr_repomdrecordfilltask_new(oth_zck_rec,
                                                        CR_CHECKSUM_SHA256,
                                                        NULL);
        g_thread_pool_push(fill_pool, oth_zck_fill_task, NULL);

        g_thread_pool_free(fill_pool, FALSE, TRUE);

        cr_repomdrecordfilltask_free(pri_zck_fill_task, NULL);
        cr_repomdrecordfilltask_free(fil_zck_fill_task, NULL);
        cr_repomdrecordfilltask_free(oth_zck_fill_task, NULL);
    }
    cr_contentstat_free(pri_zck_stat, NULL);
    cr_contentstat_free(fil_zck_stat, NULL);
    cr_contentstat_free(oth_zck_stat, NULL);

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
        cr_repomd_record_rename_file(groupfile_rec, NULL);
        cr_repomd_record_rename_file(compressed_groupfile_rec, NULL);
        cr_repomd_record_rename_file(groupfile_zck_rec, NULL);
        cr_repomd_record_rename_file(update_info_rec, NULL);
        cr_repomd_record_rename_file(update_info_zck_rec, NULL);
        cr_repomd_record_rename_file(pkgorigins_rec, NULL);
        cr_repomd_record_rename_file(pkgorigins_zck_rec, NULL);

#ifdef WITH_LIBMODULEMD
        cr_repomd_record_rename_file(modulemd_rec, NULL);
        cr_repomd_record_rename_file(modulemd_zck_rec, NULL);
#endif /* WITH_LIBMODULEMD */
    }


    // Gen repomd.xml content

    cr_Repomd *repomd_obj = cr_repomd_new();
    cr_repomd_set_record(repomd_obj, pri_xml_rec);
    cr_repomd_set_record(repomd_obj, fil_xml_rec);
    cr_repomd_set_record(repomd_obj, oth_xml_rec);
    cr_repomd_set_record(repomd_obj, pri_db_rec);
    cr_repomd_set_record(repomd_obj, fil_db_rec);
    cr_repomd_set_record(repomd_obj, oth_db_rec);
    cr_repomd_set_record(repomd_obj, pri_zck_rec);
    cr_repomd_set_record(repomd_obj, fil_zck_rec);
    cr_repomd_set_record(repomd_obj, oth_zck_rec);
    cr_repomd_set_record(repomd_obj, groupfile_rec);
    cr_repomd_set_record(repomd_obj, compressed_groupfile_rec);
    cr_repomd_set_record(repomd_obj, groupfile_zck_rec);
    cr_repomd_set_record(repomd_obj, update_info_rec);
    cr_repomd_set_record(repomd_obj, update_info_zck_rec);
    cr_repomd_set_record(repomd_obj, pkgorigins_rec);
    cr_repomd_set_record(repomd_obj, pkgorigins_zck_rec);

#ifdef WITH_LIBMODULEMD
    cr_repomd_set_record(repomd_obj, modulemd_rec);
        cr_repomd_set_record(repomd_obj, modulemd_zck_rec);
#endif /* WITH_LIBMODULEMD */

    char *repomd_xml = cr_xml_dump_repomd(repomd_obj, NULL);

    cr_repomd_free(repomd_obj);

    if (repomd_xml) {
        gchar *repomd_path = g_strconcat(cmd_options->tmp_out_repo,
                                         "repomd.xml",
                                         NULL);
        FILE *frepomd = fopen(repomd_path, "w");
        if (frepomd) {
            fputs(repomd_xml, frepomd);
            fclose(frepomd);
        } else
            g_critical("Cannot open file: %s", repomd_path);
        g_free(repomd_path);
    } else
        g_critical("Generate of repomd.xml failed");


    // Move files from out_repo into tmp_out_repo

    g_debug("Moving data from %s", cmd_options->out_repo);
    if (g_file_test(cmd_options->out_repo, G_FILE_TEST_EXISTS)) {

        // Delete old metadata
        g_debug("Removing old metadata from %s", cmd_options->out_repo);
        cr_remove_metadata_classic(cmd_options->out_dir, 0, NULL);

        // Move files from out_repo to tmp_out_repo
        GDir *dirp;
        dirp = g_dir_open (cmd_options->out_repo, 0, NULL);
        if (!dirp) {
            g_critical("Cannot open directory: %s", cmd_options->out_repo);
            exit(1);
        }

        const gchar *filename;
        while ((filename = g_dir_read_name(dirp))) {
            gchar *full_path = g_strconcat(cmd_options->out_repo, filename, NULL);
            gchar *new_full_path = g_strconcat(cmd_options->tmp_out_repo, filename, NULL);

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
        if (g_rmdir(cmd_options->out_repo) == -1) {
            g_critical("Cannot remove %s", cmd_options->out_repo);
        } else {
            g_debug("Old out repo %s removed", cmd_options->out_repo);
        }
    }


    // Rename tmp_out_repo to out_repo
    if (g_rename(cmd_options->tmp_out_repo, cmd_options->out_repo) == -1) {
        g_critical("Cannot rename %s -> %s", cmd_options->tmp_out_repo, cmd_options->out_repo);
    } else {
        g_debug("Renamed %s -> %s", cmd_options->tmp_out_repo, cmd_options->out_repo);
    }


    // Clean up

    g_free(repomd_xml);

    g_free(pri_xml_filename);
    g_free(fil_xml_filename);
    g_free(oth_xml_filename);
    g_free(update_info_filename);


    return 1;
}



int
main(int argc, char **argv)
{
    _cleanup_error_free_ GError *tmp_err = NULL;

    // Parse arguments

    struct CmdOptions *cmd_options;
    cmd_options = parse_arguments(&argc, &argv);
    if (!cmd_options) {
        return 1;
    }


    // Set logging

    cr_setup_logging(FALSE, cmd_options->verbose);


    // Check arguments

    if (!check_arguments(cmd_options)) {
        free_options(cmd_options);
        return 1;
    }

    if (cmd_options->version) {
        printf("Version: %s\n", cr_version_string_with_features());
        free_options(cmd_options);
        exit(0);
    }

    if (g_slist_length(cmd_options->repo_list) < 1) {
        free_options(cmd_options);
        g_printerr("Usage: %s [OPTION...] --repo=url --repo=url\n\n"
                   "%s: take 2 or more repositories and merge their "
                   "metadata into a new repo\n\n",
                   cr_get_filename(argv[0]), cr_get_filename(argv[0]));
        return 1;
    }

    g_debug("Version: %s", cr_version_string_with_features());

    // Prepare out_repo

    if (g_file_test(cmd_options->tmp_out_repo, G_FILE_TEST_EXISTS)) {
        g_critical("Temporary repodata directory: %s already exists! ("
                    "Another createrepo process is running?)", cmd_options->tmp_out_repo);
        free_options(cmd_options);
        return 1;
    }

    if (g_mkdir_with_parents (cmd_options->tmp_out_repo, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
        g_critical("Error while creating temporary repodata directory %s: %s",
                    cmd_options->tmp_out_repo, g_strerror(errno));

        free_options(cmd_options);
        return 1;
    }


    // Download repos

    GSList *local_repos = NULL;
    GSList *element = NULL;
    gchar *groupfile = NULL;
    gboolean cr_download_failed = FALSE;

    for (element = cmd_options->repo_list; element; element = g_slist_next(element)) {
        struct cr_MetadataLocation *loc = cr_locate_metadata((gchar *) element->data, TRUE, NULL);
        if (!loc) {
            g_warning("Downloading of repodata failed: %s", (gchar *) element->data);
            cr_download_failed = TRUE;
            break;
        }
        local_repos = g_slist_prepend(local_repos, loc);
    }

    if (cr_download_failed) {
        // Remove downloaded metadata and free structures
        for (element = local_repos; element; element = g_slist_next(element)) {
            struct cr_MetadataLocation *loc = (struct cr_MetadataLocation  *) element->data;
            cr_metadatalocation_free(loc);
        }
        return 1;
    }


    // Groupfile
    // XXX: There must be a better logic
    if (!cmd_options->groupfile) {
        // Use first groupfile you find
        for (element = local_repos; element; element = g_slist_next(element)) {
            struct cr_MetadataLocation *loc;
            loc = (struct cr_MetadataLocation  *) element->data;
            if (!groupfile){
                if (loc->additional_metadata){
                    GSList *loc_groupfile = (g_slist_find_custom(loc->additional_metadata, "group", cr_cmp_metadatum_type));
                    if (loc_groupfile) {
                        cr_Metadatum *g = loc_groupfile->data;
                        if (cr_copy_file(g->name, cmd_options->tmp_out_repo, &tmp_err)) {
                            groupfile = g_strconcat(cmd_options->tmp_out_repo,
                                    cr_get_filename(g->name),
                                    NULL);
                            g_debug("Using groupfile: %s", groupfile);
                            break;
                      } else {
                            g_warning("Groupfile %s from repo: %s cannot be used: %s\n",
                                    g->name, loc->original_url, tmp_err->message);
                            g_clear_error(&tmp_err);
                        }
                    }
                }
            }
        }
    } else {
        // Use groupfile specified by user
        if (cr_copy_file(cmd_options->groupfile, cmd_options->tmp_out_repo, &tmp_err)) {
            groupfile = g_strconcat(cmd_options->tmp_out_repo,
                                    cr_get_filename(cmd_options->groupfile),
                                    NULL);
            g_debug("Using user specified groupfile: %s", groupfile);
        } else {
            g_critical("Cannot copy groupfile %s: %s",
                       cmd_options->groupfile, tmp_err->message);
            return 1;
        }
    }

    // Load noarch repo

    cr_Metadata *noarch_metadata = NULL;
    // cr_metadata_hashtable(noarch_metadata):
    //   Key: CR_HT_KEY_FILENAME aka pkg->location_href
    //   Value: package

    if (cmd_options->noarch_repo_url) {
        struct cr_MetadataLocation *noarch_ml;

        noarch_ml = cr_locate_metadata(cmd_options->noarch_repo_url, TRUE, NULL);
        if (!noarch_ml) {
            g_critical("Cannot locate noarch repo: %s", cmd_options->noarch_repo_url);
            return 1;
        }

        noarch_metadata = cr_metadata_new(CR_HT_KEY_FILENAME, 0, NULL);

        // Base paths in output of original createrepo doesn't have trailing '/'
        gchar *noarch_repopath = cr_normalize_dir_path(noarch_ml->original_url);
        if (noarch_repopath && strlen(noarch_repopath) > 1) {
            noarch_repopath[strlen(noarch_repopath)-1] = '\0';
        }

        g_debug("Loading noarch_repo: %s", noarch_repopath);

        if (cr_metadata_load_xml(noarch_metadata, noarch_ml, NULL) != CRE_OK) {
            g_critical("Cannot load noarch repo: \"%s\"", noarch_ml->repomd);
            cr_metadata_free(noarch_metadata);
            // TODO cleanup
            cr_metadatalocation_free(noarch_ml);
            return 1;
        }

        // Fill basepath - set proper base path for all packages in noarch hastable
        GHashTableIter iter;
        gpointer p_key, p_value;

        g_hash_table_iter_init (&iter, cr_metadata_hashtable(noarch_metadata));
        while (g_hash_table_iter_next (&iter, &p_key, &p_value)) {
            cr_Package *pkg = (cr_Package *) p_value;
            if (!pkg->location_base)
                pkg->location_base = g_string_chunk_insert(pkg->chunk,
                                                           noarch_repopath);
        }

        g_free(noarch_repopath);
        cr_metadatalocation_free(noarch_ml);
    }


    // Prepare Koji stuff if needed

    struct KojiMergedReposStuff *koji_stuff = NULL;
    if (cmd_options->koji)
        koji_stuff_prepare(&koji_stuff, cmd_options, local_repos);
    else if (cmd_options->pkgorigins)
        pkgorigins_prepare(&koji_stuff, cmd_options->tmp_out_repo);


    // Load metadata

    long loaded_packages;
    GHashTable *merged_hashtable = new_merged_metadata_hashtable();
    // merged_hashtable:
    //   Key: pkg->name
    //   Value: GSList with packages with the same name
#ifdef WITH_LIBMODULEMD
    g_autoptr(ModulemdModuleIndex) merged_index = NULL;
#endif

    loaded_packages = merge_repos(merged_hashtable,
#ifdef WITH_LIBMODULEMD
                                  &merged_index,
#endif /* WITH_LIBMODULEMD */
                                  local_repos,
                                  cmd_options->arch_list,
                                  cmd_options->merge_method,
                                  noarch_metadata ?
                                        cr_metadata_hashtable(noarch_metadata)
                                      : NULL,
                                  koji_stuff,
                                  cmd_options->omit_baseurl,
                                  cmd_options->repo_prefix_search,
                                  cmd_options->repo_prefix_replace
                                 );


    // Destroy koji stuff - we have to close pkgorigins file before dump

    if (cmd_options->koji || cmd_options->pkgorigins)
        koji_stuff_destroy(&koji_stuff);


    if(loaded_packages >= 0) {
        // Dump metadata
        dump_merged_metadata(merged_hashtable,
                loaded_packages,
                groupfile,
#ifdef WITH_LIBMODULEMD
                merged_index,
#endif
                cmd_options);
    }


    // Remove downloaded repos and free repo location structures

    for (element = local_repos; element; element = g_slist_next(element)) {
        struct cr_MetadataLocation *loc = (struct cr_MetadataLocation  *) element->data;
        cr_metadatalocation_free(loc);
    }

    g_slist_free (local_repos);

    // Cleanup

    g_free(groupfile);
    cr_metadata_free(noarch_metadata);
    destroy_merged_metadata_hashtable(merged_hashtable);
    free_options(cmd_options);
    return loaded_packages >= 0 ? 0 : 1;
}
