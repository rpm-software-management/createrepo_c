/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2014  Tomas Mlcoch
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

#ifndef __C_CREATEREPOLIB_DUMPER_THREAD_H__
#define __C_CREATEREPOLIB_DUMPER_THREAD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <rpm/rpmlib.h>
#include "load_metadata.h"
#include "locate_metadata.h"
#include "misc.h"
#include "package.h"
#include "sqlite.h"
#include "xml_file.h"

/** \defgroup   dumperthread    Implementation of concurent dumping used in createrepo_c
 *  \addtogroup dumperthread
 *  @{
 */

struct PoolTask {
    long  id;                       // ID of the task
    long  media_id;                 // ID of media in split mode, 0 if not in split mode
    char* full_path;                // Complete path - /foo/bar/packages/foo.rpm
    char* filename;                 // Just filename - foo.rpm
    char* path;                     // Just path     - /foo/bar/packages
};

struct UserData {
    cr_XmlFile *pri_f;              // Opened compressed primary.xml.*
    cr_XmlFile *fil_f;              // Opened compressed filelists.xml.*
    cr_XmlFile *oth_f;              // Opened compressed other.xml.*
    cr_SqliteDb *pri_db;            // Primary db
    cr_SqliteDb *fil_db;            // Filelists db
    cr_SqliteDb *oth_db;            // Other db
    cr_XmlFile *pri_zck;            // Opened compressed primary.xml.zck
    cr_XmlFile *fil_zck;            // Opened compressed filelists.xml.zck
    cr_XmlFile *oth_zck;            // Opened compressed other.xml.zck
    char *prev_srpm;                // Previous srpm
    char *cur_srpm;                 // Current srpm
    int changelog_limit;            // Max number of changelogs for a package
    const char *location_base;      // Base location url
    int repodir_name_len;           // Len of path to repo /foo/bar/repodata
                                    //       This part     |<----->|
    const char *checksum_type_str;  // Name of selected checksum
    cr_ChecksumType checksum_type;  // Constant representing selected checksum
    const char *checksum_cachedir;  // Dir with cached checksums
    gboolean skip_symlinks;         // Skip symlinks
    long task_count;                // Total number of task to process
    long package_count;             // Total number of packages processed

    // Update stuff
    gboolean skip_stat;             // Skip stat() while updating
    cr_Metadata *old_metadata;      // Loaded metadata
    GMutex mutex_old_md;           // Mutex for accessing old metadata

    // Thread serialization
    GMutex mutex_pri;              // Mutex for primary metadata
    GMutex mutex_fil;              // Mutex for filelists metadata
    GMutex mutex_oth;              // Mutex for other metadata
    GCond cond_pri;                // Condition for primary metadata
    GCond cond_fil;                // Condition for filelists metadata
    GCond cond_oth;                // Condition for other metadata
    volatile long id_pri;           // ID of task on turn (write primary metadata)
    volatile long id_fil;           // ID of task on turn (write filelists metadata)
    volatile long id_oth;           // ID of task on turn (write other metadata)

    // Buffering
    GQueue *buffer;                 // Buffer for done tasks
    GMutex mutex_buffer;           // Mutex for accessing the buffer

    // Delta generation
    gboolean deltas;                // Are deltas enabled?
    gint64 max_delta_rpm_size;      // Max size of an rpm that to run
                                    // deltarpm against
    GMutex mutex_deltatargetpackages; // Mutex
    GSList *deltatargetpackages;    // List of cr_DeltaTargetPackages

    // Location href modifiers
    gint cut_dirs;                  // Ignore *num* of directory components
                                    // in location href path
    gchar *location_prefix;         // Append this prefix into location_href
                                    // during repodata generation
    gboolean had_errors;            // Any errors encountered?

    FILE *output_pkg_list;          // File where a list of read packages is written
    GMutex mutex_output_pkg_list;   // Mutex for output_pkg_list file
};


void
cr_dumper_thread(gpointer data, gpointer user_data);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_DUMPER_THREAD_H__ */
