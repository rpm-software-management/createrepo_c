#include <rpm/rpmfi.h>
#include "parsehdr.h"
#include "xml_dump.h"
#include "misc.h"


Package *parse_header(Header hdr, gint64 mtime, gint64 size, const char *checksum, const char *checksum_type,
                      const char *location_href, const char *location_base,
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

    // Files
    rpmtd filenames = rpmtdNew();
    rpmtd fileflags = rpmtdNew();
    rpmtd filemodes = rpmtdNew();

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

            pkg->files = g_slist_append(pkg->files, packagefile);
        }
    }

    //rpmtdFree(filenames);
    //rpmtdFree(fileflags);
    rpmtdFree(filemodes);

    // PCOR (provides, conflicts, obsoletes, requires)
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

    int x;
    for (x=0; x <= REQUIRES; x++) {
        printf("### %d | %d | %d | %d\n", x, file_tags[x], flag_tags[x], version_tags[x]);
        if (headerGet(hdr, file_tags[x], filenames, flags) &&
            headerGet(hdr, flag_tags[x], fileflags, flags) &&
            headerGet(hdr, version_tags[x], fileversions, flags))
        {
            while ((rpmtdNext(filenames) != -1) &&
                   (rpmtdNext(fileflags) != -1) &&
                   (rpmtdNext(fileversions) != -1))
            {
                printf("%s\n", rpmtdGetString(filenames));
                printf("%d\n", rpmtdGetNumber(fileflags));
                printf("%s\n", flag_to_string(rpmtdGetNumber(fileflags)));
                printf("%s\n", rpmtdGetString(fileversions));
                struct VersionStruct ver = string_to_version(rpmtdGetString(fileversions));
                printf("%s | %s | %s\n", ver.epoch, ver.version, ver.release);
            }
        }
    }



    return pkg;
}

struct XmlStruct xml_from_header(Header hdr, gint64 mtime, gint64 size, const char *checksum, const char *checksum_type,
                      const char *location_href, const char *location_base, gint64 hdr_start, gint64 hdr_end)
{
    Package *pkg = parse_header(hdr, mtime, size, checksum, checksum_type, location_href, location_base, hdr_start, hdr_end);

    struct XmlStruct result;
    result.primary = xml_dump_primary(pkg, NULL);
    result.filelists = xml_dump_filelists(pkg, NULL);
    result.other = xml_dump_other(pkg, NULL);
    return result;
}
