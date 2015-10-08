/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
 * Copyright (C) 2007  James Bowes
 * Copyright (C) 2006  Seth Vidal
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

#ifndef __C_CREATEREPOLIB_PACKAGE_H__
#define __C_CREATEREPOLIB_PACKAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>

/** \defgroup   package         Package representation.
 *  \addtogroup package
 *  @{
 */

typedef enum {
    CR_PACKAGE_FROM_HEADER  = (1<<1),   /*!< Metadata parsed from header */
    CR_PACKAGE_FROM_XML     = (1<<2),   /*!< Metadata parsed xml */
    /* Some values are reserved (for sqlite, solv, etc..) */
    CR_PACKAGE_LOADED_PRI   = (1<<10),  /*!< Primary metadata was loaded */
    CR_PACKAGE_LOADED_FIL   = (1<<11),  /*!< Filelists metadata was loaded */
    CR_PACKAGE_LOADED_OTH   = (1<<12),  /*!< Other metadata was loaded */
    CR_PACKAGE_SINGLE_CHUNK = (1<<13),  /*!< Package uses single chunk */
} cr_PackageLoadingFlags;

/** Dependency (Provides, Conflicts, Obsoletes, Requires).
 */
typedef struct {
    char *name;                 /*!< name */
    char *flags;                /*!< flags (value returned by cr_flag_to_str()
                                     from misc module) */
    char *epoch;                /*!< epoch */
    char *version;              /*!< version */
    char *release;              /*!< release */
    gboolean pre;               /*!< preinstall */
} cr_Dependency;

/** File in package.
 */
typedef struct {
    char *type;                 /*!< one of "" (regular file), "dir", "ghost" */
    char *path;                 /*!< path to file */
    char *name;                 /*!< filename */
} cr_PackageFile;

/** Changelog entry.
 */
typedef struct {
    char *author;               /*!< author of changelog */
    gint64 date;                /*!< date of changelog - seconds since epoch */
    char *changelog;            /*!< text of changelog */
} cr_ChangelogEntry;

/** Binary data.
 */
typedef struct {
    void *data;
    gsize size;
} cr_BinaryData;

/** Package
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

    GStringChunk *chunk;        /*!< string chunk for store all package strings
                                     on the single place */

    cr_PackageLoadingFlags loadingflags; /*!<
        Bitfield flags with information about package loading  */
} cr_Package;

/** Create new (empty) dependency structure.
 * @return              new empty cr_Dependency
 */
cr_Dependency *cr_dependency_new(void);

/** Create new (empty) package file structure.
 * @return              new emtpy cr_PackageFile
 */
cr_PackageFile *cr_package_file_new(void);

/** Create new (empty) changelog structure.
 * @return              new empty cr_ChangelogEntry
 */
cr_ChangelogEntry *cr_changelog_entry_new(void);

/** Create new (empty) structure for binary data
 * @return              new mepty cr_BinaryData
 */
cr_BinaryData *cr_binary_data_new(void);

/** Create new (empty) package structure.
 * @return              new empty cr_Package
 */
cr_Package *cr_package_new(void);

/** Create new (empty) package structure without initialized string chunk.
 * @return              new empty cr_Package
 */
cr_Package *cr_package_new_without_chunk(void);

/** Free package structure and all its structures.
 * @param package       cr_Package
 */
void cr_package_free(cr_Package *package);

/** Get NVRA package string
 * @param package       cr_Package
 * @return              nvra string
 */
gchar *cr_package_nvra(cr_Package *package);

/** Get NEVRA package string
 * @param package       cr_Package
 * @return              nevra string
 */
gchar *cr_package_nevra(cr_Package *package);

/** Create a standalone copy of the package.
 * @param package       cr_Package
 * @return              copy of the package
 */
cr_Package *cr_package_copy(cr_Package *package);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_PACKAGE_H__ */
