#ifndef __C_CREATEREPOLIB_CMD_PARSER_H__
#define __C_CREATEREPOLIB_CMD_PARSER_H__

#include <glib.h>
#include "constants.h"


struct CmdOptions {

    // Items filled by cmd option parser

    char *input_dir;            //
    char *location_base;        // Base URL location for all files
    char *outputdir;            // Output directory
    char **excludes;            // List of file globs to exclude
    char *pkglist;              // File with files to include
    char **includepkg;          // List of files to include
    char *groupfile;            // Groupfile
    gboolean quiet;             // Shut up!
    gboolean verbose;           // Verbosely more than usual
    gboolean update;            // Update repo if metadata already exists
    char **update_md_paths;     // Paths to other repositories which should be used for update
    gboolean skip_stat;         // skip stat() call during --update
    gboolean version;           // Output version.
    gboolean database;          // Not implemented yet!!!
    gboolean no_database;       // Implemented :-P
    char *checksum;             // Type of checksum
    gboolean skip_symlinks;     // Ignore symlinks of packages
    int changelog_limit;
    gboolean unique_md_filenames; // Include the file's checksum in filename
    gboolean simple_md_filenames; // Simple checksum in filename
    int workers;                  // Number of threads to spawn

    // Items filled by check_arguments()

    char *groupfile_fullpath;
    GSList *exclude_masks;
    GSList *include_pkgs;
    GSList *l_update_md_paths;
    ChecksumType checksum_type;

};


struct CmdOptions *parse_arguments(int *argc, char ***argv);
gboolean check_arguments(struct CmdOptions *options);
void free_options(struct CmdOptions *options);

#endif /* __C_CREATEREPOLIB_CMD_PARSER_H__ */
