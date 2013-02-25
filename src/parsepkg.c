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
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <rpm/rpmts.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmkeyring.h>
#include "logging.h"
#include "constants.h"
#include "parsehdr.h"
#include "xml_dump.h"
#include "misc.h"
#include "parsehdr.h"

volatile short cr_initialized = 0;
rpmts ts = NULL;


void
cr_package_parser_init()
{
    rpmKeyring keyring;

    if (cr_initialized)
        return;
    cr_initialized = 1;
    rpmReadConfigFiles(NULL, NULL);
    ts = rpmtsCreate();
    if (!ts)
        g_critical("%s: rpmtsCreate() failed", __func__);

    rpmVSFlags vsflags = 0;
    vsflags |= _RPMVSF_NODIGESTS;
    vsflags |= _RPMVSF_NOSIGNATURES;
    vsflags |= RPMVSF_NOHDRCHK;
    rpmtsSetVSFlags(ts, vsflags);

    // Set empty keyring
    // Why? Because RPM is not thread-safe. Not only a little bit.
    // RPM uses some internal states which makes it thread-*un*safe.
    // This includes also reading the headers.
    // Work around for this shoud be use empty keyring.
    keyring = rpmKeyringNew();
    if (rpmtsSetKeyring(ts, keyring) == -1)
        g_critical("%s: rpmtsSetKeyring() failed", __func__);
    rpmKeyringFree(keyring);
}


void
cr_package_parser_shutdown()
{
    if (ts)
        rpmtsFree(ts);

    rpmFreeMacros(NULL);
    rpmFreeRpmrc();
}


cr_Package *
cr_package_from_file(const char *filename,
                     cr_ChecksumType checksum_type,
                     const char *location_href,
                     const char *location_base,
                     int changelog_limit,
                     struct stat *stat_buf)
{
    cr_Package *result = NULL;
    const char *checksum_type_str;

    // Set checksum type

    switch (checksum_type) {
        case CR_CHECKSUM_MD5:
            checksum_type_str = "md5";
            break;
        case CR_CHECKSUM_SHA1:
             checksum_type_str = "sha1";
            break;
        case CR_CHECKSUM_SHA256:
            checksum_type_str = "sha256";
            break;
        default:
            g_warning("%s: Unknown checksum type", __func__);
            return result;
            break;
    };


    // Open rpm file

    FD_t fd = NULL;
    fd = Fopen(filename, "r.ufdio");
    if (!fd) {
        g_warning("%s: Fopen of %s failed %s",
                  __func__, filename, strerror(errno));
        return result;
    }


    // Read package

    Header hdr;
    int rc = rpmReadPackageFile(ts, fd, NULL, &hdr);
    if (rc != RPMRC_OK) {
        switch (rc) {
            case RPMRC_NOKEY:
                g_debug("%s: %s: Public key is unavailable.",
                        __func__, filename);
                break;
            case RPMRC_NOTTRUSTED:
                g_debug("%s:  %s: Signature is OK, but key is not trusted.",
                        __func__, filename);
                break;
            default:
                g_warning("%s: rpmReadPackageFile() error (%s)",
                          __func__, strerror(errno));
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
            g_warning("%s: stat() error (%s)", __func__, strerror(errno));
            return result;
        }
        mtime  = stat_buf_own.st_mtime;
        size   = stat_buf_own.st_size;
    } else {
        mtime  = stat_buf->st_mtime;
        size   = stat_buf->st_size;
    }


    // Compute checksum

    char *checksum = cr_compute_file_checksum(filename, checksum_type);


    // Get header range

    struct cr_HeaderRangeStruct hdr_r = cr_get_header_byte_range(filename);


    // Get package object

    result = cr_parse_header(hdr, mtime, size, checksum, checksum_type_str,
                             location_href, location_base, changelog_limit,
                             hdr_r.start, hdr_r.end);


    // Cleanup

    free(checksum);
    headerFree(hdr);

    return result;
}



struct cr_XmlStruct
cr_xml_from_package_file(const char *filename,
                         cr_ChecksumType checksum_type,
                         const char *location_href,
                         const char *location_base,
                         int changelog_limit,
                         struct stat *stat_buf)
{
    const char *checksum_type_str;

    struct cr_XmlStruct result;
    result.primary   = NULL;
    result.filelists = NULL;
    result.other     = NULL;


    // Set checksum type

    switch (checksum_type) {
        case CR_CHECKSUM_MD5:
            checksum_type_str = "md5";
            break;
        case CR_CHECKSUM_SHA1:
             checksum_type_str = "sha1";
            break;
        case CR_CHECKSUM_SHA256:
            checksum_type_str = "sha256";
            break;
        default:
            g_warning("%s: Unknown checksum type", __func__);
            return result;
            break;
    };


    // Open rpm file

    FD_t fd = NULL;
    fd = Fopen(filename, "r.ufdio");
    if (!fd) {
        g_warning("%s: Fopen failed %s", __func__, strerror(errno));
        return result;
    }


    // Read package

    Header hdr;
    int rc = rpmReadPackageFile(ts, fd, NULL, &hdr);
    if (rc != RPMRC_OK) {
        switch (rc) {
            case RPMRC_NOKEY:
                g_debug("%s: %s: Public key is unavailable.",
                        __func__, filename);
                break;
            case RPMRC_NOTTRUSTED:
                g_debug("%s:  %s: Signature is OK, but key is not trusted.",
                        __func__, filename);
                break;
            default:
                g_warning("%s: rpmReadPackageFile() error (%s)",
                          __func__, strerror(errno));
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
            g_warning("%s: stat() error (%s)", __func__, strerror(errno));
            return result;
        }
        mtime  = stat_buf_own.st_mtime;
        size   = stat_buf_own.st_size;
    } else {
        mtime  = stat_buf->st_mtime;
        size   = stat_buf->st_size;
    }


    // Compute checksum

    char *checksum = cr_compute_file_checksum(filename, checksum_type);


    // Get header range

    struct cr_HeaderRangeStruct hdr_r = cr_get_header_byte_range(filename);


    // Gen XML

    result = cr_xml_from_header(hdr, mtime, size, checksum, checksum_type_str,
                                location_href, location_base, changelog_limit,
                                hdr_r.start, hdr_r.end);


    // Cleanup

    free(checksum);
    headerFree(hdr);

    return result;
}
