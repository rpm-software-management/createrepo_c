#include <glib.h>
#include <rpm/rpmfi.h>
#include "parsehdr.h"
#include "xml_dump.h"
#include "misc.h"
#include "parsehdr.h"


struct XmlStruct xml_from_package_file(const char *filename, const char *checksum_type,
                const char *location_href, const char *location_base, int changelog_limit,
                gint64 hdr_start, gint64 hdr_end)
{
    struct XmlStruct result;
    result.primary   = NULL;
    result.filelists = NULL;
    result.other     = NULL;

    //
    // Get header
    //

    // Create transaction
    rpmts ts = NULL;
    rpmReadConfigFiles(NULL, NULL);
    ts = rpmtsCreate();

    // Open rpm file
    FD_t fd = NULL;
    fd = Fopen(filename, "r.ufdio");
    if (!fd) {
        perror("Fopen");
        return result;
    }

    // Read package
    Header hdr;
    int rc = rpmReadPackageFile(ts, fd, NULL, &hdr);
    if (rc != RPMRC_OK) {
        switch (rc) {
            case RPMRC_NOKEY:
                //printf("rpmReadpackageFile: Public key is unavaliable\n");
                break;
            case RPMRC_NOTTRUSTED:
                //printf("rpmReadpackageFile: Signature is OK, but key is not trusted\n");
                break;
            default:
                perror("rpmReadPackageFile");
                return result;
        }
    }

    //
    // Get file stat
    //

    gint64 mtime = 0;
    gint64 size = 0;
    char *checksum = "abcdefghijklmnop";
    //char *location_href = NULL;
    //char *location_base = NULL;
    //int changelog_limit = 5;
    //gint64 hdr_start = 10;
    //gint64 hdr_end   = 22;

    //
    // Gen XML
    //

    Package *pkg = parse_header(hdr, mtime, size, checksum, checksum_type, location_href,
                                location_base, changelog_limit, hdr_start, hdr_end);

    result.primary = xml_dump_primary(pkg, NULL);
    result.filelists = xml_dump_filelists(pkg, NULL);
    result.other = xml_dump_other(pkg, NULL);
    return result;
}
