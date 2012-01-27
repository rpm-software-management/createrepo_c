#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include "constants.h"
#include "parsepkg.h"
#include <zlib.h>


struct pool_user_data {
    gzFile *pri_f;
    gzFile *fil_f;
    gzFile *oth_f;
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


#define LOCK_PRI        0
#define LOCK_FIL        1
#define LOCK_OTH        2

G_LOCK_DEFINE (LOCK_PRI);
G_LOCK_DEFINE (LOCK_FIL);
G_LOCK_DEFINE (LOCK_OTH);


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


#define MAX_THREADS     5
#define GZ_BUFFER_SIZE  8192

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
    user_data.pri_f = gzopen("aaa_pri.xml.gz", "wb");
    gzbuffer(user_data.pri_f, GZ_BUFFER_SIZE);
    gzsetparams(user_data.pri_f, Z_BEST_SPEED, Z_DEFAULT_STRATEGY);
    user_data.fil_f = gzopen("aaa_fil.xml.gz", "wb");
    gzbuffer(user_data.fil_f, GZ_BUFFER_SIZE);
    gzsetparams(user_data.fil_f, Z_BEST_SPEED, Z_DEFAULT_STRATEGY);
    user_data.oth_f = gzopen("aaa_oth.xml.gz", "wb");
    gzbuffer(user_data.oth_f, GZ_BUFFER_SIZE);
    gzsetparams(user_data.oth_f, Z_BEST_SPEED, Z_DEFAULT_STRATEGY);
    user_data.changelog_limit = 5;
    user_data.location_base = "";
    user_data.checksum_type = PKG_CHECKSUM_SHA256;
    user_data.verbose = TRUE;
    user_data.skip_stat = FALSE;
    user_data.old_metadata = NULL;

    g_thread_init(NULL);
    GThreadPool *pool = g_thread_pool_new(dumper_thread, &user_data, MAX_THREADS, TRUE, NULL);

    // Write XML header
    gzputs(user_data.pri_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<metadata xmlns=\""XML_COMMON_NS"\" xmlns:rpm=\""XML_RPM_NS"\" packages=\"@@@@@@@@@@\">\n");
    gzputs(user_data.fil_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<filelists xmlns=\""XML_FILELISTS_NS"\" packages=\"@@@@@@@@@@\">\n");
    gzputs(user_data.oth_f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<otherdata xmlns=\""XML_OTHER_NS"\" packages=\"@@@@@@@@@@\">\n");

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

    gzputs(user_data.pri_f, "</metadata>\n");
    gzputs(user_data.fil_f, "</filelists>\n");
    gzputs(user_data.oth_f, "</otherdata>\n");

    // TODO: PREDELAT!!!!
/*    gzseek(user_data.pri_f, 152, SEEK_SET);
    gzprintf(user_data.pri_f, "%010d", package_count);
    gzseek(user_data.fil_f, 109, SEEK_SET);
    gzprintf(user_data.fil_f, "%010d", package_count);
    gzseek(user_data.oth_f, 105, SEEK_SET);
    gzprintf(user_data.oth_f, "%010d", package_count);
*/

    gzclose_w(user_data.pri_f);
    gzclose_w(user_data.fil_f);
    gzclose_w(user_data.oth_f);

    free_package_parser();

    return 0;
}
