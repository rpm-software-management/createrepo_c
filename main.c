#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include "constants.h"
#include "parsepkg.h"


struct pool_user_data {
    FILE *pri_f;
    FILE *fil_f;
    FILE *oth_f;
    int changelog_limit;
//  char *repo_path;
    char *location_base;
    int repodir_name_len;
//  char *location_href; // Dostaneme pri pushu nove ulohy
    ChecksumType checksum_type;
    gboolean verbose;

    // Update stuff
    gboolean skip_stat;
    GHashTable *old_metadata;
};


struct thread_data {
    char *filename; // alias package location_href
};


int check_file(char *filename) {
    return 1;
}


void dumper_thread(gpointer data, gpointer user_data) {
    struct pool_user_data *udata = (struct pool_user_data *) user_data;

//    if (udata->verbose) {
//        printf("Processing: %s\n", data);
//    }

    // location_href without leading part of path (path to repo)
    char *location_href =  (gchar *) data + udata->repodir_name_len + 1;

    struct XmlStruct res;
    res = xml_from_package_file(data, udata->checksum_type, location_href,
                                udata->location_base, udata->changelog_limit, NULL);

    // Write primary data
    flockfile(udata->pri_f);
    fputs(res.primary, udata->pri_f);
    funlockfile(udata->pri_f);

    // Write fielists data
    flockfile(udata->fil_f);
    fputs(res.filelists, udata->fil_f);
    funlockfile(udata->fil_f);

    // Write other data
    flockfile(udata->oth_f);
    fputs(res.other, udata->oth_f);
    funlockfile(udata->oth_f);

    // Clean up
    free(res.primary);
    free(res.filelists);
    free(res.other);
    g_free(data);

    return;
}


#define MAX_THREADS     5

#define XML_COMMON_NS           "http://linux.duke.edu/metadata/common"
#define XML_FILELISTS_NS        "http://linux.duke.edu/metadata/filelists"
#define XML_OTHER_NS            "http://linux.duke.edu/metadata/other"
#define XML_RPM_NS              "http://linux.duke.edu/metadata/rpm"


int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Malo argumentu\n");
        return 1;
    }

    // Init package parser
    init_package_parser();

    // Thread pool
    struct pool_user_data user_data;
    user_data.pri_f = fopen("aaa_pri.xml", "w");
    user_data.fil_f = fopen("aaa_fil.xml", "w");
    user_data.oth_f = fopen("aaa_oth.xml", "w");
    user_data.changelog_limit = 5;
    user_data.location_base = "";
    user_data.checksum_type = PKG_CHECKSUM_SHA256;
    user_data.verbose = TRUE;
    user_data.skip_stat = FALSE;
    user_data.old_metadata = NULL;

    g_thread_init(NULL);
    GThreadPool *pool = g_thread_pool_new(dumper_thread, &user_data, MAX_THREADS, TRUE, NULL);

    // Write XML header
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<metadata xmlns=\""XML_COMMON_NS"\" xmlns:rpm=\""XML_RPM_NS"\" packages=\"@@@@@@@@@@\">\n",
          user_data.pri_f);
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<filelists xmlns=\""XML_FILELISTS_NS"\" packages=\"@@@@@@@@@@\">\n",
          user_data.fil_f);
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<otherdata xmlns=\""XML_OTHER_NS"\" packages=\"@@@@@@@@@@\">\n",
          user_data.oth_f);

    // Recursivqe walk
    int package_count = 0;

    GStringChunk *sub_dirs_chunk = g_string_chunk_new(1024);
    GQueue *sub_dirs = g_queue_new();

    // Get dir param without trailing "/"
    char *input_dir = argv[1];
    int arg_len = strlen(input_dir);
    int x;
    for (x=(arg_len-1); x >= 0; x--) {
        if (input_dir[x] != '/') {
            break;
        }
    }
    gchar *input_dir_stripped = g_string_chunk_insert_len(sub_dirs_chunk, input_dir, (x+1));
    user_data.repodir_name_len = (x+1);

    g_queue_push_head(sub_dirs, input_dir_stripped);

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

            // Add file into pool
            g_thread_pool_push(pool, full_path, NULL);
            package_count++;
            //g_free(full_path);
        }

        // Cleanup
        g_dir_close (dirp);
    }

    g_string_chunk_free (sub_dirs_chunk);
    g_queue_free(sub_dirs);

    // Wait until pool is finished
    g_thread_pool_free(pool, FALSE, TRUE);

    fputs("</metadata>\n",  user_data.pri_f);
    fputs("</filelists>\n", user_data.fil_f);
    fputs("</otherdata>\n", user_data.oth_f);

    // TODO: PREDELAT!!!!
    fseek(user_data.pri_f, 152, SEEK_SET);
    fprintf(user_data.pri_f, "%010d", package_count);
    fseek(user_data.fil_f, 109, SEEK_SET);
    fprintf(user_data.fil_f, "%010d", package_count);
    fseek(user_data.oth_f, 105, SEEK_SET);
    fprintf(user_data.oth_f, "%010d", package_count);

    fclose(user_data.pri_f);
    fclose(user_data.fil_f);
    fclose(user_data.oth_f);

    free_package_parser();

    return 0;
}
