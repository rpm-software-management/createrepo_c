/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __C_CREATEREPOLIB_CMD_PARSER_H__
#define __C_CREATEREPOLIB_CMD_PARSER_H__

#include <glib.h>
#include "constants.h"
#include "compression_wrapper.h"


/**
 * Command line options
 */
struct CmdOptions {

    // Items filled by hand (from createrepo_c.c)

    char *input_dir;            /*!< Input directory (the mandatory argument of createrepo) */

    // Items filled by cmd option parser

    char *location_base;        /*!< base URL location */
    char *outputdir;            /*!< output directory */
    char **excludes;            /*!< list of file globs to exclude */
    char *pkglist;              /*!< file with files to include */
    char **includepkg;          /*!< list of files to include */
    char *groupfile;            /*!< groupfile path or URL */
    gboolean quiet;             /*!< quiet mode */
    gboolean verbose;           /*!< verbosely more than usual (enable debug output) */
    gboolean update;            /*!< update repo if metadata already exists */
    char **update_md_paths;     /*!< list of paths to repositories which should be used for update */
    gboolean skip_stat;         /*!< skip stat() call during --update */
    gboolean version;           /*!< print program version */
    gboolean database;          /*!< create sqlite database metadata - Not implemented yet!!! :( */
    gboolean no_database;       /*!< do not create database - Implemented ;) */
    char *checksum;             /*!< type of checksum */
    char *compress_type;        /*!< which compression type to use */
    gboolean skip_symlinks;     /*!< ignore symlinks of packages */
    int changelog_limit;        /*!< number of changelog messages in other.(xml|sqlite) */
    gboolean unique_md_filenames;       /*!< include the file checksums in the filenames */
    gboolean simple_md_filenames;       /*!< simple filenames (Name does not contain checksum) */
    int workers;                /*!< number of threads to spawn */
    gboolean xz_compression;    /*!< use xz for repodata compression */

    // Items filled by check_arguments()

    char *groupfile_fullpath;   /*!< full path to groupfile */
    GSList *exclude_masks;      /*!< list of exclude masks (list of GPatternSpec pointers) */
    GSList *include_pkgs;       /*!< list of packages to include (build from includepkg options and pkglist file) */
    GSList *l_update_md_paths;  /*!< list of repos from update_md_paths (remote repos are downloaded) */
    ChecksumType checksum_type; /*!< checksum type */
    CompressionType compression_type; /*!< compression type */

};

/**
 * Parses commandline arguments.
 * @param argc          pointer to argc
 * @param argv          pointer to argv
 * @return              CmdOptions filled by command line arguments
 */
struct CmdOptions *parse_arguments(int *argc, char ***argv);

/**
 * Performs some checks of arguments and fill some other items.
 * in the CmdOptions structure.
 */
gboolean check_arguments(struct CmdOptions *options);

/**
 * Frees CmdOptions.
 * @param options       pointer to struct with command line options
 */
void free_options(struct CmdOptions *options);

#endif /* __C_CREATEREPOLIB_CMD_PARSER_H__ */
