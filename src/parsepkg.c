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
#include "error.h"
#include "parsehdr.h"
#include "misc.h"
#include "checksum.h"

rpmts cr_ts = NULL;


static gpointer
cr_package_parser_init_once_cb(gpointer user_data G_GNUC_UNUSED)
{
    rpmReadConfigFiles(NULL, NULL);
    cr_ts = rpmtsCreate();
    if (!cr_ts)
        g_critical("%s: rpmtsCreate() failed", __func__);

    rpmVSFlags vsflags = 0;
    vsflags |= _RPMVSF_NODIGESTS;
    vsflags |= _RPMVSF_NOSIGNATURES;
    vsflags |= RPMVSF_NOHDRCHK;
    rpmtsSetVSFlags(cr_ts, vsflags);
    return NULL;
}

void
cr_package_parser_init()
{
    static GOnce package_parser_init_once = G_ONCE_INIT;
    g_once(&package_parser_init_once, cr_package_parser_init_once_cb, NULL);
}

static gpointer
cr_package_parser_cleanup_once_cb(gpointer user_data G_GNUC_UNUSED)
{
    if (cr_ts) {
        rpmtsFree(cr_ts);
        cr_ts = NULL;
    }

    rpmFreeMacros(NULL);
    rpmFreeRpmrc();
    return NULL;
}

void
cr_package_parser_cleanup()
{
    static GOnce package_parser_cleanup_once = G_ONCE_INIT;
    g_once(&package_parser_cleanup_once, cr_package_parser_cleanup_once_cb, NULL);
}

static int
read_header(const char *filename, Header *hdr, GError **err)
{
    assert(filename);
    assert(!err || *err == NULL);

    FD_t fd = Fopen(filename, "r.ufdio");
    if (!fd) {
        g_warning("%s: Fopen of %s failed %s",
                  __func__, filename, strerror(errno));
        g_set_error(err, CR_PARSEPKG_ERROR, CRE_IO,
                    "Fopen failed: %s", strerror(errno));
        return CRE_IO;
    }

    int rc = rpmReadPackageFile(cr_ts, fd, NULL, hdr);
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
                g_warning("%s: rpmReadPackageFile() error",
                          __func__);
                g_set_error(err, CR_PARSEPKG_ERROR, CRE_IO,
                            "rpmReadPackageFile() error");
                Fclose(fd);
                return CRE_IO;
        }
    }

    Fclose(fd);
    return CRE_OK;
}

cr_Package *
cr_package_from_rpm(const char *filename,
                    cr_ChecksumType checksum_type,
                    const char *location_href,
                    const char *location_base,
                    int changelog_limit,
                    struct stat *stat_buf,
                    GError **err)
{
    cr_Package *pkg = NULL;
    const char *checksum_type_str;
    GError *tmp_err = NULL;

    assert(filename);
    assert(!err || *err == NULL);

    checksum_type_str = cr_checksum_name_str(checksum_type);


    // Read header

    Header hdr;
    read_header(filename, &hdr, &tmp_err);
    if (tmp_err) {
        g_propagate_error(err, tmp_err);
        return NULL;
    }


    // Get file stat

    gint64 mtime;
    gint64 size;

    if (!stat_buf) {
        struct stat stat_buf_own;
        if (stat(filename, &stat_buf_own) == -1) {
            g_warning("%s: stat() error (%s)", __func__, strerror(errno));
            g_set_error(err,  CR_PARSEPKG_ERROR, CRE_IO, "stat() failed");
            headerFree(hdr);
            return NULL;
        }
        mtime  = stat_buf_own.st_mtime;
        size   = stat_buf_own.st_size;
    } else {
        mtime  = stat_buf->st_mtime;
        size   = stat_buf->st_size;
    }


    // Compute checksum

    char *checksum = cr_checksum_file(filename, checksum_type, &tmp_err);
    if (!checksum) {
        g_propagate_prefixed_error(err, tmp_err,
                                   "Error while checksum calculation: ");
        headerFree(hdr);
        return NULL;
    }


    // Get header range

    struct cr_HeaderRangeStruct hdr_r = cr_get_header_byte_range(filename,
                                                                 &tmp_err);
    if (tmp_err) {
        g_propagate_prefixed_error(err, tmp_err,
                                   "Error while determinig header range: ");
        free(checksum);
        return NULL;
    }


    // Get package object

    pkg = cr_package_from_header(hdr, mtime, size, checksum, checksum_type_str,
                             location_href, location_base, changelog_limit,
                             hdr_r.start, hdr_r.end, &tmp_err);
    free(checksum);
    headerFree(hdr);

    if (tmp_err) {
        g_propagate_prefixed_error(err, tmp_err,
                                   "Error while checksum calculation:");
        return NULL;
    }

    return pkg;
}



struct cr_XmlStruct
cr_xml_from_rpm(const char *filename,
                cr_ChecksumType checksum_type,
                const char *location_href,
                const char *location_base,
                int changelog_limit,
                struct stat *stat_buf,
                GError **err)
{
    cr_Package *pkg;
    struct cr_XmlStruct result;

    assert(filename);
    assert(!err || *err == NULL);

    result.primary   = NULL;
    result.filelists = NULL;
    result.other     = NULL;

    pkg = cr_package_from_rpm(filename,
                              checksum_type,
                              location_href,
                              location_base,
                              changelog_limit,
                              stat_buf,
                              err);
    if (!pkg)
        return result;

    result = cr_xml_dump(pkg, err);
    cr_package_free(pkg);
    return result;
}
