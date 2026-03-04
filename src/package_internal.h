/*
 * Copyright (C) 2021 Red Hat, Inc.
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __C_CREATEREPOLIB_PACKAGE_INTERNAL_H__
#define __C_CREATEREPOLIB_PACKAGE_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "package.h"

/** Package
 */
struct _cr_Package {
    gint64 pkgKey;              /*!< used while inserting into sqlite db */
    char *pkgId;                /*!< package hash */
    char *name;                 /*!< name */
    char *arch;                 /*!< architecture */
    char *version;              /*!< version */
    char *epoch;                /*!< epoch */
    char *release;              /*!< release */
    char *summary;              /*!< summary */
    char *description;          /*!< description */
    char *url;                  /*!< package homepage */
    gint64 time_file;           /*!< mtime of file */
    gint64 time_build;          /*!< build time (from rpm header) */
    char *rpm_license;          /*!< license */
    char *rpm_vendor;           /*!< vendor */
    char *rpm_group;            /*!< group (one value from /usr/share/doc/rpm-
                                     (your_rpm_version)/GROUPS) */
    char *rpm_buildhost;        /*!< hostname of machine which builds
                                     the package */
    char *rpm_sourcerpm;        /*!< name of srpms */
    gint64 rpm_header_start;    /*!< start byte of header in rpm */
    gint64 rpm_header_end;      /*!< end byte of header in rpm */
    char *rpm_packager;         /*!< packager of package */
    gint64 size_package;        /*!< size of rpm package */
    gint64 size_installed;      /*!< size of installed files */
    gint64 size_archive;        /*!< size of archive (I have no idea what does
                                     it mean) */
    char *location_href;        /*!< file location inside repository */
    char *location_base;        /*!< location (url) of repository */
    char *checksum_type;        /*!< type of checksum used ("sha1", "sha256",
                                     "md5", ..) */
    char *files_checksum_type;  /*!< type of checksum used for files ("sha1",
                                     "sha256", "md5", ..) */

    GSList *requires;           /*!< requires (list of cr_Dependency structs) */
    GSList *provides;           /*!< provides (list of cr_Dependency structs) */
    GSList *conflicts;          /*!< conflicts (list of cr_Dependency structs) */
    GSList *obsoletes;          /*!< obsoletes (list of cr_Dependency structs) */
    GSList *suggests;           /*!< suggests (list of cr_Dependency structs) */
    GSList *enhances;           /*!< enhances (list of cr_Dependency structs) */
    GSList *recommends;         /*!< recommends (list of cr_Dependency structs) */
    GSList *supplements;        /*!< supplements (list of cr_Dependency structs) */

    GSList *files;              /*!< files in the package (list of
                                     cr_PackageFile structs) */
    GSList *changelogs;         /*!< changelogs (list of cr_ChangelogEntry
                                     structs) */

    char *hdrid;
    cr_BinaryData *siggpg;
    cr_BinaryData *sigpgp;

    GSList *signatures;         /*!< signatures (list of char*)*/

    GStringChunk *chunk;        /*!< string chunk for store all package strings
                                     on the single place */

    cr_PackageLoadingFlags loadingflags; /*!<
        Bitfield flags with information about package loading  */
    gboolean skip_dump;         /*!<  Don't dump this package to metadata. */
};

/** Copy package data into specified package (overriding its data)
 * @param source        cr_Package
 * @param target        cr_Package
 */
void cr_package_copy_into(cr_Package *source, cr_Package *target);

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_PACKAGE_INTERNAL_H__ */
