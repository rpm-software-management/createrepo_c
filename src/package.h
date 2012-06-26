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

#ifndef __C_CREATEREPOLIB_PACKAGE_H__
#define __C_CREATEREPOLIB_PACKAGE_H__

#include <glib.h>

/** \defgroup   package         Package representation.
 */

/** \ingroup package
 * Dependency (Provides, Conflicts, Obsoletes, Requires).
 */
typedef struct {
    char *name;                 /*!< name */
    char *flags;                /*!< flags (value returned by flag_to_string() from misc module) */
    char *epoch;                /*!< epoch */
    char *version;              /*!< version */
    char *release;              /*!< release */
    gboolean pre;               /*!< preinstall */
} Dependency;

/** \ingroup package
 * File in package.
 */
typedef struct {
    char *type;                 /*!< one of "" (regular file), "dir", "ghost" */
    char *path;                 /*!< path to file */
    char *name;                 /*!< filename */
} PackageFile;

/** \ingroup package
 * Changelog entry.
 */
typedef struct {
    char *author;               /*!< author of changelog */
    gint64 date;                /*!< date of changelog in seconds since epoch */
    char *changelog;            /*!< text of changelog */
} ChangelogEntry;

/** \ingroup package
 * Package
 */
typedef struct {
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
    char *rpm_group;            /*!< group (one value from /usr/share/doc/rpm-(your_rpm_version)/GROUPS) */
    char *rpm_buildhost;        /*!< hostname of machine which builds the package */
    char *rpm_sourcerpm;        /*!< name of srpms */
    gint64 rpm_header_start;    /*!< start byte of header in rpm */
    gint64 rpm_header_end;      /*!< end byte of header in rpm */
    char *rpm_packager;         /*!< packager of package */
    gint64 size_package;        /*!< size of rpm package */
    gint64 size_installed;      /*!< size of installed files */
    gint64 size_archive;        /*!< size of archive (I have no idea what does it mean) */
    char *location_href;        /*!< file location inside repository */
    char *location_base;        /*!< location (url) of repository */
    char *checksum_type;        /*!< type of checksum used ("sha1", "sha256", "md5", ..) */

    GSList *requires;           /*!< requires (list of Dependency structs) */
    GSList *provides;           /*!< provides (list of Dependency structs) */
    GSList *conflicts;          /*!< conflicts (list of Dependency structs) */
    GSList *obsoletes;          /*!< obsoletes (list of Dependency structs) */

    GSList *files;              /*!< files in the package (list of PackageFile structs) */
    GSList *changelogs;         /*!< changelogs (list of ChangelogEntry structs) */

    GStringChunk *chunk;        /*!< string chunk for store all package strings on the single place */
} Package;

typedef void (*PackageFn) (Package *pkg, gpointer data);

/** \ingroup package
 * Create new (empty) dependency structure.
 * @return              new empty Dependency
 */
Dependency     *dependency_new      (void);

/** \ingroup package
 * Create new (empty) package file structure.
 * @return              new emtpy PackageFile
 */
PackageFile    *package_file_new    (void);

/** \ingroup package
 * Create new (empty) changelog structure.
 * @return              new empty ChangelogEntry
 */
ChangelogEntry *changelog_entry_new (void);

/** \ingroup package
 * Create new (empty) package structure.
 * @return              new empty Package
 */
Package        *package_new         (void);

/** \ingroup package
 * Free package structure and all its structures.
 * @param package       Package
 */
void            package_free        (Package *package);

#endif /* __C_CREATEREPOLIB_PACKAGE_H__ */
