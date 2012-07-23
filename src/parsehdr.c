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

#include <glib.h>
#include <assert.h>
#include <rpm/rpmfi.h>
#include <stdlib.h>
#include "logging.h"
#include "parsehdr.h"
#include "xml_dump.h"
#include "misc.h"

#undef MODULE
#define MODULE "parsehdr: "


inline gchar *safe_string_chunk_insert(GStringChunk *chunk, const char *str)
{
    if (!chunk || !str) {
        return NULL;
    }

    return g_string_chunk_insert(chunk, str);
}



cr_Package *cr_parse_header(Header hdr, gint64 mtime, gint64 size,
                            const char *checksum, const char *checksum_type,
                            const char *location_href,
                            const char *location_base, int changelog_limit,
                            gint64 hdr_start, gint64 hdr_end)
{
    // Create new package structure

    cr_Package *pkg = NULL;
    pkg = cr_package_new();


    // Create rpm tag data container

    rpmtd td = rpmtdNew();
    headerGetFlags flags = HEADERGET_MINMEM | HEADERGET_EXT;


    // Fill package structure

    pkg->pkgId = safe_string_chunk_insert(pkg->chunk, checksum);
    pkg->name = safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_NAME));

    gint64 is_src = headerGetNumber(hdr, RPMTAG_SOURCEPACKAGE);
    if (is_src) {
        pkg->arch = safe_string_chunk_insert(pkg->chunk, "src");
    } else {
        pkg->arch = safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_ARCH));
    }

    pkg->version = safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_VERSION));

#define MAX_STR_INT_LEN 24
    char tmp_epoch[MAX_STR_INT_LEN];
    if (snprintf(tmp_epoch, MAX_STR_INT_LEN, "%llu", (long long unsigned int) headerGetNumber(hdr, RPMTAG_EPOCH)) <= 0) {
        tmp_epoch[0] = '\0';
    }
    pkg->epoch = g_string_chunk_insert_len(pkg->chunk, tmp_epoch, MAX_STR_INT_LEN);

    pkg->release = safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_RELEASE));
    pkg->summary = safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_SUMMARY));
    pkg->description = safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_DESCRIPTION));
    pkg->url = safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_URL));
    pkg->time_file = mtime;
    if (headerGet(hdr, RPMTAG_BUILDTIME, td, flags)) {
        pkg->time_build = rpmtdGetNumber(td);
    }
    pkg->rpm_license = safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_LICENSE));
    pkg->rpm_vendor = safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_VENDOR));
    pkg->rpm_group = safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_GROUP));
    pkg->rpm_buildhost = safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_BUILDHOST));
    pkg->rpm_sourcerpm = safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_SOURCERPM));
    pkg->rpm_header_start = hdr_start;
    pkg->rpm_header_end = hdr_end;
    pkg->rpm_packager = safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_PACKAGER));
    pkg->size_package = size;
    if (headerGet(hdr, RPMTAG_SIZE, td, flags)) {
        pkg->size_installed = rpmtdGetNumber(td);
    }
    if (headerGet(hdr, RPMTAG_ARCHIVESIZE, td, flags)) {
        pkg->size_archive = rpmtdGetNumber(td);
    }
    pkg->location_href = safe_string_chunk_insert(pkg->chunk, location_href);
    pkg->location_base = safe_string_chunk_insert(pkg->chunk, location_base);
    pkg->checksum_type = safe_string_chunk_insert(pkg->chunk, checksum_type);

    rpmtdFreeData(td);
    rpmtdFree(td);


    //
    // Fill files
    //

    rpmtd full_filenames = rpmtdNew(); // Only for filenames_hashtable
    rpmtd indexes   = rpmtdNew();
    rpmtd filenames = rpmtdNew();
    rpmtd fileflags = rpmtdNew();
    rpmtd filemodes = rpmtdNew();

    GHashTable *filenames_hashtable = g_hash_table_new(g_str_hash, g_str_equal);

    rpmtd dirnames = rpmtdNew();


    // Create list of pointer to directory names

    int dir_count;
    char **dir_list = NULL;
    if (headerGet(hdr, RPMTAG_DIRNAMES, dirnames,  flags) && (dir_count = rpmtdCount(dirnames))) {
        int x = 0;
        dir_list = malloc(sizeof(char *) * dir_count);
        while (rpmtdNext(dirnames) != -1) {
            dir_list[x] = safe_string_chunk_insert(pkg->chunk, rpmtdGetString(dirnames));
            x++;
        }
        assert(x == dir_count);
    }

    if (headerGet(hdr, RPMTAG_FILENAMES,  full_filenames,  flags) &&
        headerGet(hdr, RPMTAG_DIRINDEXES, indexes,  flags) &&
        headerGet(hdr, RPMTAG_BASENAMES,  filenames, flags) &&
        headerGet(hdr, RPMTAG_FILEFLAGS,  fileflags, flags) &&
        headerGet(hdr, RPMTAG_FILEMODES,  filemodes, flags))
    {
        rpmtdInit(full_filenames);
        rpmtdInit(indexes);
        rpmtdInit(filenames);
        rpmtdInit(fileflags);
        rpmtdInit(filemodes);
        while ((rpmtdNext(full_filenames) != -1)   &&
               (rpmtdNext(indexes) != -1)   &&
               (rpmtdNext(filenames) != -1) &&
               (rpmtdNext(fileflags) != -1) &&
               (rpmtdNext(filemodes) != -1))
        {
            cr_PackageFile *packagefile = cr_package_file_new();
            packagefile->name = safe_string_chunk_insert(pkg->chunk, rpmtdGetString(filenames));
            packagefile->path = (dir_list) ? dir_list[(int) rpmtdGetNumber(indexes)] : "";

            if (S_ISDIR(rpmtdGetNumber(filemodes))) {
                // Directory
                packagefile->type = safe_string_chunk_insert(pkg->chunk, "dir");
            } else if (rpmtdGetNumber(fileflags) & RPMFILE_GHOST) {
                // Ghost
                packagefile->type = safe_string_chunk_insert(pkg->chunk, "ghost");
            } else {
                // Regular file
                packagefile->type = safe_string_chunk_insert(pkg->chunk, "");
            }

            g_hash_table_replace(filenames_hashtable, (gpointer) rpmtdGetString(full_filenames), (gpointer) rpmtdGetString(full_filenames));
            pkg->files = g_slist_prepend(pkg->files, packagefile);
        }
        pkg->files = g_slist_reverse (pkg->files);

        rpmtdFreeData(dirnames);
        rpmtdFreeData(indexes);
        rpmtdFreeData(filenames);
        rpmtdFreeData(fileflags);
        rpmtdFreeData(filemodes);
    }

    rpmtdFree(dirnames);
    rpmtdFree(indexes);
    rpmtdFree(filemodes);

    if (dir_list) {
        free((void *) dir_list);
    }


    //
    // PCOR (provides, conflicts, obsoletes, requires)
    //

    rpmtd fileversions = rpmtdNew();

    enum pcor {PROVIDES, CONFLICTS, OBSOLETES, REQUIRES};

    int file_tags[REQUIRES+1] = { RPMTAG_PROVIDENAME,
                                RPMTAG_CONFLICTNAME,
                                RPMTAG_OBSOLETENAME,
                                RPMTAG_REQUIRENAME
                              };

    int flag_tags[REQUIRES+1] = { RPMTAG_PROVIDEFLAGS,
                                RPMTAG_CONFLICTFLAGS,
                                RPMTAG_OBSOLETEFLAGS,
                                RPMTAG_REQUIREFLAGS
                              };

    int version_tags[REQUIRES+1] = { RPMTAG_PROVIDEVERSION,
                                    RPMTAG_CONFLICTVERSION,
                                    RPMTAG_OBSOLETEVERSION,
                                    RPMTAG_REQUIREVERSION
                                   };

    // Struct used as value in ap_hashtable
    struct ap_value_struct {
        const char *flags;
        const char *version;
        int pre;
    };

    // Hastable with filenames from provided
    GHashTable *provided_hashtable = g_hash_table_new(g_str_hash, g_str_equal);

    // Hashtable with already processed files from requires
    GHashTable *ap_hashtable = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, free);

    int pcor_type;
    for (pcor_type=0; pcor_type <= REQUIRES; pcor_type++) {
        if (headerGet(hdr, file_tags[pcor_type], filenames, flags) &&
            headerGet(hdr, flag_tags[pcor_type], fileflags, flags) &&
            headerGet(hdr, version_tags[pcor_type], fileversions, flags))
        {

            // Because we have to select only libc.so with highest version
            // e.g. libc.so.6(GLIBC_2.4)
            cr_Dependency *libc_require_highest = NULL;

            rpmtdInit(filenames);
            rpmtdInit(fileflags);
            rpmtdInit(fileversions);
            while ((rpmtdNext(filenames) != -1) &&
                   (rpmtdNext(fileflags) != -1) &&
                   (rpmtdNext(fileversions) != -1))
            {
                int pre = 0;
                const char *filename = rpmtdGetString(filenames);
                guint64 num_flags = rpmtdGetNumber(fileflags);
                const char *flags = cr_flag_to_string(num_flags);
                const char *full_version = rpmtdGetString(fileversions);

                // Requires specific stuff
                if (pcor_type == REQUIRES) {
                    // Skip requires which start with "rpmlib("
                    if (!strncmp("rpmlib(", filename, 7)) {
                        continue;
                    }

                    // Skip package primary files
                    if (g_hash_table_lookup_extended(filenames_hashtable, filename, NULL, NULL)) {
                        if (cr_is_primary(filename)) {
                            continue;
                        }
                    }

                    // Skip files which are provided
                    if (g_hash_table_lookup_extended(provided_hashtable, filename, NULL, NULL)) {
                        continue;
                    }

                    // Calculate pre value
                    if (num_flags & (RPMSENSE_PREREQ |
                                     RPMSENSE_SCRIPT_PRE |
                                     RPMSENSE_SCRIPT_POST))
                    {
                        pre = 1;
                    }

                    // Skip duplicate files
                    gpointer value;
                    if (g_hash_table_lookup_extended(ap_hashtable, filename, NULL, &value)) {
                        struct ap_value_struct *ap_value = value;
                        if (!g_strcmp0(ap_value->flags, flags) &&
                            !strcmp(ap_value->version, full_version) &&
                            (ap_value->pre == pre))
                        {
                            continue;
                        }
                    }
                }

                // Create dynamic dependency object
                cr_Dependency *dependency = cr_dependency_new();
                dependency->name = safe_string_chunk_insert(pkg->chunk, filename);
                dependency->flags = safe_string_chunk_insert(pkg->chunk, flags);
                struct cr_EVR evr = cr_string_to_version(full_version, pkg->chunk);
                dependency->epoch = evr.epoch;
                dependency->version = evr.version;
                dependency->release = evr.release;

                switch (pcor_type) {
                    case PROVIDES:
                        g_hash_table_replace(provided_hashtable, dependency->name, dependency->name);
                        pkg->provides = g_slist_prepend(pkg->provides, dependency);
                        break;
                    case CONFLICTS:
                        pkg->conflicts = g_slist_prepend(pkg->conflicts, dependency);
                        break;
                    case OBSOLETES:
                        pkg->obsoletes = g_slist_prepend(pkg->obsoletes, dependency);
                        break;
                    case REQUIRES:
                        dependency->pre = pre;

                        // XXX: Ugly libc.so filtering ////////////////////////////////
                        if (g_str_has_prefix(dependency->name, "libc.so.6")) {
                            if (!libc_require_highest) {
                                libc_require_highest = dependency;
                            } else {
                                // Only highest version can survive! Death figh!
                                // libc.so.6, libc.so.6(GLIBC_2.3.4), libc.so.6(GLIBC_2.4), ...
                                char *old_name = libc_require_highest->name;
                                char *new_name = dependency->name;
                                int old_len = strlen(old_name);
                                int new_len = strlen(new_name);

                                if (old_len == 9 && new_len > 9) {
                                    // Old name is probably only "libc.so.6"
                                    g_free(libc_require_highest);
                                    libc_require_highest = dependency;
                                } else if (old_len > 16 && new_len > 16) {
                                    // We have two long names, keep only highest version
                                    int new_i = 16;
                                    int old_i = 16;
                                    do {
                                        // Compare version
                                        if (g_ascii_isdigit(old_name[old_i]) &&
                                            g_ascii_isdigit(new_name[new_i]))
                                        {
                                            // Everything is ok, compare two numbers
                                            int new_v = atoi(new_name + new_i);
                                            int old_v = atoi(old_name + old_i);
                                            if (new_v > old_v) {
                                                g_free(libc_require_highest);
                                                libc_require_highest = dependency;
                                                break;
                                            } else if (new_v < old_v) {
                                                g_free(dependency);
                                                break;
                                            }
                                        } else {
                                            // we probably got something like libc.so.6()(64bit)
                                            if (!g_ascii_isdigit(old_name[old_i])) {
                                                // replace current libc.so.6()(64bit) with
                                                // the new one libc.so.6...
                                                g_free(libc_require_highest);
                                                libc_require_highest = dependency;
                                            }
                                            break;
                                        }

                                        // Find next digit in new
                                        int dot = 0;
                                        while (1) {
                                            new_i++;
                                            if (new_name[new_i] == '\0') {
                                                // End of string
                                                g_free(dependency);
                                                break;
                                            }
                                            if (new_name[new_i] == '.') {
                                                // '.' is separator between digits
                                                dot = 1;
                                            } else if (dot && g_ascii_isdigit(new_name[new_i])) {
                                                // We got next digit!
                                                break;
                                            }
                                        }

                                        // Find next digit in old
                                        dot = 0;
                                        while (1) {
                                            old_i++;
                                            if (old_name[old_i] == '\0') {
                                                // End of string
                                                g_free(libc_require_highest);
                                                libc_require_highest = dependency;
                                                break;
                                            }
                                            if (new_name[old_i] == '.') {
                                                // '.' is separator between digits
                                                dot = 1;
                                            } else if (dot && g_ascii_isdigit(old_name[old_i])) {
                                                // We got next digit!
                                                break;
                                            }
                                        }
                                    } while (new_name[new_i] != '\0' && old_name[old_i] != '\0');
                                } else {
                                    // Do not touch anything
                                    g_free(dependency);
                                }
                            }
                            break;
                        }
                        // XXX: Ugly libc.so filtering - END ////////////////////////////////

                        pkg->requires = g_slist_prepend(pkg->requires, dependency);

                        // Add file into ap_hashtable
                        struct ap_value_struct *value = malloc(sizeof(struct ap_value_struct));
                        value->flags = flags;
                        value->version = full_version;
                        value->pre = dependency->pre;
                        g_hash_table_replace(ap_hashtable, dependency->name, value);
                        break;
                } // Switch end
            } // While end

            // XXX: Ugly libc.so filtering ////////////////////////////////
            if (pcor_type == REQUIRES && libc_require_highest) {
                pkg->requires = g_slist_prepend(pkg->requires, libc_require_highest);
            }
            // XXX: Ugly libc.so filtering - END ////////////////////////////////
        }

        rpmtdFreeData(filenames);
        rpmtdFreeData(fileflags);
        rpmtdFreeData(fileversions);
    }

    pkg->provides  = g_slist_reverse (pkg->provides);
    pkg->conflicts = g_slist_reverse (pkg->conflicts);
    pkg->obsoletes = g_slist_reverse (pkg->obsoletes);
    pkg->requires  = g_slist_reverse (pkg->requires);

    g_hash_table_remove_all(filenames_hashtable);
    g_hash_table_remove_all(provided_hashtable);
    g_hash_table_remove_all(ap_hashtable);

    g_hash_table_unref(filenames_hashtable);
    g_hash_table_unref(provided_hashtable);
    g_hash_table_unref(ap_hashtable);

    rpmtdFree(filenames);
    rpmtdFree(fileflags);
    rpmtdFree(fileversions);

    rpmtdFreeData(full_filenames);
    rpmtdFree(full_filenames);

    //
    // Changelogs
    //

    rpmtd changelogtimes = rpmtdNew();
    rpmtd changelognames = rpmtdNew();
    rpmtd changelogtexts = rpmtdNew();

    if (headerGet(hdr, RPMTAG_CHANGELOGTIME, changelogtimes, flags) &&
        headerGet(hdr, RPMTAG_CHANGELOGNAME, changelognames, flags) &&
        headerGet(hdr, RPMTAG_CHANGELOGTEXT, changelogtexts, flags))
    {
        gint64 last_time = 0;
        rpmtdInit(changelogtimes);
        rpmtdInit(changelognames);
        rpmtdInit(changelogtexts);
        while ((rpmtdNext(changelogtimes) != -1) &&
               (rpmtdNext(changelognames) != -1) &&
               (rpmtdNext(changelogtexts) != -1) &&
               (changelog_limit > 0))
        {
            gint64 time = rpmtdGetNumber(changelogtimes);

            cr_ChangelogEntry *changelog = cr_changelog_entry_new();
            changelog->author    = safe_string_chunk_insert(pkg->chunk, rpmtdGetString(changelognames));
            changelog->date      = time;
            changelog->changelog = safe_string_chunk_insert(pkg->chunk, rpmtdGetString(changelogtexts));

            // Remove space from end of author name
            if (changelog->author) {
                size_t len, x;
                len = strlen(changelog->author);
                for (x=(len-1); x > 0; x--) {
                    if (changelog->author[x] == ' ') {
                        changelog->author[x] = '\0';
                    } else {
                        break;
                    }
                }
            }

            pkg->changelogs = g_slist_prepend(pkg->changelogs, changelog);
            changelog_limit--;

            // If a previous entry has the same time, increment time of the previous
            // entry by one. Ugly but works!
            if (last_time == time) {
                int tmp_time = time;
                GSList *previous = pkg->changelogs;
                while ((previous = g_slist_next(previous)) != NULL &&
                       ((cr_ChangelogEntry *) (previous->data))->date == tmp_time) {
                    ((cr_ChangelogEntry *) (previous->data))->date++;
                    tmp_time++;
                }
            } else {
                last_time = time;
            }


        }
        //pkg->changelogs = g_slist_reverse (pkg->changelogs);
    }

    rpmtdFreeData(changelogtimes);
    rpmtdFreeData(changelognames);
    rpmtdFreeData(changelogtexts);

    rpmtdFree(changelogtimes);
    rpmtdFree(changelognames);
    rpmtdFree(changelogtexts);

    return pkg;
}



struct cr_XmlStruct cr_xml_from_header(Header hdr, gint64 mtime, gint64 size,
                                       const char *checksum,
                                       const char *checksum_type,
                                       const char *location_href,
                                       const char *location_base,
                                       int changelog_limit,
                                       gint64 hdr_start, gint64 hdr_end)
{
    cr_Package *pkg = cr_parse_header(hdr, mtime, size, checksum, checksum_type,
                                      location_href, location_base,
                                      changelog_limit, hdr_start, hdr_end);

    struct cr_XmlStruct result;
    result.primary   = cr_xml_dump_primary(pkg);
    result.filelists = cr_xml_dump_filelists(pkg);
    result.other     = cr_xml_dump_other(pkg);

    // Cleanup
    cr_package_free(pkg);

    return result;
}
