#include <glib.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "compression_wrapper.h"
#include "misc.h"
#include "load_metadata.h"
#include "package.h"
#include "xml_dump.h"
#include "repomd.h"


#define VERSION         "0.1"
#define G_LOG_DOMAIN    ((gchar*) 0)

#define DEFAULT_OUTPUTDIR               "merged_repo/"
#define DEFAULT_COMPRESSION_TYPE        GZ_COMPRESSION



struct CmdOptions {

    // Items filled by cmd option parser

    gboolean version;
    char **repos;
    char *archlist;
    gboolean database;
    gboolean no_database;
    gboolean verbose;
    char *outputdir;
    char *outputrepo;
    gboolean nogroups;
    gboolean noupdateinfo;
    char *compress_type;

    // Items filled by check_arguments()

    char *out_dir;
    char *out_repo;
    GSList *repo_list;
    GSList *arch_list;
    CompressionType compression_type;

};


struct CmdOptions _cmd_options = {
        .compression_type = GZ_COMPRESSION
    };


static GOptionEntry cmd_entries[] =
{
    { "version", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.version), "Show program's version number and exit", NULL },
    { "repo", 'r', 0, G_OPTION_ARG_FILENAME_ARRAY, &(_cmd_options.repos), "Repo url", "REPOS" },
    { "archlist", 'a', 0, G_OPTION_ARG_STRING, &(_cmd_options.archlist), "Defaults to all arches - otherwise specify arches", "ARCHLIST" },
    { "database", 'd', 0, G_OPTION_ARG_NONE, &(_cmd_options.database), "", NULL },
    { "no-database", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.no_database), "", NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &(_cmd_options.verbose), "", NULL },
    { "outputdir", 'o', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.outputdir), "Location to create the repository", "OUTPUTDIR" },
    { "nogroups", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.nogroups), "Do not merge group(comps) metadata", NULL },
    { "noupdateinfo", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.noupdateinfo), "Do not merge updateinfo metadata", NULL },
    { "compress-type", 0, 0, G_OPTION_ARG_STRING, &(_cmd_options.compress_type), "Which compression type to use", "COMPRESS_TYPE" },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};


gboolean check_arguments(struct CmdOptions *options)
{
    int x;
    gboolean ret = TRUE;

    if (options->outputdir){
        if (!g_file_test(options->outputdir, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR)) {
            g_warning("Specified outputdir \"%s\" is regular file.", options->outputdir);
            ret = FALSE;
        }
        options->out_dir = normalize_dir_path(options->outputdir);
    } else {
        options->out_dir = g_strdup(DEFAULT_OUTPUTDIR);
    }

    options->out_repo = g_strconcat(options->out_dir, "repodata/", NULL);

    // Process repos
    x = 0;
    options->repo_list = NULL;
    while (options->repos && options->repos[x] != NULL) {
        char *normalized = normalize_dir_path(options->repos[x]);
        if (normalized) {
            options->repo_list = g_slist_prepend(options->repo_list, normalized);
            if (!g_file_test(normalized, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
                g_critical("Path \"%s\" doesn't exists", normalized);
                ret = FALSE;
            } else {
                g_debug("Using repo: %s", normalized);
            }
        }
        x++;
    }

    // Process archlist
    options->arch_list = NULL;
    if (options->archlist) {
        x = 0;
        gchar **arch_set = g_strsplit_set(options->archlist, ",;", -1);
        while (arch_set && arch_set[x] != NULL) {
            gchar *arch = arch_set[x];
            if (arch[0] != '\0') {
                options->arch_list = g_slist_prepend(options->arch_list, (gpointer) g_strdup(arch));
            }
            x++;
        }
        g_strfreev(arch_set);
    }

    return ret;
}


struct CmdOptions *parse_arguments(int *argc, char ***argv)
{
    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new(": take 2 or more repositories and merge their metadata into a new repo");
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



void free_options(struct CmdOptions *options)
{
    g_free(options->outputdir);
    g_free(options->archlist);
    g_free(options->compress_type);

    g_strfreev(options->repos);
    g_free(options->out_dir);
    g_free(options->out_repo);

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



#define UNUSED(x) (void)(x) // Used to suppress compiler warning about unused param

void black_hole_log_function (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
    UNUSED(log_domain);
    UNUSED(log_level);
    UNUSED(message);
    UNUSED(user_data);
    return;
}



void free_merged_values(gpointer data)
{
    GSList *element = (GSList *) data;
    for (; element; element=g_slist_next(element)) {
        Package *pkg = (Package *) element->data;
        package_free(pkg);
    }
    g_slist_free((GSList *) data);
}



GHashTable *new_merged_metadata_hashtable()
{
    GHashTable *hashtable = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, free_merged_values); // TODO!!
    return hashtable;
}



void destroy_merged_metadata_hashtable(GHashTable *hashtable)
{
    if (hashtable) {
        g_hash_table_destroy(hashtable);
    }
}



// Merged table structure: {"package_name": [pkg, pkg, pkg, ...], ...}

int add_package(Package *pkg, gchar *repopath, GHashTable *merged, struct CmdOptions *cmd_options)
{
    GSList *list, *element;


    // Check if the package meet the command line architecture constraints

    if (cmd_options->arch_list) {
        gboolean right_arch = FALSE;
        for (element=cmd_options->arch_list; element; element=g_slist_next(element)) {
            if (!g_strcmp0(pkg->arch, (gchar *) element->data)) {
                right_arch = TRUE;
            }
        }

        if (!right_arch) {
            g_debug("Skip - %s (Bad arch: %s)", pkg->name, pkg->arch);
            return 0;
        }
    }


    // Lookup package in the merged

    list = (GSList *) g_hash_table_lookup(merged, pkg->name);


    // Key doesn't exist yet

    if (!list) {
        list = g_slist_prepend(list, pkg);
        pkg->location_base = repopath; // XXX: maybe insert into chunk
        g_hash_table_insert (merged, (gpointer) pkg->name, (gpointer) list);
        return 1;
    }


    // Check if package with the architecture isn't in the list already

    for (element=list; element; element=g_slist_next(element)) {
        Package *c_pkg = (Package *) element->data;
        if (!g_strcmp0(pkg->arch, c_pkg->arch)) {
            // Package with the same arch already exists
            g_debug("Package %s (%s) already exists", pkg->name, pkg->arch);
            return 0;
        }
    }


    // Add package

    list = g_slist_prepend(list, pkg);

    return 1;
}



long merge_repos(GHashTable *merged, struct CmdOptions *cmd_options) {

    long loaded_packages = 0;

    // Load all repos

    GSList *element = NULL;
    for (element = cmd_options->repo_list; element; element = g_slist_next(element)) {
        GHashTable *tmp_hashtable;
        gchar *repopath;

        tmp_hashtable = new_metadata_hashtable();
        repopath = (gchar *) element->data;
        g_debug("Processing: %s", repopath);

        if (locate_and_load_xml_metadata(tmp_hashtable, repopath, HT_KEY_HASH) == 0) {
            g_critical("Cannot load repo: \"%s\"", repopath);
            destroy_metadata_hashtable(merged);
            break;
        }

        GHashTableIter iter;
        gpointer key, value;
        guint original_size, used;

        original_size = g_hash_table_size(tmp_hashtable);

        g_hash_table_iter_init (&iter, tmp_hashtable);
        while (g_hash_table_iter_next (&iter, &key, &value)) {
            Package *pkg = (Package *) value;
            if (add_package(pkg, repopath, merged, cmd_options)) {
                // Package was added - remove only record from hashtable
                g_hash_table_iter_steal(&iter);
            } /* else {
                // Package was not added - remove record and data
                g_hash_table_iter_remove(&iter);
            } */
        }

        used = original_size - g_hash_table_size(tmp_hashtable);
        loaded_packages += used;
        destroy_metadata_hashtable(tmp_hashtable);
        g_debug("Repo: %s (Loaded: %ld Used: %ld)", repopath, (unsigned long) original_size, (unsigned long) used);
    }

    return loaded_packages;
}



int dump_merged_metadata(GHashTable *merged_hashtable, long packages, struct CmdOptions *cmd_options)
{
    // Create/Open output xml files

    CW_FILE *pri_f;
    CW_FILE *fil_f;
    CW_FILE *oth_f;

    gchar *pri_xml_filename = g_strconcat(cmd_options->out_repo, "/primary.xml.gz", NULL);
    gchar *fil_xml_filename = g_strconcat(cmd_options->out_repo, "/filelists.xml.gz", NULL);
    gchar *oth_xml_filename = g_strconcat(cmd_options->out_repo, "/other.xml.gz", NULL);

    if ((pri_f = cw_open(pri_xml_filename, CW_MODE_WRITE, GZ_COMPRESSION)) == NULL) {
        g_critical("Cannot open file: %s", pri_xml_filename);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        return 0;
    }

    if ((fil_f = cw_open(fil_xml_filename, CW_MODE_WRITE, GZ_COMPRESSION)) == NULL) {
        g_critical("Cannot open file: %s", fil_xml_filename);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        cw_close(pri_f);
        return 0;
    }

    if ((oth_f = cw_open(oth_xml_filename, CW_MODE_WRITE, GZ_COMPRESSION)) == NULL) {
        g_critical("Cannot open file: %s", oth_xml_filename);
        g_free(pri_xml_filename);
        g_free(fil_xml_filename);
        g_free(oth_xml_filename);
        cw_close(fil_f);
        cw_close(pri_f);
        return 0;
    }


    // Write xml headers

    cw_printf(pri_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<metadata xmlns=\""XML_COMMON_NS"\" xmlns:rpm=\""XML_RPM_NS"\" packages=\"%d\">\n", packages);
    cw_printf(fil_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<filelists xmlns=\""XML_FILELISTS_NS"\" packages=\"%d\">\n", packages);
    cw_printf(oth_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<otherdata xmlns=\""XML_OTHER_NS"\" packages=\"%d\">\n", packages);


    // Dump hashtable

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init (&iter, merged_hashtable);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
        GSList *element = (GSList *) value;
        for (; element; element=g_slist_next(element)) {
            struct XmlStruct res;
            Package *pkg;

            pkg = (Package *) element->data;
            res = xml_dump(pkg);

            cw_puts(pri_f, (const char *) res.primary);
            cw_puts(fil_f, (const char *) res.filelists);
            cw_puts(oth_f, (const char *) res.other);

            free(res.primary);
            free(res.filelists);
            free(res.other);
        }
    }


    // Write xml footers

    cw_puts(pri_f, "\n</metadata>\n");
    cw_puts(fil_f, "\n</filelists>\n");
    cw_puts(oth_f, "\n</otherdata>\n");


    // Close files

    cw_close(pri_f);
    cw_close(fil_f);
    cw_close(oth_f);


    // Gen repomd.xml

    gchar *pri_xml_name = g_strconcat("repodata/", "primary.xml.gz", NULL);
    gchar *fil_xml_name = g_strconcat("repodata/", "filelists.xml.gz", NULL);
    gchar *oth_xml_name = g_strconcat("repodata/", "other.xml.gz", NULL);

    struct repomdResult *repomd_res = xml_repomd(cmd_options->out_dir, 1, pri_xml_name, fil_xml_name, oth_xml_name, NULL, NULL, NULL, NULL, NULL);
    gchar *repomd_path = g_strconcat(cmd_options->out_repo, "repomd.xml", NULL);

    FILE *frepomd = fopen(repomd_path, "w");
    if (frepomd && repomd_res->repomd_xml) {
        fputs(repomd_res->repomd_xml, frepomd);
        fclose(frepomd);
    } else {
        g_critical("Generate of repomd.xml failed");
    }


    // Clean up

    g_free(repomd_path);
    g_free(pri_xml_name);
    g_free(fil_xml_name);
    g_free(oth_xml_name);

    g_free(pri_xml_filename);
    g_free(fil_xml_filename);
    g_free(oth_xml_filename);

    return 1;
}



int main(int argc, char **argv)
{
    // Parse arguments

    struct CmdOptions *cmd_options;
    cmd_options = parse_arguments(&argc, &argv);
    if (!cmd_options) {
        return 1;
    }


    // Check arguments

    if (!check_arguments(cmd_options)) {
        free_options(cmd_options);
        return 1;
    }

    if (g_slist_length(cmd_options->repo_list) < 2) {
        free_options(cmd_options);
        fprintf(stderr, "Usage: %s [options]\n\n"
                        "%s: take 2 or more repositories and merge their metadata into a new repo\n\n",
                        get_filename(argv[0]), get_filename(argv[0]));
        return 1;
    }


    // Set logging

    if (!cmd_options->verbose) {
        GLogLevelFlags levels = G_LOG_LEVEL_DEBUG;
        g_log_set_handler(NULL, levels, black_hole_log_function, NULL);
        g_log_set_handler("C_CREATEREPOLIB", levels, black_hole_log_function, NULL);
    }


    // Prepare out_repo

    if (g_file_test(cmd_options->out_repo, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        remove_dir(cmd_options->out_repo);
    }

    if(g_mkdir_with_parents(cmd_options->out_repo, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
        g_critical("Cannot create out_repo \"%s\" (%s)", cmd_options->out_repo, strerror(errno));
        free_options(cmd_options);
        return 1;
    }


    // Load metadata

    long loaded_packages;
    GHashTable *merged_hashtable = new_merged_metadata_hashtable();
    loaded_packages = merge_repos(merged_hashtable, cmd_options);


    // Dump metadata

    dump_merged_metadata(merged_hashtable, loaded_packages, cmd_options);


    // Cleanup

    destroy_merged_metadata_hashtable(merged_hashtable);
    free_options(cmd_options);
    return 0;
}
