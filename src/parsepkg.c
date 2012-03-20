#include <glib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <rpm/rpmts.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmmacro.h>
#include "logging.h"
#include "constants.h"
#include "parsehdr.h"
#include "xml_dump.h"
#include "misc.h"
#include "parsehdr.h"

#undef MODULE
#define MODULE "parsepkg: "


short initialized = 0;
rpmts ts = NULL;



void init_package_parser()
{
    initialized = 1;
    rpmReadConfigFiles(NULL, NULL);
    ts = rpmtsCreate();
    if (!ts) {
        g_critical(MODULE"%s: rpmtsCreate() failed", __func__);
    }

    rpmVSFlags vsflags = 0;
    vsflags |= _RPMVSF_NODIGESTS;
    vsflags |= _RPMVSF_NOSIGNATURES;
    vsflags |= RPMVSF_NOHDRCHK;
    rpmtsSetVSFlags(ts, vsflags);
}



void free_package_parser()
{
    if (ts) {
        rpmtsFree(ts);
    }

    rpmFreeMacros(NULL);
    rpmFreeRpmrc();
}



struct XmlStruct xml_from_package_file(const char *filename, ChecksumType checksum_type,
                const char *location_href, const char *location_base, int changelog_limit,
                struct stat *stat_buf)
{
    const char *checksum_type_str;

    struct XmlStruct result;
    result.primary   = NULL;
    result.filelists = NULL;
    result.other     = NULL;


    // Set checksum type

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
        default:
            g_critical(MODULE"%s: Unknown checksum type", __func__);
            return result;
            break;
    };


    // Open rpm file

    FD_t fd = NULL;
    fd = Fopen(filename, "r.ufdio");
    if (!fd) {
        g_critical(MODULE"%s: Fopen failed %s", __func__, strerror(errno));
        return result;
    }


    // Read package

    Header hdr;
    int rc = rpmReadPackageFile(ts, fd, NULL, &hdr);
    if (rc != RPMRC_OK) {
        switch (rc) {
            case RPMRC_NOKEY:
                g_debug(MODULE"%s: %s: Public key is unavailable.", __func__, filename);
                break;
            case RPMRC_NOTTRUSTED:
                g_debug(MODULE"%s:  %s: Signature is OK, but key is not trusted.", __func__, filename);
                break;
            default:
                g_critical(MODULE"%s: rpmReadPackageFile() error (%s)", __func__, strerror(errno));
                return result;
        }
    }


    // Cleanup

    Fclose(fd);


    // Get file stat

    gint64 mtime;
    gint64 size;

    if (!stat_buf) {
        struct stat stat_buf_own;
        if (stat(filename, &stat_buf_own) == -1) {
            g_critical(MODULE"%s: stat() error (%s)", __func__, strerror(errno));
            return result;
        }
        mtime  = stat_buf_own.st_mtime;
        size   = stat_buf_own.st_size;
    } else {
        mtime  = stat_buf->st_mtime;
        size   = stat_buf->st_size;
    }


    // Compute checksum

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
