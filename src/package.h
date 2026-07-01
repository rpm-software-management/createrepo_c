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
    CR_PACKAGE_LOADED_FIL   = (1<<11),  /*!< Filelists[_ext] metadata was loaded */
    CR_PACKAGE_LOADED_OTH   = (1<<12),  /*!< Other metadata was loaded */
    CR_PACKAGE_SINGLE_CHUNK = (1<<13),  /*!< Package shares a single chunk with others */
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
    char *digest;               /*!< file checksum */
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

/** Opaque package structure.
 *
 * The layout is not part of the public ABI.  Use the public API functions
 * to work with packages.
 */
typedef struct _cr_Package cr_Package;

/** Create new (empty) dependency structure.
 * Ownership: transferred to the caller (free with g_free()).
 * @return              new empty cr_Dependency
 */
cr_Dependency *cr_dependency_new(void);

/** Create new (empty) package file structure.
 * Ownership: transferred to the caller (free with g_free()).
 * @return              new emtpy cr_PackageFile
 */
cr_PackageFile *cr_package_file_new(void);

/** Create new (empty) changelog structure.
 * Ownership: transferred to the caller (free with g_free()).
 * @return              new empty cr_ChangelogEntry
 */
cr_ChangelogEntry *cr_changelog_entry_new(void);

/** Create new (empty) structure for binary data
 * Ownership: transferred to the caller (free with g_free()).
 * @return              new mepty cr_BinaryData
 */
cr_BinaryData *cr_binary_data_new(void);

/** Create new (empty) package structure.
 * Ownership: transferred to the caller (free with cr_package_free()).
 * @return              new empty cr_Package
 */
cr_Package *cr_package_new(void);

/** Create new (empty) package structure without initialized string chunk.
 * Ownership: transferred to the caller (free with cr_package_free()).
 * @return              new empty cr_Package
 */
cr_Package *cr_package_new_without_chunk(void);

/** Free package structure and all its structures.
 * @param package       cr_Package
 */
void cr_package_free(cr_Package *package);

/** Get NVRA package string
 * Ownership: transferred to the caller (free with g_free()).
 * @param package       cr_Package
 * @return              nvra string
 */
gchar *cr_package_nvra(cr_Package *package);

/** Get NEVRA package string
 * Ownership: transferred to the caller (free with g_free()).
 * @param package       cr_Package
 * @return              nevra string
 */
gchar *cr_package_nevra(cr_Package *package);

/** Create a standalone copy of the package.
 * Ownership: transferred to the caller (free with cr_package_free()).
 * @param package       cr_Package
 * @return              copy of the package
 */
cr_Package *cr_package_copy(cr_Package *package);

/** Get package hash.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              package hash or NULL
 */
const char *cr_package_get_pkg_id(cr_Package *package);

/** Set package hash.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         package hash
 */
void cr_package_set_pkg_id(cr_Package *package, const char *value);

/** Get name.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              name or NULL
 */
const char *cr_package_get_name(cr_Package *package);

/** Set name.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         name
 */
void cr_package_set_name(cr_Package *package, const char *value);

/** Get architecture.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              architecture or NULL
 */
const char *cr_package_get_arch(cr_Package *package);

/** Set architecture.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         architecture
 */
void cr_package_set_arch(cr_Package *package, const char *value);

/** Get version.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              version or NULL
 */
const char *cr_package_get_version(cr_Package *package);

/** Set version.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         version
 */
void cr_package_set_version(cr_Package *package, const char *value);

/** Get epoch.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              epoch or NULL
 */
const char *cr_package_get_epoch(cr_Package *package);

/** Set epoch.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         epoch
 */
void cr_package_set_epoch(cr_Package *package, const char *value);

/** Get release.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              release or NULL
 */
const char *cr_package_get_release(cr_Package *package);

/** Set release.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         release
 */
void cr_package_set_release(cr_Package *package, const char *value);

/** Get summary.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              summary or NULL
 */
const char *cr_package_get_summary(cr_Package *package);

/** Set summary.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         summary
 */
void cr_package_set_summary(cr_Package *package, const char *value);

/** Get description.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              description or NULL
 */
const char *cr_package_get_description(cr_Package *package);

/** Set description.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         description
 */
void cr_package_set_description(cr_Package *package, const char *value);

/** Get package homepage.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              package homepage or NULL
 */
const char *cr_package_get_url(cr_Package *package);

/** Set package homepage.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         package homepage
 */
void cr_package_set_url(cr_Package *package, const char *value);

/** Get license.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              license or NULL
 */
const char *cr_package_get_rpm_license(cr_Package *package);

/** Set license.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         license
 */
void cr_package_set_rpm_license(cr_Package *package, const char *value);

/** Get vendor.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              vendor or NULL
 */
const char *cr_package_get_rpm_vendor(cr_Package *package);

/** Set vendor.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         vendor
 */
void cr_package_set_rpm_vendor(cr_Package *package, const char *value);

/** Get group.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              group or NULL
 */
const char *cr_package_get_rpm_group(cr_Package *package);

/** Set group.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         group
 */
void cr_package_set_rpm_group(cr_Package *package, const char *value);

/** Get hostname of machine which builds the package.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              build host or NULL
 */
const char *cr_package_get_rpm_buildhost(cr_Package *package);

/** Set hostname of machine which builds the package.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         build host
 */
void cr_package_set_rpm_buildhost(cr_Package *package, const char *value);

/** Get name of srpms.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              source rpm name or NULL
 */
const char *cr_package_get_rpm_sourcerpm(cr_Package *package);

/** Set name of srpms.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         source rpm name
 */
void cr_package_set_rpm_sourcerpm(cr_Package *package, const char *value);

/** Get packager of package.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              packager or NULL
 */
const char *cr_package_get_rpm_packager(cr_Package *package);

/** Set packager of package.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         packager
 */
void cr_package_set_rpm_packager(cr_Package *package, const char *value);

/** Get file location inside repository.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              location href or NULL
 */
const char *cr_package_get_location_href(cr_Package *package);

/** Set file location inside repository.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         location href
 */
void cr_package_set_location_href(cr_Package *package, const char *value);

/** Get location (url) of repository.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              location base or NULL
 */
const char *cr_package_get_location_base(cr_Package *package);

/** Set location (url) of repository.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         location base
 */
void cr_package_set_location_base(cr_Package *package, const char *value);

/** Get type of checksum used ("sha1", "sha256", "md5", ..).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              checksum type or NULL
 */
const char *cr_package_get_checksum_type(cr_Package *package);

/** Set type of checksum used ("sha1", "sha256", "md5", ..).
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         checksum type
 */
void cr_package_set_checksum_type(cr_Package *package, const char *value);

/** Get type of checksum used for files ("sha1", "sha256", "md5", ..).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              files checksum type or NULL
 */
const char *cr_package_get_files_checksum_type(cr_Package *package);

/** Set type of checksum used for files ("sha1", "sha256", "md5", ..).
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         files checksum type
 */
void cr_package_set_files_checksum_type(cr_Package *package, const char *value);

/** Get header id.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              header id or NULL
 */
const char *cr_package_get_hdrid(cr_Package *package);

/** Set header id.
 * Ownership: not transferred; value is copied.
 * @param package       cr_Package
 * @param value         header id
 */
void cr_package_set_hdrid(cr_Package *package, const char *value);

/** Get key used while inserting into sqlite db.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              package key
 */
gint64 cr_package_get_pkg_key(cr_Package *package);

/** Set key used while inserting into sqlite db.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @param value         package key
 */
void cr_package_set_pkg_key(cr_Package *package, gint64 value);

/** Get mtime of file.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              file mtime
 */
gint64 cr_package_get_time_file(cr_Package *package);

/** Set mtime of file.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @param value         file mtime
 */
void cr_package_set_time_file(cr_Package *package, gint64 value);

/** Get build time (from rpm header).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              build time
 */
gint64 cr_package_get_time_build(cr_Package *package);

/** Set build time (from rpm header).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @param value         build time
 */
void cr_package_set_time_build(cr_Package *package, gint64 value);

/** Get start byte of header in rpm.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              rpm header start
 */
gint64 cr_package_get_rpm_header_start(cr_Package *package);

/** Set start byte of header in rpm.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @param value         rpm header start
 */
void cr_package_set_rpm_header_start(cr_Package *package, gint64 value);

/** Get end byte of header in rpm.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              rpm header end
 */
gint64 cr_package_get_rpm_header_end(cr_Package *package);

/** Set end byte of header in rpm.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @param value         rpm header end
 */
void cr_package_set_rpm_header_end(cr_Package *package, gint64 value);

/** Get size of rpm package.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              package size
 */
gint64 cr_package_get_size_package(cr_Package *package);

/** Set size of rpm package.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @param value         package size
 */
void cr_package_set_size_package(cr_Package *package, gint64 value);

/** Get size of installed files.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              installed size
 */
gint64 cr_package_get_size_installed(cr_Package *package);

/** Set size of installed files.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @param value         installed size
 */
void cr_package_set_size_installed(cr_Package *package, gint64 value);

/** Get size of archive.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              archive size
 */
gint64 cr_package_get_size_archive(cr_Package *package);

/** Set size of archive.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @param value         archive size
 */
void cr_package_set_size_archive(cr_Package *package, gint64 value);

/** Get bitfield flags with information about package loading.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              loading flags
 */
cr_PackageLoadingFlags cr_package_get_loading_flags(cr_Package *package);

/** Set bitfield flags with information about package loading.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @param flags         loading flags
 */
void cr_package_set_loading_flags(cr_Package *package, cr_PackageLoadingFlags flags);

/** Get whether to skip dumping this package to metadata.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              TRUE if the package should not be dumped
 */
gboolean cr_package_get_skip_dump(cr_Package *package);

/** Set whether to skip dumping this package to metadata.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @param skip_dump     skip dump flag
 */
void cr_package_set_skip_dump(cr_Package *package, gboolean skip_dump);

/** Get requires (list of cr_Dependency structs).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              requires list or NULL
 */
GSList *cr_package_get_requires(cr_Package *package);

/** Set requires (list of cr_Dependency structs).
 * Ownership: list is transferred to the package.
 * @param package       cr_Package
 * @param list          requires list
 */
void cr_package_set_requires(cr_Package *package, GSList *list);

/** Get provides (list of cr_Dependency structs).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              provides list or NULL
 */
GSList *cr_package_get_provides(cr_Package *package);

/** Set provides (list of cr_Dependency structs).
 * Ownership: list is transferred to the package.
 * @param package       cr_Package
 * @param list          provides list
 */
void cr_package_set_provides(cr_Package *package, GSList *list);

/** Get conflicts (list of cr_Dependency structs).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              conflicts list or NULL
 */
GSList *cr_package_get_conflicts(cr_Package *package);

/** Set conflicts (list of cr_Dependency structs).
 * Ownership: list is transferred to the package.
 * @param package       cr_Package
 * @param list          conflicts list
 */
void cr_package_set_conflicts(cr_Package *package, GSList *list);

/** Get obsoletes (list of cr_Dependency structs).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              obsoletes list or NULL
 */
GSList *cr_package_get_obsoletes(cr_Package *package);

/** Set obsoletes (list of cr_Dependency structs).
 * Ownership: list is transferred to the package.
 * @param package       cr_Package
 * @param list          obsoletes list
 */
void cr_package_set_obsoletes(cr_Package *package, GSList *list);

/** Get suggests (list of cr_Dependency structs).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              suggests list or NULL
 */
GSList *cr_package_get_suggests(cr_Package *package);

/** Set suggests (list of cr_Dependency structs).
 * Ownership: list is transferred to the package.
 * @param package       cr_Package
 * @param list          suggests list
 */
void cr_package_set_suggests(cr_Package *package, GSList *list);

/** Get enhances (list of cr_Dependency structs).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              enhances list or NULL
 */
GSList *cr_package_get_enhances(cr_Package *package);

/** Set enhances (list of cr_Dependency structs).
 * Ownership: list is transferred to the package.
 * @param package       cr_Package
 * @param list          enhances list
 */
void cr_package_set_enhances(cr_Package *package, GSList *list);

/** Get recommends (list of cr_Dependency structs).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              recommends list or NULL
 */
GSList *cr_package_get_recommends(cr_Package *package);

/** Set recommends (list of cr_Dependency structs).
 * Ownership: list is transferred to the package.
 * @param package       cr_Package
 * @param list          recommends list
 */
void cr_package_set_recommends(cr_Package *package, GSList *list);

/** Get supplements (list of cr_Dependency structs).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              supplements list or NULL
 */
GSList *cr_package_get_supplements(cr_Package *package);

/** Set supplements (list of cr_Dependency structs).
 * Ownership: list is transferred to the package.
 * @param package       cr_Package
 * @param list          supplements list
 */
void cr_package_set_supplements(cr_Package *package, GSList *list);

/** Get files in the package (list of cr_PackageFile structs).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              files list or NULL
 */
GSList *cr_package_get_files(cr_Package *package);

/** Set files in the package (list of cr_PackageFile structs).
 * Ownership: list is transferred to the package.
 * @param package       cr_Package
 * @param list          files list
 */
void cr_package_set_files(cr_Package *package, GSList *list);

/** Get changelogs (list of cr_ChangelogEntry structs).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              changelogs list or NULL
 */
GSList *cr_package_get_changelogs(cr_Package *package);

/** Set changelogs (list of cr_ChangelogEntry structs).
 * Ownership: list is transferred to the package.
 * @param package       cr_Package
 * @param list          changelogs list
 */
void cr_package_set_changelogs(cr_Package *package, GSList *list);

/** Get signatures (list of signature strings).
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              signatures list or NULL
 */
GSList *cr_package_get_signatures(cr_Package *package);

/** DEPRECATED: Get OpenPGP signature. Use cr_package_get_signatures() instead.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              OpenPGP signature or NULL
 */
cr_BinaryData *cr_package_get_siggpg(cr_Package *package);

/** DEPRECATED: Set OpenPGP signature.
 * Ownership: data is transferred to the package.
 * @param package       cr_Package
 * @param data          OpenPGP signature
 */
void cr_package_set_siggpg(cr_Package *package, cr_BinaryData *data);

/** DEPRECATED: Get PGP signature. Use cr_package_get_signatures() instead.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              PGP signature or NULL
 */
cr_BinaryData *cr_package_get_sigpgp(cr_Package *package);

/**DEPRECATED:  Set PGP signature.
 * Ownership: data is transferred to the package.
 * @param package       cr_Package
 * @param data          PGP signature
 */
void cr_package_set_sigpgp(cr_Package *package, cr_BinaryData *data);

/** Get string chunk used to store all package strings in one place.
 * Ownership: not transferred.
 * @param package       cr_Package
 * @return              string chunk or NULL
 */
GStringChunk *cr_package_get_chunk(cr_Package *package);

/** Set string chunk used to store all package strings in one place.
 * Ownership: chunk is transferred to the package.
 * @param package       cr_Package
 * @param chunk         string chunk
 */
void cr_package_set_chunk(cr_Package *package, GStringChunk *chunk);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_PACKAGE_H__ */
