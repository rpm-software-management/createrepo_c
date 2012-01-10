#include <glib.h>
#include <rpm/rpmfi.h>
#include "parsehdr.h"
#include "xml_dump.h"
#include "misc.h"


Package *parse_header(Header hdr, gint64 mtime, gint64 size, const char *checksum, const char *checksum_type,
                      const char *location_href, const char *location_base, int changelog_limit,
                      gint64 hdr_start, gint64 hdr_end)
{
    Package *pkg = NULL;
    pkg = package_new();

    rpmtd td = rpmtdNew();
    headerGetFlags flags = HEADERGET_MINMEM | HEADERGET_EXT;

    pkg->pkgId = checksum;
    pkg->name = headerGetString(hdr, RPMTAG_NAME);
    pkg->arch = headerGetString(hdr, RPMTAG_ARCH);
    pkg->version = headerGetString(hdr, RPMTAG_VERSION);
    pkg->epoch = headerGetString(hdr, RPMTAG_EPOCH);
    pkg->release = headerGetString(hdr, RPMTAG_RELEASE);
    pkg->summary = headerGetString(hdr, RPMTAG_SUMMARY);
    pkg->description = headerGetString(hdr, RPMTAG_DESCRIPTION);
    pkg->url = headerGetString(hdr, RPMTAG_URL);
    pkg->time_file = mtime;
    if (headerGet(hdr, RPMTAG_BUILDTIME, td, flags)) {
        pkg->time_build = rpmtdGetNumber(td);
    }
    pkg->rpm_license = headerGetString(hdr, RPMTAG_LICENSE);
    pkg->rpm_vendor = headerGetString(hdr, RPMTAG_VENDOR);
    pkg->rpm_group = headerGetString(hdr, RPMTAG_GROUP);
    pkg->rpm_buildhost = headerGetString(hdr, RPMTAG_BUILDHOST);
    pkg->rpm_sourcerpm = headerGetString(hdr, RPMTAG_SOURCERPM);
    pkg->rpm_header_start = hdr_start;
    pkg->rpm_header_end = hdr_end;
    pkg->rpm_packager = headerGetString(hdr, RPMTAG_PACKAGER);
    pkg->size_package = size;
    if (headerGet(hdr, RPMTAG_SIZE, td, flags)) {
        pkg->size_installed = rpmtdGetNumber(td);
    }
    if (headerGet(hdr, RPMTAG_ARCHIVESIZE, td, flags)) {
        pkg->size_archive = rpmtdGetNumber(td);
    }
    pkg->location_href = location_href;
    pkg->location_base = location_base;
    pkg->checksum_type = checksum_type;

    rpmtdFree(td);


    //
    // Files
    //

    rpmtd filenames = rpmtdNew();
    rpmtd fileflags = rpmtdNew();
    rpmtd filemodes = rpmtdNew();

    GHashTable *filenames_hashtable = g_hash_table_new(g_str_hash, g_str_equal);

    if (headerGet(hdr, RPMTAG_FILENAMES, filenames, flags) &&
        headerGet(hdr, RPMTAG_FILEFLAGS, fileflags, flags) &&
        headerGet(hdr, RPMTAG_FILEMODES, filemodes, flags))
    {
        while ((rpmtdNext(filenames) != -1) &&
               (rpmtdNext(fileflags) != -1) &&
               (rpmtdNext(filemodes) != -1))
        {
            PackageFile *packagefile = package_file_new();
            packagefile->name = rpmtdGetString(filenames);

            if (S_ISDIR(rpmtdGetNumber(filemodes))) {
                // Directory
                packagefile->type = "dir";
            } else if (rpmtdGetNumber(fileflags) & RPMFILE_GHOST) {
                // Ghost
                packagefile->type = "ghost";
            } else {
                // Regular file
                packagefile->type = "";
            }

            g_hash_table_insert(filenames_hashtable, packagefile->name, packagefile->name);
            pkg->files = g_slist_append(pkg->files, packagefile);
        }
    }

    rpmtdFree(filemodes);


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
        char *flags;
        char *version;
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
            while ((rpmtdNext(filenames) != -1) &&
                   (rpmtdNext(fileflags) != -1) &&
                   (rpmtdNext(fileversions) != -1))
            {
                int pre = 0;
                char *filename = rpmtdGetString(filenames);
                guint64 num_flags = rpmtdGetNumber(fileflags);
                char *flags = flag_to_string(num_flags);
                char *full_version = rpmtdGetString(fileversions);

                // Requires specific stuff
                if (pcor_type == REQUIRES) {
                    // Skip requires which start with "rpmlib("
                    if (!strncmp("rpmlib(", filename, 7)) {
                        continue;
                    }

                    // Skip package primary files
                    if (g_hash_table_lookup_extended(filenames_hashtable, filename, NULL, NULL)) {
                        if (is_primary(filename)) {
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
                        if (!strcmp(ap_value->flags, flags) &&
                            !strcmp(ap_value->version, full_version) &&
                            (ap_value->pre == pre))
                        {
                            continue;
                        }
                    }
                }

                // Create dynamic dependency object
                Dependency *dependency = dependency_new();
                dependency->name = filename;
                dependency->flags = flags;
                struct VersionStruct ver = string_to_version(full_version, pkg->chunk);
                dependency->epoch = ver.epoch;
                dependency->version = ver.version;
                dependency->release = ver.release;

/*
#ifdef DEBUG
                printf("%s\n", filename);
                printf("%d\n", num_flags);
                printf("%s\n", flags);
                printf("%s\n", full_version);
                if (ver.epoch)
                    printf("%s", ver.epoch);
                printf(" | ");
                if (ver.version)
                    printf("%s", ver.version);
                printf(" | ");
                if (ver.release)
                    printf("%s", ver.release);
                printf("\n");
                printf("-------------------\n");
#endif
*/

                switch (pcor_type) {
                    case PROVIDES:
                        g_hash_table_insert(provided_hashtable, dependency->name, dependency->name);
                        pkg->provides = g_slist_append(pkg->provides, dependency);
                        break;
                    case CONFLICTS:
                        pkg->conflicts = g_slist_append(pkg->conflicts, dependency);
                        break;
                    case OBSOLETES:
                        pkg->obsoletes = g_slist_append(pkg->obsoletes, dependency);
                        break;
                    case REQUIRES:
                        dependency->pre = pre;
                        pkg->requires = g_slist_append(pkg->requires, dependency);

                        // Add file into ap_hashtable
                        struct ap_value_struct *value = malloc(sizeof(struct ap_value_struct));
                        value->flags = dependency->flags;
                        value->version = full_version;
                        value->pre = dependency->pre;
                        g_hash_table_insert(ap_hashtable, dependency->name, value);
                        break;
                }
            }
        }
    }

    g_hash_table_remove_all(filenames_hashtable);
    g_hash_table_remove_all(provided_hashtable);
    g_hash_table_remove_all(ap_hashtable);

    rpmtdFree(filenames);
    rpmtdFree(fileflags);
    rpmtdFree(fileversions);


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
        gint64 time_offset = 1;
        while ((rpmtdNext(changelogtimes) != -1) &&
               (rpmtdNext(changelognames) != -1) &&
               (rpmtdNext(changelogtexts) != -1) &&
               (changelog_limit > 0))
        {
            gint64 time = rpmtdGetNumber(changelogtimes);
            if (last_time == time) {
                time += time_offset;
                time_offset++;
            } else {
                last_time = time;
                time_offset = 1;
            }

            ChangelogEntry *changelog = changelog_entry_new();
            changelog->author    = rpmtdGetString(changelognames);
            changelog->date      = time;
            changelog->changelog = rpmtdGetString(changelogtexts);

            pkg->changelogs = g_slist_append(pkg->changelogs, changelog);
            changelog_limit--;
        }
    }

    rpmtdFree(changelogtimes);
    rpmtdFree(changelognames);
    rpmtdFree(changelogtexts);


    return pkg;
}

struct XmlStruct xml_from_header(Header hdr, gint64 mtime, gint64 size, const char *checksum, const char *checksum_type,
                      const char *location_href, const char *location_base,
                      int changelog_limit, gint64 hdr_start, gint64 hdr_end)
{
    Package *pkg = parse_header(hdr, mtime, size, checksum, checksum_type, location_href,
                                location_base, changelog_limit, hdr_start, hdr_end);

    struct XmlStruct result;
    result.primary = xml_dump_primary(pkg, NULL);
    result.filelists = xml_dump_filelists(pkg, NULL);
    result.other = xml_dump_other(pkg, NULL);
    return result;
}
