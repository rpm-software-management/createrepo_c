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

#include <glib.h>
#include <assert.h>
#include <rpm/rpmfi.h>
#include <stdlib.h>
#include "parsehdr.h"
#include "xml_dump.h"
#include "misc.h"
#include "cleanup.h"

#if defined(RPMTAG_SUGGESTS) && defined(RPMTAG_ENHANCES) \
    && defined(RPMTAG_RECOMMENDS) && defined(RPMTAG_SUPPLEMENTS)
#define RPM_WEAK_DEPS_SUPPORT 1
#endif

#ifdef ENABLE_LEGACY_WEAKDEPS
#define RPMSENSE_STRONG (1 << 27)
#define RPMSENSE_MISSINGOK (1 << 19)
#endif

typedef enum DepType_e {
    DEP_PROVIDES,
    DEP_CONFLICTS,
    DEP_OBSOLETES,
    DEP_REQUIRES,
    DEP_SUGGESTS,
    DEP_ENHANCES,
    DEP_RECOMMENDS,
    DEP_SUPPLEMENTS,
#ifdef ENABLE_LEGACY_WEAKDEPS
    DEP_OLDSUGGESTS,
    DEP_OLDENHANCES,
#endif
    DEP_SENTINEL
} DepType;

typedef struct DepItem_s {
    DepType type;
    int nametag;
    int flagstag;
    int versiontag;
} DepItem;

// Keep this list sorted in the same order as the enum DepType_e!
static DepItem dep_items[] = {
    { DEP_PROVIDES, RPMTAG_PROVIDENAME, RPMTAG_PROVIDEFLAGS, RPMTAG_PROVIDEVERSION },
    { DEP_CONFLICTS, RPMTAG_CONFLICTNAME, RPMTAG_CONFLICTFLAGS, RPMTAG_CONFLICTVERSION },
    { DEP_OBSOLETES, RPMTAG_OBSOLETENAME, RPMTAG_OBSOLETEFLAGS, RPMTAG_OBSOLETEVERSION },
    { DEP_REQUIRES, RPMTAG_REQUIRENAME, RPMTAG_REQUIREFLAGS, RPMTAG_REQUIREVERSION },
#ifdef RPM_WEAK_DEPS_SUPPORT
    { DEP_SUGGESTS, RPMTAG_SUGGESTNAME, RPMTAG_SUGGESTFLAGS, RPMTAG_SUGGESTVERSION },
    { DEP_ENHANCES, RPMTAG_ENHANCENAME, RPMTAG_ENHANCEFLAGS, RPMTAG_ENHANCEVERSION },
    { DEP_RECOMMENDS, RPMTAG_RECOMMENDNAME, RPMTAG_RECOMMENDFLAGS, RPMTAG_RECOMMENDVERSION },
    { DEP_SUPPLEMENTS, RPMTAG_SUPPLEMENTNAME, RPMTAG_SUPPLEMENTFLAGS, RPMTAG_SUPPLEMENTVERSION },
#ifdef ENABLE_LEGACY_WEAKDEPS
    { DEP_OLDSUGGESTS,   RPMTAG_OLDSUGGESTSNAME, RPMTAG_OLDSUGGESTSFLAGS, RPMTAG_OLDSUGGESTSVERSION },
    { DEP_OLDENHANCES,    RPMTAG_OLDENHANCESNAME, RPMTAG_OLDENHANCESFLAGS, RPMTAG_OLDENHANCESVERSION },
#endif
#endif
    { DEP_SENTINEL, 0, 0, 0 },
};

static inline int
cr_compare_dependency(const char *dep1, const char *dep2)
{
    /* Compares two dependency by name
     * NOTE: The function assume first parts must be same!
     * libc.so.6() < libc.so.6(GLIBC_2.3.4)(64 bit) < libc.so.6(GLIBC_2.4)
     * Return values: 0 - same; 1 - first is bigger; 2 - second is bigger,
     * -1 - error
     */
    int ret1;
    char *ver1, *ver2, *ver1_e, *ver2_e;

    if (dep1 == dep2) return 0;

    ver1 = strchr(dep1, '('); // libc.so.6(...
    ver2 = strchr(dep2, '('); //    verX  ^

    // There is no '('
    if (!ver1 && !ver2) return 0;
    if (!ver1) return 2;
    if (!ver2) return 1;

    ver1_e = strchr(ver1, ')'); // libc.so.6(xxx)...
    ver2_e = strchr(ver2, ')'); //       verX_e ^

    // If there is no ')'
    if (!ver1_e && !ver2_e) return -1;
    if (!ver1_e) return 2;
    if (!ver2_e) return 1;

    // Go to char next to '('
    ver1++; // libc.so.6(...
    ver2++; //      verX ^

    // If parentheses have no content - libc.so.6()... == libc.so.6()...
    if (ver1 == ver1_e && ver2 == ver2_e) return 0;
    if (ver1 == ver1_e) return 2;
    if (ver2 == ver2_e) return 1;

    // Go to first number
    for (; *ver1 && (*ver1 < '0' || *ver1 > '9'); ver1++); // libc.so.6(GLIBC_2...
    for (; *ver2 && (*ver2 < '0' || *ver2 > '9'); ver2++); //            verX ^

    // Too far
    // libc.so.6(xxx)(64bit)
    //           verX ^
    if (ver1 > ver1_e && ver2 > ver2_e) return 0;
    if (ver1 > ver1_e) return 2;
    if (ver2 > ver2_e) return 1;

/*  XXX: This piece of code could be removed in future 
    // Check if version is really version and not an architecture
    // case: libc.so.6(64bit) = 64 is not a version!
    ret1 = strncmp(ver1, "64bit", 5);
    ret2 = strncmp(ver2, "64bit", 5);
    if (!ret1 && !ret2) return 0;
    if (!ret1) return 2;
    if (!ret2) return 1;
*/
    // Get version string
    ver1 = g_strndup(ver1, (ver1_e - ver1));
    ver2 = g_strndup(ver2, (ver2_e - ver2));

    // Compare versions
    ret1 = rpmvercmp(ver1, ver2);
    if (ret1 == -1) ret1 = 2;

    g_free(ver1);
    g_free(ver2);
    return ret1;
}


cr_Package *
cr_package_from_header(Header hdr,
                       int changelog_limit,
                       cr_HeaderReadingFlags hdrrflags,
                       G_GNUC_UNUSED GError **err)
{
    cr_Package *pkg;

    assert(hdr);
    assert(!err || *err == NULL);

    // Create new package structure

    pkg = cr_package_new();
    pkg->loadingflags |= CR_PACKAGE_FROM_HEADER;
    pkg->loadingflags |= CR_PACKAGE_LOADED_PRI;
    pkg->loadingflags |= CR_PACKAGE_LOADED_FIL;
    pkg->loadingflags |= CR_PACKAGE_LOADED_OTH;


    // Create rpm tag data container

    rpmtd td = rpmtdNew();
    headerGetFlags flags = HEADERGET_MINMEM | HEADERGET_EXT;


    // Fill package structure

    pkg->name = cr_safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_NAME));

    gint64 is_src = headerGetNumber(hdr, RPMTAG_SOURCEPACKAGE);
    if (is_src) {
        pkg->arch = cr_safe_string_chunk_insert(pkg->chunk, "src");
    } else {
        pkg->arch = cr_safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_ARCH));
    }

    pkg->version = cr_safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_VERSION));

#define MAX_STR_INT_LEN 24
    char tmp_epoch[MAX_STR_INT_LEN];
    if (snprintf(tmp_epoch, MAX_STR_INT_LEN, "%llu", (long long unsigned int) headerGetNumber(hdr, RPMTAG_EPOCH)) <= 0) {
        tmp_epoch[0] = '\0';
    }
    pkg->epoch = g_string_chunk_insert_len(pkg->chunk, tmp_epoch, MAX_STR_INT_LEN);

    pkg->release = cr_safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_RELEASE));
    pkg->summary = cr_safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_SUMMARY));
    pkg->description = cr_safe_string_chunk_insert_null(pkg->chunk, headerGetString(hdr, RPMTAG_DESCRIPTION));
    pkg->url = cr_safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_URL));
    if (headerGet(hdr, RPMTAG_BUILDTIME, td, flags)) {
        pkg->time_build = rpmtdGetNumber(td);
    }
    pkg->rpm_license = cr_safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_LICENSE));
    pkg->rpm_vendor = cr_safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_VENDOR));
    pkg->rpm_group = cr_safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_GROUP));
    pkg->rpm_buildhost = cr_safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_BUILDHOST));
    pkg->rpm_sourcerpm = cr_safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_SOURCERPM));
    pkg->rpm_packager = cr_safe_string_chunk_insert(pkg->chunk, headerGetString(hdr, RPMTAG_PACKAGER));
    // RPMTAG_LONGSIZE is allways present (is emulated for small packages because HEADERGET_EXT flag was used)
    if (headerGet(hdr, RPMTAG_LONGSIZE, td, flags)) {
        pkg->size_installed = rpmtdGetNumber(td);
    }
    rpmtdFreeData(td);
    // RPMTAG_LONGARCHIVESIZE is allways present (is emulated for small packages because HEADERGET_EXT flag was used)
    if (headerGet(hdr, RPMTAG_LONGARCHIVESIZE, td, flags)) {
        pkg->size_archive = rpmtdGetNumber(td);
    }

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
            dir_list[x] = cr_safe_string_chunk_insert(pkg->chunk, rpmtdGetString(dirnames));
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
            packagefile->name = cr_safe_string_chunk_insert(pkg->chunk,
                                                         rpmtdGetString(filenames));
            packagefile->path = (dir_list) ? dir_list[(int) rpmtdGetNumber(indexes)] : "";

            if (S_ISDIR(rpmtdGetNumber(filemodes))) {
                // Directory
                packagefile->type = cr_safe_string_chunk_insert(pkg->chunk, "dir");
            } else if (rpmtdGetNumber(fileflags) & RPMFILE_GHOST) {
                // Ghost
                packagefile->type = cr_safe_string_chunk_insert(pkg->chunk, "ghost");
            } else {
                // Regular file
                packagefile->type = cr_safe_string_chunk_insert(pkg->chunk, "");
            }

            g_hash_table_replace(filenames_hashtable,
                                 (gpointer) rpmtdGetString(full_filenames),
                                 (gpointer) rpmtdGetString(full_filenames));
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

    // Struct used as value in ap_hashtable
    struct ap_value_struct {
        const char *flags;
        const char *version;
        int pre;
    };

    // Hastable with filenames from provided
    GHashTable *provided_hashtable = g_hash_table_new_full(g_str_hash,
                                                           g_str_equal,
                                                           g_free,
                                                           NULL);

    // Hashtable with already processed files from requires
    GHashTable *ap_hashtable = g_hash_table_new_full(g_str_hash,
                                                     g_str_equal,
                                                     NULL,
                                                     free);

    for (int deptype=0; dep_items[deptype].type != DEP_SENTINEL; deptype++) {
        if (headerGet(hdr, dep_items[deptype].nametag, filenames, flags) &&
            headerGet(hdr, dep_items[deptype].flagstag, fileflags, flags) &&
            headerGet(hdr, dep_items[deptype].versiontag, fileversions, flags))
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
                const char *flags = cr_flag_to_str(num_flags);
                const char *full_version = rpmtdGetString(fileversions);

                _cleanup_free_ char *depnfv = NULL;  // Dep NameFlagsVersion
                depnfv = g_strconcat(filename,
                                     flags ? flags : "",
                                     full_version ? full_version : "",
                                     NULL);

                // Requires specific stuff
                if (deptype == DEP_REQUIRES) {
                    // Skip requires which start with "rpmlib("
                    if (!strncmp("rpmlib(", filename, 7)) {
                        continue;
                    }

                    // Skip package primary files
                    if (*filename == '/' && g_hash_table_lookup_extended(filenames_hashtable, filename, NULL, NULL)) {
                        if (cr_is_primary(filename)) {
                            continue;
                        }
                    }

                    // Skip files which are provided
                    if (g_hash_table_lookup_extended(provided_hashtable, depnfv, NULL, NULL)) {
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
                            !strcmp(ap_value->version, (full_version ? full_version : "")) &&
                            (ap_value->pre == pre))
                        {
                            continue;
                        }
                    }
                }

                // Parse dep string
                cr_EVR *evr = cr_str_to_evr(full_version, pkg->chunk);
                if ((full_version && *full_version) && !evr->epoch) {
                    // NULL in epoch mean that the epoch was bad (non-numerical)
                    _cleanup_free_ gchar *pkg_nevra = cr_package_nevra(pkg);
                    g_warning("Bad epoch in version string \"%s\" for dependency \"%s\" in package \"%s\"",
                              full_version, filename, pkg_nevra);
                    g_warning("Skipping this dependency");
                    g_free(evr);
                    continue;
                }

                // Create dynamic dependency object
                cr_Dependency *dependency = cr_dependency_new();
                dependency->name = cr_safe_string_chunk_insert(pkg->chunk, filename);
                dependency->flags = cr_safe_string_chunk_insert(pkg->chunk, flags);
                dependency->epoch = evr->epoch;
                dependency->version = evr->version;
                dependency->release = evr->release;
                g_free(evr);

                switch (deptype) {
                    case DEP_PROVIDES: {
                        char *depnfv_dup = g_strdup(depnfv);
                        g_hash_table_replace(provided_hashtable, depnfv_dup, NULL);
                        pkg->provides = g_slist_prepend(pkg->provides, dependency);
                        break;
                    }
                    case DEP_CONFLICTS:
                        pkg->conflicts = g_slist_prepend(pkg->conflicts, dependency);
                        break;
                    case DEP_OBSOLETES:
                        pkg->obsoletes = g_slist_prepend(pkg->obsoletes, dependency);
                        break;
                    case DEP_REQUIRES:
#ifdef ENABLE_LEGACY_WEAKDEPS
                        if ( num_flags & RPMSENSE_MISSINGOK ) {
                            pkg->recommends = g_slist_prepend(pkg->recommends, dependency);
                            break;
                        }
#endif
                        dependency->pre = pre;

                        // XXX: libc.so filtering ////////////////////////////
                        if (g_str_has_prefix(dependency->name, "libc.so.6")) {
                            if (!libc_require_highest)
                                libc_require_highest = dependency;
                            else {
                                if (cr_compare_dependency(libc_require_highest->name,
                                                       dependency->name) == 2)
                                {
                                    g_free(libc_require_highest);
                                    libc_require_highest = dependency;
                                } else
                                    g_free(dependency);
                            }
                            break;
                        }
                        // XXX: libc.so filtering - END ///////////////////////

                        pkg->requires = g_slist_prepend(pkg->requires, dependency);

                        // Add file into ap_hashtable
                        struct ap_value_struct *value = malloc(sizeof(struct ap_value_struct));
                        value->flags = flags;
                        value->version = full_version;
                        value->pre = dependency->pre;
                        g_hash_table_replace(ap_hashtable, dependency->name, value);
                        break; //case REQUIRES end
                    case DEP_SUGGESTS:
                        pkg->suggests = g_slist_prepend(pkg->suggests, dependency);
                        break;
                    case DEP_ENHANCES:
                        pkg->enhances = g_slist_prepend(pkg->enhances, dependency);
                        break;
                    case DEP_RECOMMENDS:
                        pkg->recommends = g_slist_prepend(pkg->recommends, dependency);
                        break;
                    case DEP_SUPPLEMENTS:
                        pkg->supplements = g_slist_prepend(pkg->supplements, dependency);
                        break;
#ifdef ENABLE_LEGACY_WEAKDEPS
                    case DEP_OLDSUGGESTS:
                        if ( num_flags & RPMSENSE_STRONG ) {
                            pkg->recommends = g_slist_prepend(pkg->recommends, dependency);
                        } else {
                            pkg->suggests = g_slist_prepend(pkg->suggests, dependency);
                        }
                        break;
                    case DEP_OLDENHANCES:
                        if ( num_flags & RPMSENSE_STRONG ) {
                            pkg->supplements = g_slist_prepend(pkg->supplements, dependency);
                        } else {
                            pkg->enhances = g_slist_prepend(pkg->enhances, dependency);
                        }
                        break;
                    default:
                        g_warning("Unknown dependency type for dependency: \"%s\" with version: \"%s\"",
                                  dependency->name, dependency->version);
                        g_free(dependency);
#endif
                } // Switch end
            } // While end

            // XXX: libc.so filtering ////////////////////////////////
            if (deptype == DEP_REQUIRES && libc_require_highest)
                pkg->requires = g_slist_prepend(pkg->requires, libc_require_highest);
            // XXX: libc.so filtering - END ////////////////////////////////
        }

        rpmtdFreeData(filenames);
        rpmtdFreeData(fileflags);
        rpmtdFreeData(fileversions);
    }

    pkg->provides    = g_slist_reverse (pkg->provides);
    pkg->conflicts   = g_slist_reverse (pkg->conflicts);
    pkg->obsoletes   = g_slist_reverse (pkg->obsoletes);
    pkg->requires    = g_slist_reverse (pkg->requires);
    pkg->suggests    = g_slist_reverse (pkg->suggests);
    pkg->enhances    = g_slist_reverse (pkg->enhances);
    pkg->recommends  = g_slist_reverse (pkg->recommends);
    pkg->supplements = g_slist_reverse (pkg->supplements);

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
        gint64 last_time = G_GINT64_CONSTANT(0);
        rpmtdInit(changelogtimes);
        rpmtdInit(changelognames);
        rpmtdInit(changelogtexts);
        while ((rpmtdNext(changelogtimes) != -1) &&
               (rpmtdNext(changelognames) != -1) &&
               (rpmtdNext(changelogtexts) != -1) &&
               (changelog_limit > 0 || changelog_limit == -1))
        {
            gint64 time = rpmtdGetNumber(changelogtimes);

            cr_ChangelogEntry *changelog = cr_changelog_entry_new();
            changelog->author    = cr_safe_string_chunk_insert(pkg->chunk,
                                            rpmtdGetString(changelognames));
            changelog->date      = time;
            changelog->changelog = cr_safe_string_chunk_insert(pkg->chunk,
                                            rpmtdGetString(changelogtexts));

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
            if (changelog_limit != -1)
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


    //
    // Keys and hdrid (data used for caching when the --cachedir is specified)
    //

    if (hdrrflags & CR_HDRR_LOADHDRID)
        pkg->hdrid = cr_safe_string_chunk_insert(pkg->chunk,
                                                 headerGetString(hdr, RPMTAG_HDRID));

    if (hdrrflags & CR_HDRR_LOADSIGNATURES) {
        rpmtd gpgtd = rpmtdNew();
        rpmtd pgptd = rpmtdNew();

        if (headerGet(hdr, RPMTAG_SIGGPG, gpgtd, hdrrflags)
            && gpgtd->count > 0)
        {
            pkg->siggpg = cr_binary_data_new();
            pkg->siggpg->size = gpgtd->count;
            pkg->siggpg->data = g_string_chunk_insert_len(pkg->chunk,
                                                          gpgtd->data,
                                                          gpgtd->count);
        }

        if (headerGet(hdr, RPMTAG_SIGPGP, pgptd, hdrrflags)
            && pgptd->count > 0)
        {
            pkg->sigpgp = cr_binary_data_new();
            pkg->sigpgp->size = pgptd->count;
            pkg->sigpgp->data = g_string_chunk_insert_len(pkg->chunk,
                                                          pgptd->data,
                                                          pgptd->count);
        }

        rpmtdFree(gpgtd);
        rpmtdFree(pgptd);
    }

    return pkg;
}
