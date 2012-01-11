#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <rpm/rpmfi.h>
#include "constants.h"
#include "parsehdr.h"
#include "xml_dump.h"
#include "misc.h"
#include "parsehdr.h"


struct XmlStruct xml_from_package_file(const char *filename, ChecksumType checksum_type,
                const char *location_href, const char *location_base, int changelog_limit)
{
    struct XmlStruct result;
    result.primary   = NULL;
    result.filelists = NULL;
    result.other     = NULL;

    // Get header

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

    // Cleanup
    rpmtsFree(ts);
    Fclose(fd);

    // Get file stat

    struct stat stat_buf;
    if (stat(filename, &stat_buf) == -1) {
        perror("stat");
        return result;
    }
    gint64 mtime  = stat_buf.st_mtime;
    gint64 size   = stat_buf.st_size;

    // Compute checksum

    const char *checksum_type_str;
    switch (checksum_type) {
        case PKG_CHECKSUM_MD5:
            checksum_type_str = "md5";
            break;
        case PKG_CHECKSUM_SHA1:
             checksum_type_str = "sha1";
            break;
        case PKG_CHECKSUM_SHA256:
            checksum_type_str = "sha256";
            break;
    };

    char *checksum = compute_file_checksum(filename, checksum_type);

    // Get header range

    struct HeaderRangeStruct hdr_r = get_header_byte_range(filename);

    // Gen XML

    result = xml_from_header(hdr, mtime, size, checksum, checksum_type_str, location_href,
                                location_base, changelog_limit, hdr_r.start, hdr_r.end);

    // Cleanup
    free(checksum);
    headerFree(hdr);

    return result;
}
