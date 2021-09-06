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
#include "createrepo_shared.h"
#include "locate_metadata.h"
#include "load_metadata.h"
#include "package.h"
#include "repomd.h"
#include "sqlite.h"
#include "xml_file.h"
#include "modifyrepo_shared.h"
#include "threads.h"
#include "xml_dump.h"


#define DEFAULT_CHECKSUM    CR_CHECKSUM_SHA256

/**
 * Command line options
 */
typedef struct {

    /* Items filled by cmd option parser */

    gboolean version;           /*!< print program version */
    gboolean quiet;             /*!< quiet mode */
    gboolean verbose;           /*!< verbose mode */
    gboolean force;             /*!< overwrite existing DBs */
    gboolean keep_old;          /*!< keep old DBs around */
    gboolean xz_compression;    /*!< use xz for DBs compression */
    gchar *compress_type;       /*!< which compression type to use */
    gboolean local_sqlite;      /*!< gen sqlite locally into a directory
                                     temporary files. (For situations when
                                     sqlite has a trouble to gen DBs
                                     on NFS mounts.)*/
    gchar *chcksum_type;       /*!< type of checksum in repomd.xml */

    /* Items filled by check_sqliterepo_arguments() */

    cr_CompressionType compression_type;    /*!< compression type */
    cr_ChecksumType checksum_type;          /*!< checksum type */

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
    options->keep_old = FALSE;
    options->xz_compression = FALSE;
    options->compress_type = NULL;
    options->chcksum_type = NULL;
    options->local_sqlite = FALSE;
    options->compression_type = CR_CW_BZ2_COMPRESSION;
    options->checksum_type = CR_CHECKSUM_UNKNOWN;

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
          "Overwrite existing DBs.", NULL },
        { "keep-old", '\0', 0, G_OPTION_ARG_NONE, &(options->keep_old),
          "Do not remove old DBs. Use only with combination with --force.", NULL },
        { "xz", '\0', 0, G_OPTION_ARG_NONE, &(options->xz_compression),
          "Use xz for repodata compression.", NULL },
        { "compress-type", '\0', 0, G_OPTION_ARG_STRING, &(options->compress_type),
          "Which compression type to use.", "<compress_type>" },
        { "checksum", '\0', 0, G_OPTION_ARG_STRING, &(options->chcksum_type),
          "Which checksum type to use in repomd.xml for sqlite DBs.", "<checksum_type>" },
        { "local-sqlite", '\0', 0, G_OPTION_ARG_NONE, &(options->local_sqlite),
          "Gen sqlite DBs locally (into a directory for temporary files). "
          "Sometimes, sqlite has a trouble to gen DBs on a NFS mount, "
          "use this option in such cases. "
          "This option could lead to a higher memory consumption "
          "if TMPDIR is set to /tmp or not set at all, because then the /tmp is "
          "used and /tmp dir is often a ramdisk.", NULL },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL },
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
    // --compress-type
    if (options->compress_type) {
        options->compression_type = cr_compression_type(options->compress_type);
        if (options->compression_type == CR_CW_UNKNOWN_COMPRESSION) {
            g_set_error(err, CREATEREPO_C_ERROR, CRE_ERROR,
                        "Unknown compression type \"%s\"", options->compress_type);
            return FALSE;
        }
    }

    // --checksum
    if (options->chcksum_type) {
        cr_ChecksumType type;
        type = cr_checksum_type(options->chcksum_type);
        if (type == CR_CHECKSUM_UNKNOWN) {
            g_set_error(err, CREATEREPO_C_ERROR, CRE_BADARG,
                        "Unknown/Unsupported checksum type \"%s\"",
                        options->chcksum_type);
            return FALSE;
        }
        options->checksum_type = type;
    }

    // --xz
    if (options->xz_compression)
        options->compression_type = CR_CW_XZ_COMPRESSION;

    return TRUE;
}

// Common

static int
warningcb(G_GNUC_UNUSED cr_XmlParserWarningType type,
          char *msg,
          void *cbdata,
          G_GNUC_UNUSED GError **err)
{
    g_warning("XML parser warning (%s): %s\n", (gchar *) cbdata, msg);
    return CR_CB_RET_OK;
}

static int
pkgcb(cr_Package *pkg,
              void *cbdata,
              GError **err)
{
    int rc;
    rc = cr_db_add_pkg((cr_SqliteDb *) cbdata, pkg, err);
    cr_package_free(pkg);
    if (rc != CRE_OK)
        return CR_CB_RET_ERR;
    return CR_CB_RET_OK;
}

// Primary sqlite db

static gboolean
primary_to_sqlite(const gchar *pri_xml_path,
                  cr_SqliteDb *pri_db,
                  GError **err)
{
    int rc;
    rc = cr_xml_parse_primary(pri_xml_path,
                              NULL,
                              NULL,
                              pkgcb,
                              (void *) pri_db,
                              warningcb,
                              (void *) pri_xml_path,
                              TRUE,
                              err);
    if (rc != CRE_OK)
        return FALSE;
    return TRUE;
}

// Filelists

static gboolean
filelists_to_sqlite(const gchar *fil_xml_path,
                    cr_SqliteDb *fil_db,
                    GError **err)
{
    int rc;
    rc = cr_xml_parse_filelists(fil_xml_path,
                                NULL,
                                NULL,
                                pkgcb,
                                (void *) fil_db,
                                warningcb,
                                (void *) fil_xml_path,
                                err);
    if (rc != CRE_OK)
        return FALSE;
    return TRUE;
}

// Other

static gboolean
other_to_sqlite(const gchar *oth_xml_path,
                cr_SqliteDb *oth_db,
                GError **err)
{
    int rc;
    rc = cr_xml_parse_other(oth_xml_path,
                            NULL,
                            NULL,
                            pkgcb,
                            (void *) oth_db,
                            warningcb,
                            (void *) oth_xml_path,
                            err);
    if (rc != CRE_OK)
        return FALSE;
    return TRUE;
}

// Main

static gboolean
xml_to_sqlite(const gchar *pri_xml_path,
              const gchar *fil_xml_path,
              const gchar *oth_xml_path,
              cr_SqliteDb *pri_db,
              cr_SqliteDb *fil_db,
              cr_SqliteDb *oth_db,
              GError **err)
{
    gboolean ret;

    if (pri_xml_path && pri_db) {
        ret = primary_to_sqlite(pri_xml_path,
                                pri_db,
                                err);
        if (!ret)
            return FALSE;
        g_debug("Primary sqlite done");
    }

    if (fil_xml_path && fil_db) {
        ret = filelists_to_sqlite(fil_xml_path,
                                  fil_db,
                                  err);
        if (!ret)
            return FALSE;
        g_debug("Filelists sqlite done");
    }

    if (oth_xml_path && oth_db) {
        ret = other_to_sqlite(oth_xml_path,
                              oth_db,
                              err);
        if (!ret)
            return FALSE;
        g_debug("Other sqlite done");
    }

    return TRUE;
}

static gboolean
sqlite_dbinfo_update(cr_Repomd *repomd,
                     cr_SqliteDb *pri_db,
                     cr_SqliteDb *fil_db,
                     cr_SqliteDb *oth_db,
                     GError **err)
{
    cr_RepomdRecord *rec = NULL;

    // Parse repomd.xml
    // Get files checksums and insert them into sqlite dbs
    if (pri_db) {
        rec = cr_repomd_get_record(repomd, "primary");
        if (rec && rec->checksum)
            if (cr_db_dbinfo_update(pri_db, rec->checksum, err) != CRE_OK)
                return FALSE;
    }

    if (fil_db) {
        rec = cr_repomd_get_record(repomd, "filelists");
        if (rec && rec->checksum)
            if (cr_db_dbinfo_update(fil_db, rec->checksum, err) != CRE_OK)
                return FALSE;
    }

    if (oth_db) {
        rec = cr_repomd_get_record(repomd, "other");
        if (rec && rec->checksum)
            if (cr_db_dbinfo_update(oth_db, rec->checksum, err) != CRE_OK)
                return FALSE;
    }

    return TRUE;
}

static gboolean
compress_sqlite_dbs(const gchar *tmp_out_repo,
                    const gchar *pri_db_filename,
                    cr_RepomdRecord **in_pri_db_rec,
                    const gchar *fil_db_filename,
                    cr_RepomdRecord **in_fil_db_rec,
                    const gchar *oth_db_filename,
                    cr_RepomdRecord **in_oth_db_rec,
                    cr_CompressionType compression_type,
                    cr_ChecksumType checksum_type)
{
    cr_CompressionTask *pri_db_task;
    cr_CompressionTask *fil_db_task;
    cr_CompressionTask *oth_db_task;
    const char *sqlite_compression_suffix;
    cr_RepomdRecord *pri_db_rec = NULL;
    cr_RepomdRecord *fil_db_rec = NULL;
    cr_RepomdRecord *oth_db_rec = NULL;

    // Prepare thread pool for compression tasks
    GThreadPool *compress_pool =  g_thread_pool_new(cr_compressing_thread,
                                                    NULL, 3, FALSE, NULL);

    // Prepare output filenames
    sqlite_compression_suffix = cr_compression_suffix(compression_type);
    gchar *pri_db_name = g_strconcat(tmp_out_repo, "/primary.sqlite",
                                     sqlite_compression_suffix, NULL);
    gchar *fil_db_name = g_strconcat(tmp_out_repo, "/filelists.sqlite",
                                     sqlite_compression_suffix, NULL);
    gchar *oth_db_name = g_strconcat(tmp_out_repo, "/other.sqlite",
                                     sqlite_compression_suffix, NULL);

    // Prepare compression tasks
    pri_db_task = cr_compressiontask_new(pri_db_filename,
                                         pri_db_name,
                                         compression_type,
                                         checksum_type,
                                         NULL, FALSE, 1, NULL);
    g_thread_pool_push(compress_pool, pri_db_task, NULL);

    fil_db_task = cr_compressiontask_new(fil_db_filename,
                                         fil_db_name,
                                         compression_type,
                                         checksum_type,
                                         NULL, FALSE, 1, NULL);
    g_thread_pool_push(compress_pool, fil_db_task, NULL);

    oth_db_task = cr_compressiontask_new(oth_db_filename,
                                         oth_db_name,
                                         compression_type,
                                         checksum_type,
                                         NULL, FALSE, 1, NULL);
    g_thread_pool_push(compress_pool, oth_db_task, NULL);

    // Wait till all tasks are complete and free the thread pool
    g_thread_pool_free(compress_pool, FALSE, TRUE);

    // Remove uncompressed DBs
    cr_rm(pri_db_filename, CR_RM_FORCE, NULL, NULL);
    cr_rm(fil_db_filename, CR_RM_FORCE, NULL, NULL);
    cr_rm(oth_db_filename, CR_RM_FORCE, NULL, NULL);

    // Prepare repomd records
    pri_db_rec = cr_repomd_record_new("primary_db", pri_db_name);
    fil_db_rec = cr_repomd_record_new("filelists_db", fil_db_name);
    oth_db_rec = cr_repomd_record_new("other_db", oth_db_name);

    *in_pri_db_rec = pri_db_rec;
    *in_fil_db_rec = fil_db_rec;
    *in_oth_db_rec = oth_db_rec;

    // Free paths to compressed files
    g_free(pri_db_name);
    g_free(fil_db_name);
    g_free(oth_db_name);

    // Fill repomd records from stats gathered during compression
    cr_repomd_record_load_contentstat(pri_db_rec, pri_db_task->stat);
    cr_repomd_record_load_contentstat(fil_db_rec, fil_db_task->stat);
    cr_repomd_record_load_contentstat(oth_db_rec, oth_db_task->stat);

    // Free the compression tasks
    cr_compressiontask_free(pri_db_task, NULL);
    cr_compressiontask_free(fil_db_task, NULL);
    cr_compressiontask_free(oth_db_task, NULL);

    // Prepare thread pool for repomd record filling tasks
    GThreadPool *fill_pool = g_thread_pool_new(cr_repomd_record_fill_thread,
                                               NULL, 3, FALSE, NULL);

    // Prepare the tasks themselves
    cr_RepomdRecordFillTask *pri_db_fill_task;
    cr_RepomdRecordFillTask *fil_db_fill_task;
    cr_RepomdRecordFillTask *oth_db_fill_task;

    pri_db_fill_task = cr_repomdrecordfilltask_new(pri_db_rec,
                                                   checksum_type,
                                                   NULL);
    g_thread_pool_push(fill_pool, pri_db_fill_task, NULL);

    fil_db_fill_task = cr_repomdrecordfilltask_new(fil_db_rec,
                                                   checksum_type,
                                                   NULL);
    g_thread_pool_push(fill_pool, fil_db_fill_task, NULL);

    oth_db_fill_task = cr_repomdrecordfilltask_new(oth_db_rec,
                                                   checksum_type,
                                                   NULL);
    g_thread_pool_push(fill_pool, oth_db_fill_task, NULL);

    // Wait till the all tasks are finished and free the pool
    g_thread_pool_free(fill_pool, FALSE, TRUE);

    // Clear the tasks
    cr_repomdrecordfilltask_free(pri_db_fill_task, NULL);
    cr_repomdrecordfilltask_free(fil_db_fill_task, NULL);
    cr_repomdrecordfilltask_free(oth_db_fill_task, NULL);

    return TRUE;
}

static gboolean
uses_simple_md_filename(cr_Repomd *repomd,
                        gboolean *simple_md,
                        GError **err)
{
    cr_RepomdRecord *rec = NULL;

    // Get primary record
    rec = cr_repomd_get_record(repomd, "primary");
    if (!rec) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_ERROR,
                    "Repomd doen't contain primary.xml");
        return FALSE;
    }

    if (!rec->location_href) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_ERROR,
                    "Primary repomd record doesn't contain location href");
        return FALSE;
    }

    // Check if it's prefixed by checksum or not
    _cleanup_free_ gchar *basename = NULL;

    basename = g_path_get_basename(rec->location_href);

    if (g_str_has_prefix(basename, "primary"))
        *simple_md = TRUE;
    else
        *simple_md = FALSE;

    return TRUE;
}

/* Prepare new repomd.xml
 * Detect if unique or simple md filenames should be used.
 * Rename the files if necessary (add checksums into prefixes)
 * Add the records for databases
 * Write the updated repomd.xml into tmp_out_repo
 */
static gboolean
gen_new_repomd(const gchar *tmp_out_repo,
               cr_Repomd *in_repomd,
               cr_RepomdRecord *in_pri_db_rec,
               cr_RepomdRecord *in_fil_db_rec,
               cr_RepomdRecord *in_oth_db_rec,
               GError **err)
{
    cr_Repomd *repomd = NULL;
    cr_RepomdRecord *pri_db_rec = NULL;
    cr_RepomdRecord *fil_db_rec = NULL;
    cr_RepomdRecord *oth_db_rec = NULL;
    gboolean simple_md_filename = FALSE;

    // Check if a unique md filename should be used or not
    if (!uses_simple_md_filename(in_repomd, &simple_md_filename, err))
        return FALSE;

    // Create copy of repomd
    repomd = cr_repomd_copy(in_repomd);

    // Prepend checksum if unique md filename should be used
    if (!simple_md_filename) {
        g_debug("Renaming generated DBs to unique filenames..");
        cr_repomd_record_rename_file(in_pri_db_rec, NULL);
        cr_repomd_record_rename_file(in_fil_db_rec, NULL);
        cr_repomd_record_rename_file(in_oth_db_rec, NULL);
    }

    // Remove existing DBs
    cr_repomd_remove_record(repomd, "primary_db");
    cr_repomd_remove_record(repomd, "filelists_db");
    cr_repomd_remove_record(repomd, "other_db");

    // Create copy of the records
    //
    // Note: We do this copy, because once we set a record into
    // a repomd, the repomd overtake the ownership of the record,
    // but we don't want to lose ownership in this case.
    //
    // Note: We do this copy intentionaly after the rename,
    // because we want to have the rename propagated into
    // original records (the ones referenced in caller function).
    pri_db_rec = cr_repomd_record_copy(in_pri_db_rec);
    fil_db_rec = cr_repomd_record_copy(in_fil_db_rec);
    oth_db_rec = cr_repomd_record_copy(in_oth_db_rec);

    // Add records to repomd.xml
    cr_repomd_set_record(repomd, pri_db_rec);
    cr_repomd_set_record(repomd, fil_db_rec);
    cr_repomd_set_record(repomd, oth_db_rec);

    // Sort the records
    cr_repomd_sort_records(repomd);

    // Dump the repomd.xml content
    _cleanup_free_ gchar *repomd_content = NULL;
    repomd_content = cr_xml_dump_repomd(repomd, err);
    if (!repomd_content) {
        cr_repomd_free(repomd);
        return FALSE;
    }

    // Prepare output repomd.xml path
    _cleanup_free_ gchar *repomd_path = NULL;
    repomd_path = g_build_filename(tmp_out_repo, "repomd.xml", NULL);

    // Write the repomd.xml
    _cleanup_file_fclose_ FILE *f_repomd = NULL;
    if (!(f_repomd = fopen(repomd_path, "w"))) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                    "Cannot open %s: %s", repomd_path, g_strerror(errno));
        cr_repomd_free(repomd);
        return FALSE;
    }

    // Write the content
    fputs(repomd_content, f_repomd);

    // Cleanup
    cr_repomd_free(repomd);

    return TRUE;
}


/** Intelligently move content of tmp_out_repo to in_repo
 * (the repomd.xml is moved as a last file)
 */
static gboolean
move_results(const gchar *tmp_out_repo,
             const gchar *in_repo,
             GError **err)
{
    _cleanup_dir_close_ GDir *dirp = NULL;
    _cleanup_error_free_ GError *tmp_err = NULL;

    // Open the source directory
    dirp = g_dir_open(tmp_out_repo, 0, &tmp_err);
    if (!dirp) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                    "Cannot open dir %s: %s",
                    tmp_out_repo, tmp_err->message);
        return FALSE;
    }

    // Iterate over its content
    const gchar *filename;
    while ((filename = g_dir_read_name(dirp))) {
        _cleanup_free_ gchar *src_path = NULL;
        _cleanup_free_ gchar *dst_path = NULL;

        // Skip repomd.xml
        if (!g_strcmp0(filename, "repomd.xml"))
            continue;

        // Get full src path
        src_path = g_build_filename(tmp_out_repo, filename, NULL);

        // Prepare full dst path
        dst_path = g_build_filename(in_repo, filename, NULL);

        // Move the file
        if (g_rename(src_path, dst_path) == -1) {
            g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                        "Cannot move: %s to: %s: %s",
                        src_path, dst_path, g_strerror(errno));
            return FALSE;
        }
    }

    // The last step - move of the repomd.xml
    {
        _cleanup_free_ gchar *src_path = NULL;
        _cleanup_free_ gchar *dst_path = NULL;
        src_path = g_build_filename(tmp_out_repo, "repomd.xml", NULL);
        dst_path = g_build_filename(in_repo, "repomd.xml", NULL);
        if (g_rename(src_path, dst_path) == -1) {
            g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                        "Cannot move: %s to: %s: %s",
                        src_path, dst_path, g_strerror(errno));
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
remove_old_if_different(const gchar *repo_path,
                        cr_RepomdRecord *old_rec,
                        cr_RepomdRecord *new_rec,
                        GError **err)
{
    int rc;
    _cleanup_free_ gchar *old_fn = NULL;
    _cleanup_free_ gchar *new_fn = NULL;

    // Input check
    if (!old_rec)
        return TRUE;

    // Build filenames
    old_fn = g_build_filename(repo_path, old_rec->location_href, NULL);
    new_fn = g_build_filename(repo_path, new_rec->location_href, NULL);

    // Check if the files are the same
    gboolean identical = FALSE;
    if (!cr_identical_files(old_fn, new_fn, &identical, err))
        return FALSE;

    if (identical) {
        g_debug("Old DB file %s has been overwritten by the new one.", new_fn);
        return TRUE;
    }

    // Remove file referenced by the old record
    g_debug("Removing old DB file %s", old_fn);
    rc = g_remove(old_fn);
    if (rc == -1) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                    "Cannot remove %s: %s", old_fn, g_strerror(errno));
        return FALSE;
    }

    return TRUE;
}

static gboolean
generate_sqlite_from_xml(const gchar *path,
                         cr_CompressionType compression_type,
                         cr_ChecksumType checksum_type,
                         gboolean local_sqlite,
                         gboolean force,
                         gboolean keep_old,
                         GError **err)
{
    _cleanup_free_ gchar *in_dir       = NULL;  // path/to/repo/
    _cleanup_free_ gchar *in_repo      = NULL;  // path/to/repo/repodata/
    _cleanup_free_ gchar *out_dir      = NULL;  // path/to/out_repo/
    _cleanup_free_ gchar *tmp_out_repo = NULL;  // usually path/to/out_repo/.repodata/
    _cleanup_free_ gchar *lock_dir     = NULL;  // path/to/out_repo/.repodata/
    gboolean ret;

    // Check if input dir exists
    in_dir = cr_normalize_dir_path(path);
    if (!g_file_test(in_dir, G_FILE_TEST_IS_DIR)) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                    "Directory %s must exist\n", in_dir);
        return FALSE;
    }

    // Set other paths
    in_repo         = g_build_filename(in_dir, "repodata/", NULL);
    out_dir         = g_strdup(in_dir);
    lock_dir        = g_build_filename(out_dir, ".repodata/", NULL);
    tmp_out_repo    = g_build_filename(out_dir, ".repodata/", NULL);

    // Block signals that terminates the process
    if (!cr_block_terminating_signals(err))
        return FALSE;

    // Check if lock exists & Create lock dir
    if (!cr_lock_repo(out_dir, FALSE, &lock_dir, &tmp_out_repo, err))
        return FALSE;

    // Setup cleanup handlers
    if (!cr_set_cleanup_handler(lock_dir, tmp_out_repo, err))
        return FALSE;

    // Unblock the blocked signals
    if (!cr_unblock_terminating_signals(err))
        return FALSE;

    // Locate repodata
    struct cr_MetadataLocation *md_loc = NULL;
    _cleanup_free_ gchar *pri_xml_path = NULL;
    _cleanup_free_ gchar *fil_xml_path = NULL;
    _cleanup_free_ gchar *oth_xml_path = NULL;
    _cleanup_free_ gchar *repomd_path = NULL;

    md_loc = cr_locate_metadata(in_dir, TRUE, NULL);
    if (!md_loc || !md_loc->repomd) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_NOFILE,
                    "repomd.xml doesn't exist");
        cr_metadatalocation_free(md_loc);
        return FALSE;
    }

    repomd_path = g_build_filename(md_loc->repomd, NULL);

    if (md_loc->pri_xml_href)
        pri_xml_path = g_build_filename(md_loc->pri_xml_href, NULL);
    if (md_loc->fil_xml_href)
        fil_xml_path = g_build_filename(md_loc->fil_xml_href, NULL);
    if (md_loc->oth_xml_href)
        oth_xml_path = g_build_filename(md_loc->oth_xml_href, NULL);
    cr_metadatalocation_free(md_loc);

    // Parse repomd.xml
    int rc;
    cr_Repomd *repomd = cr_repomd_new();

    rc = cr_xml_parse_repomd(repomd_path,
                             repomd,
                             warningcb,
                             (void *) repomd_path,
                             err);
    if (rc != CRE_OK)
        return FALSE;

    // Check if DBs already exist or not
    gboolean dbs_already_exist = FALSE;

    if (cr_repomd_get_record(repomd, "primary_db")
        || cr_repomd_get_record(repomd, "filename_db")
        || cr_repomd_get_record(repomd, "other_db"))
    {
        dbs_already_exist = TRUE;
    }

    if (dbs_already_exist && !force) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_ERROR,
                    "Repository already has sqlitedb present "
                    "in repomd.xml (You may use --force)");
        return FALSE;
    }

    // Auto-detect used checksum algorithm if not specified explicitly
    if (checksum_type == CR_CHECKSUM_UNKNOWN) {
        cr_RepomdRecord *rec = cr_repomd_get_record(repomd, "primary");

        if (!rec) {
            g_set_error(err, CREATEREPO_C_ERROR, CRE_ERROR,
                        "repomd.xml is missing primary metadata");
            return FALSE;
        }

        if (rec->checksum_type)
            checksum_type = cr_checksum_type(rec->checksum_type);
        else if (rec->checksum_open_type)
            checksum_type = cr_checksum_type(rec->checksum_open_type);

        if (checksum_type == CR_CHECKSUM_UNKNOWN) {
            g_debug("Cannot auto-detect checksum type, using default %s",
                    cr_checksum_name_str(DEFAULT_CHECKSUM));
            checksum_type = DEFAULT_CHECKSUM;
        }
    }

    // Open sqlite databases
    _cleanup_free_ gchar *pri_db_filename = NULL;
    _cleanup_free_ gchar *fil_db_filename = NULL;
    _cleanup_free_ gchar *oth_db_filename = NULL;
    cr_SqliteDb *pri_db = NULL;
    cr_SqliteDb *fil_db = NULL;
    cr_SqliteDb *oth_db = NULL;

    _cleanup_file_close_ int pri_db_fd = -1;
    _cleanup_file_close_ int fil_db_fd = -1;
    _cleanup_file_close_ int oth_db_fd = -1;

    g_message("Preparing sqlite DBs");
    if (!local_sqlite) {
        g_debug("Creating databases");
        pri_db_filename = g_strconcat(tmp_out_repo, "/primary.sqlite", NULL);
        fil_db_filename = g_strconcat(tmp_out_repo, "/filelists.sqlite", NULL);
        oth_db_filename = g_strconcat(tmp_out_repo, "/other.sqlite", NULL);
    } else {
        g_debug("Creating databases localy");
        const gchar *tmpdir = g_get_tmp_dir();
        pri_db_filename = g_build_filename(tmpdir, "primary.XXXXXX.sqlite", NULL);
        fil_db_filename = g_build_filename(tmpdir, "filelists.XXXXXX.sqlite", NULL);
        oth_db_filename = g_build_filename(tmpdir, "other.XXXXXXX.sqlite", NULL);
        pri_db_fd = g_mkstemp(pri_db_filename);
        g_debug("%s", pri_db_filename);
        if (pri_db_fd == -1) {
            g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                        "Cannot open %s: %s", pri_db_filename, g_strerror(errno));
            return FALSE;
        }
        fil_db_fd = g_mkstemp(fil_db_filename);
        g_debug("%s", fil_db_filename);
        if (fil_db_fd == -1) {
            g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                        "Cannot open %s: %s", fil_db_filename, g_strerror(errno));
            return FALSE;
        }
        oth_db_fd = g_mkstemp(oth_db_filename);
        g_debug("%s", oth_db_filename);
        if (oth_db_fd == -1) {
            g_set_error(err, CREATEREPO_C_ERROR, CRE_IO,
                        "Cannot open %s: %s", oth_db_filename, g_strerror(errno));
            return FALSE;
        }
    }

    pri_db = cr_db_open_primary(pri_db_filename, err);
    if (!pri_db)
        return FALSE;

    fil_db = cr_db_open_filelists(fil_db_filename, err);
    assert(fil_db);
    if (!fil_db) {
        cr_db_close(pri_db, NULL);
        return FALSE;
    }

    oth_db = cr_db_open_other(oth_db_filename, err);
    assert(oth_db);
    if (!oth_db) {
        cr_db_close(pri_db, NULL);
        cr_db_close(fil_db, NULL);
        return FALSE;
    }

    // XML to Sqlite
    ret = xml_to_sqlite(pri_xml_path,
                        fil_xml_path,
                        oth_xml_path,
                        pri_db,
                        fil_db,
                        oth_db,
                        err);
    if (!ret)
        return FALSE;

    // Put checksums of XML files into Sqlite
    ret = sqlite_dbinfo_update(repomd,
                               pri_db,
                               fil_db,
                               oth_db,
                               err);
    if (!ret)
        return FALSE;

    // Close dbs
    cr_db_close(pri_db, NULL);
    cr_db_close(fil_db, NULL);
    cr_db_close(oth_db, NULL);

    // Repomd records
    cr_RepomdRecord *pri_db_rec = NULL;
    cr_RepomdRecord *fil_db_rec = NULL;
    cr_RepomdRecord *oth_db_rec = NULL;

    // Compress DB files and fill records
    ret = compress_sqlite_dbs(tmp_out_repo,
                              pri_db_filename,
                              &pri_db_rec,
                              fil_db_filename,
                              &fil_db_rec,
                              oth_db_filename,
                              &oth_db_rec,
                              compression_type,
                              checksum_type);
    if (!ret)
        return FALSE;

    // Prepare new repomd.xml
    ret = gen_new_repomd(tmp_out_repo,
                         repomd,
                         pri_db_rec,
                         fil_db_rec,
                         oth_db_rec,
                         err);
    if (!ret)
        return FALSE;

    // Move the results (compressed DBs and repomd.xml) into in_repo
    ret = move_results(tmp_out_repo,
                       in_repo,
                       err);
    if (!ret)
        return FALSE;

    // Remove old DBs
    if (dbs_already_exist && force && !keep_old) {
        ret = remove_old_if_different(in_dir,
                                      cr_repomd_get_record(repomd, "primary_db"),
                                      pri_db_rec, err);
        if (!ret)
            return FALSE;
        ret = remove_old_if_different(in_dir,
                                      cr_repomd_get_record(repomd, "filelists_db"),
                                      fil_db_rec, err);
        if (!ret)
            return FALSE;
        ret = remove_old_if_different(in_dir,
                                      cr_repomd_get_record(repomd, "other_db"),
                                      oth_db_rec, err);
        if (!ret)
            return FALSE;
    }

    // Remove tmp_out_repo
    g_rmdir(tmp_out_repo);

    // Clean up
    cr_repomd_free(repomd);
    cr_repomd_record_free(pri_db_rec);
    cr_repomd_record_free(fil_db_rec);
    cr_repomd_record_free(oth_db_rec);

    return TRUE;
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
    if (!parse_sqliterepo_arguments(&argc, &argv, options, &tmp_err)) {
        g_printerr("%s\n", tmp_err->message);
        exit(EXIT_FAILURE);
    }

    // Set logging
    cr_setup_logging(FALSE, options->verbose);

    // Print version if required
    if (options->version) {
        printf("Version: %s\n", cr_version_string_with_features());
        exit(EXIT_SUCCESS);
    }

    // Check arguments
    if (!check_arguments(options, &tmp_err)) {
        g_printerr("%s\n", tmp_err->message);
        exit(EXIT_FAILURE);
    }

    if (argc != 2) {
        g_printerr("Must specify exactly one repo directory to work on\n");
        exit(EXIT_FAILURE);
    }

    // Emit debug message with version
    g_debug("Version: %s", cr_version_string_with_features());

    // Gen the databases
    ret = generate_sqlite_from_xml(argv[1],
                                   options->compression_type,
                                   options->checksum_type,
                                   options->local_sqlite,
                                   options->force,
                                   options->keep_old,
                                   &tmp_err);
    if (!ret) {
        g_printerr("%s\n", tmp_err->message);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
