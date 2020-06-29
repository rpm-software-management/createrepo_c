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

#ifndef __C_CREATEREPOLIB_CMD_PARSER_H__
#define __C_CREATEREPOLIB_CMD_PARSER_H__

#include <glib.h>
#include "checksum.h"
#include "compression_wrapper.h"

#define DEFAULT_CHANGELOG_LIMIT         10


/**
 * Command line options
 */
struct CmdOptions {

    /* Items filled by cmd option parser */

    char *basedir;              /*!< basedir for path to directories */
    char *location_base;        /*!< base URL location */
    char *outputdir;            /*!< output directory */
    char **excludes;            /*!< list of file globs to exclude */
    char *pkglist;              /*!< file with files to include */
    char **includepkg;          /*!< list of files to include */
    char *groupfile;            /*!< groupfile path or URL */
    gboolean quiet;             /*!< quiet mode */
    gboolean verbose;           /*!< verbosely more than usual (debug) */
    gboolean update;            /*!< update repo if metadata already exists */
    gboolean pretty;            /*!< generate pretty xml (just for compatibility) */
    char **update_md_paths;     /*!< list of paths to repositories which should
                                     be used for update */
    gboolean skip_stat;         /*!< skip stat() call during --update */
    gboolean split;             /*!< generate split media */
    gboolean version;           /*!< print program version */
    gboolean database;          /*!< create sqlite database metadata */
    gboolean no_database;       /*!< do not create database */
    char *checksum;             /*!< type of checksum */
    char *compress_type;        /*!< which compression type to use */
    char *general_compress_type;/*!< which compression type to use (even for
                                     primary, filelists and other xml) */
    gboolean skip_symlinks;     /*!< ignore symlinks of packages */
    gint changelog_limit;       /*!< number of changelog messages in
                                     other.(xml|sqlite) */
    gboolean unique_md_filenames;       /*!< include the file checksums in
                                             the filenames */
    gboolean simple_md_filenames;       /*!< simple filenames (names without
                                             checksums) */
    gint retain_old;            /*!< keep latest N copies of the old repodata */
    char **distro_tags;         /*!< distro tag and optional cpeid */
    char **content_tags;        /*!< tags for the content in the repository */
    char **repo_tags;           /*!< tags to describe the repo_tagsitory itself */
    char *revision;             /*!< user-specified revision */
    gboolean set_timestamp_to_revision; /*!< use --revision instead of current
                                             time for timestamps */
    char *read_pkgs_list;       /*!< output the paths to pkgs actually read */
    gint workers;               /*!< number of threads to spawn */
    gboolean xz_compression;    /*!< use xz for repodata compression */
    gboolean zck_compression;   /*!< generate zchunk files */
    char *zck_dict_dir;         /*!< directory with zchunk dictionaries */
    gboolean keep_all_metadata; /*!< keep groupfile and updateinfo from source
                                     repo during update */
    gboolean ignore_lock;       /*!< Ignore existing .repodata/ - remove it,
                                     create the new one (empty) to serve as
                                     a lock and use a .repodata.date.pid for
                                     data generation. */
    gboolean compatibility;     /*!< Enforce maximal compatibility with
                                     createrepo. I.e. mimics some dump
                                     behavior like perseve old comps file(s)
                                     during update etc.*/
    char *retain_old_md_by_age; /*!< Remove all files in repodata/ older
                                     then specified period of time,
                                     during --update.
                                     Value examples: "360" (360 sec),
                                     "5d" (5 days), ..
                                     Available units: (m - minutes, h - hours,
                                     d - days) */
    char *cachedir;             /*!< Cache dir for checksums */

    gboolean deltas;            /*!< Is delta generation enabled? */
    char **oldpackagedirs;      /*!< Paths to look for older pks
                                     to delta agains */
    gint num_deltas;            /*!< Number of older version to make
                                     deltas against */
    gint64 max_delta_rpm_size;  /*!< Max size of an rpm that to run
                                     deltarpm against */
    gboolean local_sqlite;      /*!< Gen sqlite locally into a directory for
                                     temporary files.
                                     For situations when sqlite has a trouble
                                     to gen DBs on NFS mounts. */
    gint cut_dirs;              /*!< Ignore *num* of directory components
                                     during repodata generation in location
                                     href value. */
    gchar *location_prefix;     /*!< Append this prefix into location_href
                                     during repodata generation. */
    gchar *repomd_checksum;     /*!< Checksum type for entries in repomd.xml */
    gboolean error_exit_val;        /*!< exit 2 on processing errors */

    /* Items filled by check_arguments() */

    char *groupfile_fullpath;   /*!< full path to groupfile */
    GSList *exclude_masks;      /*!< list of exclude masks
                                     (list of GPatternSpec pointers) */
    GSList *include_pkgs;       /*!< list of packages to include (build from
                                     includepkg options and pkglist file) */
    GSList *l_update_md_paths;  /*!< list of repo from update_md_paths
                                     (remote repo are downloaded) */
    GSList *distro_cpeids;      /*!< CPEIDs from --distro params */
    GSList *distro_values;      /*!< values from --distro params */
    cr_ChecksumType checksum_type;          /*!< checksum type */
    cr_ChecksumType repomd_checksum_type;   /*!< checksum type */
    cr_CompressionType compression_type;    /*!< compression type */
    cr_CompressionType general_compression_type; /*!< compression type */
    gint64 md_max_age;          /*!< Max age of files in repodata/.
                                     Older files will be removed
                                     during --update.
                                     Filled if --retain-old-md-by-age
                                     is used */
    char *checksum_cachedir;    /*!< Path to cachedir */
    GSList *oldpackagedirs_paths; /*!< paths to look for older pkgs to delta against */
    GSList *modulemd_metadata;  /*!< paths to all modulemd metadata */

    gboolean recycle_pkglist;
};

/**
 * Parses commandline arguments.
 * @param argc          pointer to argc
 * @param argv          pointer to argv
 * @return              CmdOptions filled by command line arguments
 */
struct CmdOptions *
parse_arguments(int *argc, char ***argv, GError **err);

/**
 * Performs some checks of arguments and fill some other items.
 * in the CmdOptions structure.
 */
gboolean
check_arguments(struct CmdOptions *options,
                const char *inputdir,
                GError **err);

/**
 * Frees CmdOptions.
 * @param options       pointer to struct with command line options
 */
void
free_options(struct CmdOptions *options);

#endif /* __C_CREATEREPOLIB_CMD_PARSER_H__ */
