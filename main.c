#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include "constants.h"
#include "parsepkg.h"
#include <zlib.h>
#include <fcntl.h>


#define DEFAULT_CHANGELOG_LIMIT         10
#define DEFAULT_CHECKSUM                "sha256"
#define DEFAULT_WORKERS                 5
#define DEFAULT_UNIQUE_MD_FILENAMES     TRUE


struct CmdOptions {

    // Items filled by cmd option parser

    char *location_base;        // Base URL location for all files
    char *outputdir;            // Output directory
    char **excludes;            // List of file globs to exclude
    char *pkglist;              // File with files to include
    char **includepkg;          // List of files to include
    gboolean quiet;             // Shut up!
    gboolean verbose;           // Verbosely more than usual
    gboolean update;            // Update repo if metadata already exists
    gboolean skip_stat;         // skip stat() call during --update
    gboolean version;           // Output version.
    gboolean database;          // Not implemented yet!!!
    gboolean no_database;       // Implemented :-P
    char *checksum;             // Type of checksum
    gboolean skip_symlinks;     // Ignore symlinks of packages
    int changelog_limit;
    gboolean unique_md_filenames; // Include the file's checksum in filename
    gboolean simple_md_filenames; // Simple checksum in filename
    int workers;                // Number of threads to spawn

    // Items filled by check_arguments()

    GSList *exclude_masks;
    GSList *include_pkgs;
    ChecksumType checksum_type;

} cmd_options = {   .changelog_limit = DEFAULT_CHANGELOG_LIMIT,
                    .checksum = NULL,
                    .workers =  DEFAULT_WORKERS,
                    .unique_md_filenames = DEFAULT_UNIQUE_MD_FILENAMES };


struct UserData {
    gzFile *pri_f;
    gzFile *fil_f;
    gzFile *oth_f;
    int changelog_limit;
    char *location_base;
    int repodir_name_len;
    ChecksumType checksum_type;
    gboolean quiet;
    gboolean verbose;
    gboolean skip_symlinks;

    // Update stuff
    gboolean skip_stat;
    GHashTable *old_metadata;
};


int allowed_file(char *filename, struct CmdOptions *options) {

    // Check file against exclude glob masks
    if (options->exclude_masks) {
        int str_len = strlen(filename);
        gchar *reversed_filename = g_utf8_strreverse(filename, str_len);

        GSList *element;
        for (element=options->exclude_masks; element; element=g_slist_next(element)) {
            if (g_pattern_match((GPatternSpec *) element->data, str_len, filename, reversed_filename)) {
                g_free(reversed_filename);
                return FALSE;
            }
            g_free(reversed_filename);
        }
    }

    // TODO: Hash table could be more effective
    // Check file against include_pkgs filenames
    if (options->include_pkgs) {
        GSList *element;
        for (element=options->include_pkgs; element; element=g_slist_next(element)) {
            if (g_strcmp0(filename, (char *) element->data)) {
                return TRUE;
            }
        }
        return FALSE;
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

    // location_href without leading part of path (path to repo)
    char *location_href =  (gchar *) data + udata->repodir_name_len + 1;

    struct XmlStruct res;
    res = xml_from_package_file(data, udata->checksum_type, location_href,
                                udata->location_base, udata->changelog_limit, NULL);

    // Write primary data
    G_LOCK(LOCK_PRI);
    gzputs(udata->pri_f, (char *) res.primary);
    G_UNLOCK(LOCK_PRI);

    // Write fielists data
    G_LOCK(LOCK_FIL);
    gzputs(udata->fil_f, (char *) res.filelists);
    G_UNLOCK(LOCK_FIL);

    // Write other data
    G_LOCK(LOCK_OTH);
    gzputs(udata->oth_f, (char *) res.other);
    G_UNLOCK(LOCK_OTH);

    // Clean up
    free(res.primary);
    free(res.filelists);
    free(res.other);
    g_free(data);

    return;
}


// Command line params

static GOptionEntry cmd_entries[] =
{
    { "baseurl", 'u', 0, G_OPTION_ARG_STRING, &(cmd_options.location_base),
      "Optional base URL location for all files.", "<URL>" },
    { "outputdir", 'o', 0, G_OPTION_ARG_STRING, &(cmd_options.outputdir),
      "Optional output directory", "<URL>" },
    { "excludes", 'x', 0, G_OPTION_ARG_STRING_ARRAY, &(cmd_options.excludes),
      "File globs to exclude, can be specified multiple times.", "<packages>" },
    { "pkglist", 'i', 0, G_OPTION_ARG_FILENAME, &(cmd_options.pkglist),
      "specify a text file which contains the complete list of files to include"
      " in the  repository  from the set found in the directory. File format is"
      " one package per line, no wildcards or globs.", "<filename>" },
    { "includepkg", 'n', 0, G_OPTION_ARG_FILENAME_ARRAY, &(cmd_options.includepkg),
      "specify pkgs to include on the command line. Takes urls as well as local paths.",
      "<packages>" },
    { "quiet", 'q', 0, G_OPTION_ARG_NONE, &(cmd_options.quiet),
      "Run quietly.", NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &(cmd_options.verbose),
      "Run verbosely.", NULL },
    { "update", 0, 0, G_OPTION_ARG_NONE, &(cmd_options.update),
      "If  metadata already exists in the outputdir and an rpm is unchanged (based on file size "
      "and mtime) since the metadata was generated, reuse the  existing  metadata  rather  than "
      "recalculating it. In the case of a large repository with only a few new or modified rpms"
      "this can significantly reduce I/O and processing time.", NULL },
    { "skip-stat", 0, 0, G_OPTION_ARG_NONE, &(cmd_options.skip_stat),
      "skip the stat() call on a --update, assumes if the filename is the same then the file is"
      "still the same (only use this if you're fairly trusting or gullible).", NULL },
    { "version", 'V', 0, G_OPTION_ARG_NONE, &(cmd_options.version),
      "Output version.", NULL},
    { "database", 'd', 0, G_OPTION_ARG_NONE, &(cmd_options.database),
      "Generate sqlite databases for use with yum. NOT IMPLEMENTED!", NULL },
    { "no-database", 0, 0, G_OPTION_ARG_NONE, &(cmd_options.no_database),
      "Do not generate sqlite databases in the repository.", NULL },
    { "skip-symlinks", 'S', 0, G_OPTION_ARG_NONE, &(cmd_options.skip_symlinks),
      "Ignore symlinks of packages", NULL},
    { "checksum", 's', 0, G_OPTION_ARG_STRING, &(cmd_options.checksum),
      "Choose the checksum type used in repomd.xml and  for  packages  in  the  metadata."
      "The default  is  now \"sha256\".", "<checksum_type>" },
    { "changelog-limit", 0, 0, G_OPTION_ARG_INT, &(cmd_options.changelog_limit),
      "Only import the last N changelog entries, from each rpm, into the metadata.",
      "<number>" },
    { "unique-md-filenames", 0, 0, G_OPTION_ARG_NONE, &(cmd_options.unique_md_filenames),
      "Include the file's checksum in the metadata filename, helps HTTP caching (default)",
      NULL },
    { "simple-md-filenames", 0, 0, G_OPTION_ARG_NONE, &(cmd_options.simple_md_filenames),
      "Do not include the file's checksum in the metadata filename.", NULL },
    { "workers", 0, 0, G_OPTION_ARG_INT, &(cmd_options.workers),
      "number of workers to spawn to read rpms.", NULL },
    { NULL }
};


gboolean check_arguments(struct CmdOptions *options)
{
    // Check outputdir
    if (options->outputdir && !g_file_test(options->outputdir, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        printf("Specified outputdir doesn't exists\n");
        return FALSE;
    }

    // Check workers
    if ((options->workers < 1) || (options->workers > 100)) {
        printf("Warning: Wrong number of workers - Using 5 workers.");
        options->workers = DEFAULT_WORKERS;
    }

    // Check changelog_limit
    if ((options->changelog_limit < 0) || (options->changelog_limit > 100)) {
        printf("Warning: Wrong changelog limit - Using 10");
        options->changelog_limit = DEFAULT_CHANGELOG_LIMIT;
    }

    // Check and set checksum type
    if (options->checksum) {
        GString *checksum_str = g_string_ascii_down(g_string_new(options->checksum));
        if (!strcmp(checksum_str->str, "sha256")) {
            options->checksum_type = PKG_CHECKSUM_SHA256;
        } else if (!strcmp(checksum_str->str, "sha1")) {
            options->checksum_type = PKG_CHECKSUM_SHA1;
        } else if (!strcmp(checksum_str->str, "md5")) {
            options->checksum_type = PKG_CHECKSUM_MD5;
        } else {
            g_string_free(checksum_str, TRUE);
            printf("Error: Unknown/Unsupported checksum type");
            return FALSE;
        }
        g_string_free(checksum_str, TRUE);
    } else {
        options->checksum_type = PKG_CHECKSUM_SHA256;
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

    // Process pkglist file
    if (!options->pkglist) {
        return TRUE;
    }

    if (!g_file_test(options->pkglist, G_FILE_TEST_IS_REGULAR|G_FILE_TEST_EXISTS)) {
        printf("Warning: pkglist file doesn't exists\n");
        return TRUE;
    }

    char *content;
    GError *err;
    if (!g_file_get_contents(options->pkglist, &content, NULL, &err)) {
        printf("Warning: Error while reading pkglist file: %s\n", err->message);
        g_error_free(err);
        g_free(content);
        return TRUE;
    }

    x = 0;
    char **pkgs = g_strsplit(content, "\n", 0);
    while (pkgs && pkgs[x] != NULL) {
        options->include_pkgs = g_slist_prepend(options->include_pkgs, (gpointer) pkgs[x]);
        x++;
    }

    g_free(pkgs);  // Free pkgs array, pointers from array are already stored in include_pkgs list
    g_free(content);

    return TRUE;
}



void free_options(struct CmdOptions *options)
{
    g_free(options->location_base);
    g_free(options->outputdir);
    g_free(options->pkglist);
    g_free(options->checksum);

    // Free excludes string list
    int x = 0;
    while (options->excludes && options->excludes[x] != NULL) {
        g_free(options->excludes[x]);
        options->excludes[x] = NULL;
    }

    // inludepkg string list MUST NOT be freed!
    // Items from the list are referenced by include_pkgs GSList and will be
    // freed from them.

    GSList *element = NULL;

    // Free include_pkgs GSList
    for (element = options->include_pkgs; element; element = g_slist_next(element)) {
        g_free( (gchar *) element->data );
    }

    // Free glob exclude masks GSList
    for (element = options->exclude_masks; element; element = g_slist_next(element)) {
        g_pattern_spec_free( (GPatternSpec *) element->data );
    }
}



#define MAX_THREADS     5
#define GZ_BUFFER_SIZE  8192

#define XML_COMMON_NS           "http://linux.duke.edu/metadata/common"
#define XML_FILELISTS_NS        "http://linux.duke.edu/metadata/filelists"
#define XML_OTHER_NS            "http://linux.duke.edu/metadata/other"
#define XML_RPM_NS              "http://linux.duke.edu/metadata/rpm"


int main(int argc, char **argv) {


    // Arguments parsing

    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new("- program that creates a repomd (xml-based rpm metadata) repository from a set of rpms.");
    g_option_context_add_main_entries(context, cmd_entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("Option parsing failed: %s\n", error->message);
        exit(1);
    }

/* // -------------- DEBUG STUFF
    puts("Argument list - Start");
    int ix;
    printf("baseurl: %s\n", cmd_options.location_base);
    printf("outputdir: %s\n", cmd_options.outputdir);
    ix = 0;
    puts("excludes:");
    while (cmd_options.excludes && cmd_options.excludes[ix] != NULL) {
        printf("  %2d) %s\n", ix, cmd_options.excludes[ix]);
        ix++;
    }
    printf("pkglist: %s\n", cmd_options.pkglist);
    ix = 0;
    puts("includepkg:");
    while (cmd_options.includepkg && cmd_options.includepkg[ix] != NULL) {
        printf("  %2d) %s\n", ix, cmd_options.includepkg[ix]);
        ix++;
    }
    printf("quiet: %d\n", cmd_options.quiet);
    printf("verbose: %d\n", cmd_options.verbose);
    printf("update: %d\n", cmd_options.update);
    printf("skip_stat: %d\n", cmd_options.skip_stat);
    printf("version: %d\n", cmd_options.version);
    printf("database: %d\n", cmd_options.database);
    printf("no-database: %d\n", cmd_options.no_database);
    printf("checksum: %s\n", cmd_options.checksum);
    printf("skip_symlinks: %d\n", cmd_options.skip_symlinks);
    printf("changelog_limit: %d\n", cmd_options.changelog_limit);
    printf("unique_md_filenames: %d\n", cmd_options.unique_md_filenames);
    printf("simple_md_filenames: %d\n", cmd_options.simple_md_filenames);
    printf("workers: %d\n", cmd_options.workers);
    puts("Arguments list - End");
// ---------- DEBUG STUFF END */


    // Process parsed arguments

    if (!check_arguments(&cmd_options)) {
        free_options(&cmd_options);
        g_option_context_free(context);
        exit(1);
    }

    g_option_context_free(context);

    if (argc != 2) {
        printf("Must specify exactly one directory to index.");
        free_options(&cmd_options);
        exit(1);
    }


    // Prepare dirs

    gchar *in_repo = argv[1];
    gchar *out_repo = NULL;

    if (cmd_options.outputdir) {
        out_repo = g_strconcat(cmd_options.outputdir, "/repodata/", NULL);
    } else {
        out_repo = g_strconcat(in_repo, "/repodata/", NULL);
    }

    if (!g_file_test(out_repo, G_FILE_TEST_EXISTS)) {
        if (g_mkdir (out_repo, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
            perror("Error while creating repodata directory");
            exit(1);
        }
    } else {
        if (!g_file_test(out_repo, G_FILE_TEST_IS_DIR)) {
            printf("Error repodata already exists and it is not a directory!");
            exit(1);
        }
    }


    // Prepare new xml files in temp

    gchar *pri_xml_filename = g_strconcat(out_repo, "/_primary.xml.gz");
    //int fd_pri_xml = g_file_open_tmp("XXXXXX", &pri_xml_filename, NULL);
    //int fd_pri_xml = open(pri_xml_filename, O_RDWR);
    gzFile pri_gz_file = gzopen(pri_xml_filename, "wb");
    gzsetparams(pri_gz_file, Z_BEST_SPEED, Z_DEFAULT_STRATEGY);
    gzbuffer(pri_gz_file, GZ_BUFFER_SIZE);

    gchar *fil_xml_filename = g_strconcat(out_repo, "/_filelists.xml.gz");
    //int fd_fil_xml = g_file_open_tmp("XXXXXX", &fil_xml_filename, NULL);
    //int fd_fil_xml = open(fil_xml_filename, O_RDWR);
    gzFile fil_gz_file = gzopen(fil_xml_filename, "wb");
    gzsetparams(fil_gz_file, Z_BEST_SPEED, Z_DEFAULT_STRATEGY);
    gzbuffer(fil_gz_file, GZ_BUFFER_SIZE);

    gchar *oth_xml_filename = g_strconcat(out_repo, "/_other.xml.gz");
    //int fd_oth_xml = g_file_open_tmp("XXXXXX", &oth_xml_filename, NULL);
    //int fd_oth_xml = open(oth_xml_filename, O_RDWR);
    gzFile oth_gz_file = gzopen(oth_xml_filename, "wb");
    gzsetparams(oth_gz_file, Z_BEST_SPEED, Z_DEFAULT_STRATEGY);
    gzbuffer(oth_gz_file, GZ_BUFFER_SIZE);



    puts("Using tmp files:");
    puts(pri_xml_filename);
    puts(fil_xml_filename);
    puts(oth_xml_filename);

    // Load old metadata if --update

    // TODO

    // Init package parser

    init_package_parser();


    // Thread pool - User data initialization

    struct UserData user_data;
    user_data.pri_f           = pri_gz_file;
    user_data.fil_f           = fil_gz_file;
    user_data.oth_f           = oth_gz_file;
    user_data.changelog_limit = cmd_options.changelog_limit;
    user_data.location_base   = cmd_options.location_base;
    user_data.checksum_type   = cmd_options.checksum_type;
    user_data.quiet           = cmd_options.quiet;
    user_data.verbose         = cmd_options.verbose;
    user_data.skip_symlinks   = cmd_options.skip_symlinks;
    user_data.skip_stat       = cmd_options.skip_stat;
    user_data.old_metadata    = NULL;

    puts("Thread pool user data ready");

    // Thread pool - Creation

    g_thread_init(NULL);
    GThreadPool *pool = g_thread_pool_new(dumper_thread, &user_data, 0, TRUE, NULL);

    puts("Thread pool ready");

    // Recursive walk

    int package_count = 0;

    GStringChunk *sub_dirs_chunk = g_string_chunk_new(1024);
    GQueue *sub_dirs = g_queue_new();

    // Get dir param without trailing "/"
    int arg_len = strlen(in_repo);
    int x;
    for (x=(arg_len-1); x >= 0; x--) {
        if (in_repo[x] != '/') {
            break;
        }
    }
    gchar *input_dir_stripped = g_string_chunk_insert_len(sub_dirs_chunk, in_repo, (x+1));
    user_data.repodir_name_len = (x+1);

    g_queue_push_head(sub_dirs, input_dir_stripped);

    puts("Walk starting");

    char *dirname;
    while (dirname = g_queue_pop_head(sub_dirs)) {
        // Open dir
        GDir *dirp;
        dirp = g_dir_open (dirname, 0, NULL);
        if (!dirp) {
            puts("Cannot open directory");
            return 1;
        }

        const gchar *filename;
        while (filename = g_dir_read_name(dirp)) {
            gchar *full_path = g_strconcat(dirname, "/", filename, NULL);
            if (!g_file_test(full_path, G_FILE_TEST_IS_REGULAR)) {
                if (g_file_test(full_path, G_FILE_TEST_IS_DIR)) {
                    gchar *sub_dir_in_chunk = g_string_chunk_insert (sub_dirs_chunk, full_path);
                    g_queue_push_head(sub_dirs, sub_dir_in_chunk);
                }
                g_free(full_path);
                continue;
            }

            // Skip non .rpm files
            if (!g_str_has_suffix (filename, ".rpm")) {
                g_free(full_path);
                continue;
            }

            // Check filename against exclude glob masks, pkglist and includepkg values
            if (allowed_file, &cmd_options) {
                // FINALLY! Add file into pool
                g_thread_pool_push(pool, full_path, NULL);
                package_count++;
            }
        }

        // Cleanup
        g_dir_close (dirp);
    }

    puts("Walk done");

    g_string_chunk_free (sub_dirs_chunk);
    g_queue_free(sub_dirs);

    // Write XML header
    gzprintf(user_data.pri_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<metadata xmlns=\""XML_COMMON_NS"\" xmlns:rpm=\""XML_RPM_NS"\" packages=\"%d\">\n", package_count);
    gzprintf(user_data.fil_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<filelists xmlns=\""XML_FILELISTS_NS"\" packages=\"%d\">\n", package_count);
    gzprintf(user_data.oth_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<otherdata xmlns=\""XML_OTHER_NS"\" packages=\"%d\">\n", package_count);

    // Start pool
    g_thread_pool_set_max_threads(pool, MAX_THREADS, NULL);

    // Wait until pool is finished
    g_thread_pool_free(pool, FALSE, TRUE);

    gzputs(user_data.pri_f, "</metadata>\n");
    gzputs(user_data.fil_f, "</filelists>\n");
    gzputs(user_data.oth_f, "</otherdata>\n");

    gzclose_w(user_data.pri_f);
    gzclose_w(user_data.fil_f);
    gzclose_w(user_data.oth_f);

    // Move files from tmp
/*
    if (rename(pri_xml_filename, "primary.xml.gz")) {
        perror("Error renaming file");
    }
    if (rename(fil_xml_filename, "filelists.xml.gz")) {
        perror("Error renaming file");
    }
    if (rename(oth_xml_filename, "other.xml.gz")) {
        perror("Error renaming file");
    }
*/

    // Clean up

    g_free(out_repo);
    g_free(pri_xml_filename);
    g_free(fil_xml_filename);
    g_free(oth_xml_filename);

    free_options(&cmd_options);
    free_package_parser();

    return 0;
}
