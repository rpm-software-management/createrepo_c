/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
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

#include <string.h>
#include "package_internal.h"
#include "package.h"
#include "misc.h"

#define PACKAGE_CHUNK_SIZE 2048

cr_Dependency *
cr_dependency_new(void)
{
    cr_Dependency *dep;

    dep = g_new0 (cr_Dependency, 1);

    return dep;
}

cr_PackageFile *
cr_package_file_new(void)
{
    cr_PackageFile *file;

    file = g_new0 (cr_PackageFile, 1);

    return file;
}

cr_ChangelogEntry *
cr_changelog_entry_new(void)
{
    cr_ChangelogEntry *entry;

    entry = g_new0 (cr_ChangelogEntry, 1);

    return entry;
}

cr_BinaryData *
cr_binary_data_new(void)
{
    return (cr_BinaryData *) g_new0(cr_BinaryData, 1);
}

cr_Package *
cr_package_new(void)
{
    cr_Package *package;

    package = g_new0 (cr_Package, 1);
    package->chunk = g_string_chunk_new (PACKAGE_CHUNK_SIZE);

    return package;
}

cr_Package *
cr_package_new_without_chunk(void)
{
    return g_new0(cr_Package, 1);
}

void
cr_package_free(cr_Package *package)
{
    if (!package)
        return;

    if (package->chunk && !(package->loadingflags & CR_PACKAGE_SINGLE_CHUNK))
        g_string_chunk_free (package->chunk);

    if (package->requires) {
        g_slist_free_full(package->requires, g_free);
    }

    if (package->provides) {
        g_slist_free_full(package->provides, g_free);
    }

    if (package->conflicts) {
        g_slist_free_full(package->conflicts, g_free);
    }

    if (package->obsoletes) {
        g_slist_free_full(package->obsoletes, g_free);
    }

    if (package->suggests) {
        g_slist_free_full(package->suggests, g_free);
    }

    if (package->enhances) {
        g_slist_free_full(package->enhances, g_free);
    }

    if (package->recommends) {
        g_slist_free_full(package->recommends, g_free);
    }

    if (package->supplements) {
        g_slist_free_full(package->supplements, g_free);
    }

    if (package->files) {
        g_slist_free_full(package->files, g_free);
    }

    if (package->changelogs) {
        g_slist_free_full(package->changelogs, g_free);
    }

    g_free(package->siggpg);
    g_free(package->sigpgp);

    g_free (package);
}

gchar *
cr_package_nvra(cr_Package *package)
{
    return g_strdup_printf("%s-%s-%s.%s", package->name, package->version,
                                          package->release, package->arch);
}

gchar *
cr_package_nevra(cr_Package *package)
{
    char *epoch;
    if (package->epoch && strlen(package->epoch))
        epoch = package->epoch;
    else
        epoch = "0";

    return g_strdup_printf("%s-%s:%s-%s.%s", package->name, epoch,
                           package->version, package->release, package->arch);
}

static GSList *
cr_dependency_dup(GStringChunk *chunk, GSList *orig)
{
    GSList *list = NULL;

    for (GSList *elem = orig; elem; elem = g_slist_next(elem)) {
        cr_Dependency *odep = elem->data;
        cr_Dependency *ndep  = cr_dependency_new();
        ndep->name    = cr_safe_string_chunk_insert(chunk, odep->name);
        ndep->flags   = cr_safe_string_chunk_insert(chunk, odep->flags);
        ndep->epoch   = cr_safe_string_chunk_insert(chunk, odep->epoch);
        ndep->version = cr_safe_string_chunk_insert(chunk, odep->version);
        ndep->release = cr_safe_string_chunk_insert(chunk, odep->release);
        ndep->pre     = odep->pre;
        list = g_slist_prepend(list, ndep);
    }

    return g_slist_reverse(list);
}

cr_Package *
cr_package_copy(cr_Package *orig)
{
    cr_Package *pkg = cr_package_new();
    cr_package_copy_into(orig, pkg);
    return pkg;
}

void
cr_package_copy_into(cr_Package *orig, cr_Package *pkg)
{
    pkg->pkgKey           = orig->pkgKey;
    pkg->pkgId            = cr_safe_string_chunk_insert(pkg->chunk, orig->pkgId);
    pkg->name             = cr_safe_string_chunk_insert(pkg->chunk, orig->name);
    pkg->arch             = cr_safe_string_chunk_insert(pkg->chunk, orig->arch);
    pkg->version          = cr_safe_string_chunk_insert(pkg->chunk, orig->version);
    pkg->epoch            = cr_safe_string_chunk_insert(pkg->chunk, orig->epoch);
    pkg->release          = cr_safe_string_chunk_insert(pkg->chunk, orig->release);
    pkg->summary          = cr_safe_string_chunk_insert(pkg->chunk, orig->summary);
    pkg->description      = cr_safe_string_chunk_insert(pkg->chunk, orig->description);
    pkg->url              = cr_safe_string_chunk_insert(pkg->chunk, orig->url);
    pkg->time_file        = orig->time_file;
    pkg->time_build       = orig->time_build;
    pkg->rpm_license      = cr_safe_string_chunk_insert(pkg->chunk, orig->rpm_license);
    pkg->rpm_vendor       = cr_safe_string_chunk_insert(pkg->chunk, orig->rpm_vendor);
    pkg->rpm_group        = cr_safe_string_chunk_insert(pkg->chunk, orig->rpm_group);
    pkg->rpm_buildhost    = cr_safe_string_chunk_insert(pkg->chunk, orig->rpm_buildhost);
    pkg->rpm_sourcerpm    = cr_safe_string_chunk_insert(pkg->chunk, orig->rpm_sourcerpm);
    pkg->rpm_header_start = orig->rpm_header_start;
    pkg->rpm_header_end   = orig->rpm_header_end;
    pkg->rpm_packager     = cr_safe_string_chunk_insert(pkg->chunk, orig->rpm_packager);
    pkg->size_package     = orig->size_package;
    pkg->size_installed   = orig->size_installed;
    pkg->size_archive     = orig->size_archive;
    pkg->location_href    = cr_safe_string_chunk_insert(pkg->chunk, orig->location_href);
    pkg->location_base    = cr_safe_string_chunk_insert(pkg->chunk, orig->location_base);
    pkg->checksum_type    = cr_safe_string_chunk_insert(pkg->chunk, orig->checksum_type);
    pkg->files_checksum_type = cr_safe_string_chunk_insert(pkg->chunk, orig->files_checksum_type);

    pkg->requires    = cr_dependency_dup(pkg->chunk, orig->requires);
    pkg->provides    = cr_dependency_dup(pkg->chunk, orig->provides);
    pkg->conflicts   = cr_dependency_dup(pkg->chunk, orig->conflicts);
    pkg->obsoletes   = cr_dependency_dup(pkg->chunk, orig->obsoletes);
    pkg->suggests    = cr_dependency_dup(pkg->chunk, orig->suggests);
    pkg->enhances    = cr_dependency_dup(pkg->chunk, orig->enhances);
    pkg->recommends  = cr_dependency_dup(pkg->chunk, orig->recommends);
    pkg->supplements = cr_dependency_dup(pkg->chunk, orig->supplements);

    for (GSList *elem = orig->files; elem; elem = g_slist_next(elem)) {
        cr_PackageFile *orig_file = elem->data;
        cr_PackageFile *file = cr_package_file_new();
        file->type   = cr_safe_string_chunk_insert(pkg->chunk, orig_file->type);
        file->path   = cr_safe_string_chunk_insert(pkg->chunk, orig_file->path);
        file->name   = cr_safe_string_chunk_insert(pkg->chunk, orig_file->name);
        file->digest = cr_safe_string_chunk_insert(pkg->chunk, orig_file->digest);
        pkg->files = g_slist_prepend(pkg->files, file);
    }

    for (GSList *elem = orig->changelogs; elem; elem = g_slist_next(elem)) {
        cr_ChangelogEntry *orig_log = elem->data;
        cr_ChangelogEntry *log = cr_changelog_entry_new();
        log->author    = cr_safe_string_chunk_insert(pkg->chunk, orig_log->author);
        log->date      = orig_log->date;
        log->changelog = cr_safe_string_chunk_insert(pkg->chunk, orig_log->changelog);
        pkg->changelogs = g_slist_prepend(pkg->changelogs, log);
    }
}

static void
cr_package_assign_string(cr_Package *pkg, char **field, const char *value)
{
    if (!pkg) {
        return;
    }

    if (pkg->chunk) {
        *field = cr_safe_string_chunk_insert(pkg->chunk, value);
        return;
    }

    if (*field != value) {
        g_free(*field);
        *field = value ? g_strdup(value) : NULL;
    }
}

const char *
cr_package_get_pkg_id(cr_Package *pkg)
{
    return pkg ? pkg->pkgId : NULL;
}

void
cr_package_set_pkg_id(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->pkgId, value);
}

const char *
cr_package_get_name(cr_Package *pkg)
{
    return pkg ? pkg->name : NULL;
}

void
cr_package_set_name(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->name, value);
}

const char *
cr_package_get_arch(cr_Package *pkg)
{
    return pkg ? pkg->arch : NULL;
}

void
cr_package_set_arch(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->arch, value);
}

const char *
cr_package_get_version(cr_Package *pkg)
{
    return pkg ? pkg->version : NULL;
}

void
cr_package_set_version(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->version, value);
}

const char *
cr_package_get_epoch(cr_Package *pkg)
{
    return pkg ? pkg->epoch : NULL;
}

void
cr_package_set_epoch(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->epoch, value);
}

const char *
cr_package_get_release(cr_Package *pkg)
{
    return pkg ? pkg->release : NULL;
}

void
cr_package_set_release(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->release, value);
}

const char *
cr_package_get_summary(cr_Package *pkg)
{
    return pkg ? pkg->summary : NULL;
}

void
cr_package_set_summary(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->summary, value);
}

const char *
cr_package_get_description(cr_Package *pkg)
{
    return pkg ? pkg->description : NULL;
}

void
cr_package_set_description(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->description, value);
}

const char *
cr_package_get_url(cr_Package *pkg)
{
    return pkg ? pkg->url : NULL;
}

void
cr_package_set_url(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->url, value);
}

const char *
cr_package_get_rpm_license(cr_Package *pkg)
{
    return pkg ? pkg->rpm_license : NULL;
}

void
cr_package_set_rpm_license(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->rpm_license, value);
}

const char *
cr_package_get_rpm_vendor(cr_Package *pkg)
{
    return pkg ? pkg->rpm_vendor : NULL;
}

void
cr_package_set_rpm_vendor(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->rpm_vendor, value);
}

const char *
cr_package_get_rpm_group(cr_Package *pkg)
{
    return pkg ? pkg->rpm_group : NULL;
}

void
cr_package_set_rpm_group(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->rpm_group, value);
}

const char *
cr_package_get_rpm_buildhost(cr_Package *pkg)
{
    return pkg ? pkg->rpm_buildhost : NULL;
}

void
cr_package_set_rpm_buildhost(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->rpm_buildhost, value);
}

const char *
cr_package_get_rpm_sourcerpm(cr_Package *pkg)
{
    return pkg ? pkg->rpm_sourcerpm : NULL;
}

void
cr_package_set_rpm_sourcerpm(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->rpm_sourcerpm, value);
}

const char *
cr_package_get_rpm_packager(cr_Package *pkg)
{
    return pkg ? pkg->rpm_packager : NULL;
}

void
cr_package_set_rpm_packager(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->rpm_packager, value);
}

const char *
cr_package_get_location_href(cr_Package *pkg)
{
    return pkg ? pkg->location_href : NULL;
}

void
cr_package_set_location_href(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->location_href, value);
}

const char *
cr_package_get_location_base(cr_Package *pkg)
{
    return pkg ? pkg->location_base : NULL;
}

void
cr_package_set_location_base(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->location_base, value);
}

const char *
cr_package_get_checksum_type(cr_Package *pkg)
{
    return pkg ? pkg->checksum_type : NULL;
}

void
cr_package_set_checksum_type(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->checksum_type, value);
}

const char *
cr_package_get_files_checksum_type(cr_Package *pkg)
{
    return pkg ? pkg->files_checksum_type : NULL;
}

void
cr_package_set_files_checksum_type(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->files_checksum_type, value);
}

const char *
cr_package_get_hdrid(cr_Package *pkg)
{
    return pkg ? pkg->hdrid : NULL;
}

void
cr_package_set_hdrid(cr_Package *pkg, const char *value)
{
    cr_package_assign_string(pkg, &pkg->hdrid, value);
}

gint64
cr_package_get_pkg_key(cr_Package *pkg)
{
    return pkg ? pkg->pkgKey : 0;
}

void
cr_package_set_pkg_key(cr_Package *pkg, gint64 value)
{
    if (pkg) {
        pkg->pkgKey = value;
    }
}

gint64
cr_package_get_time_file(cr_Package *pkg)
{
    return pkg ? pkg->time_file : 0;
}

void
cr_package_set_time_file(cr_Package *pkg, gint64 value)
{
    if (pkg) {
        pkg->time_file = value;
    }
}

gint64
cr_package_get_time_build(cr_Package *pkg)
{
    return pkg ? pkg->time_build : 0;
}

void
cr_package_set_time_build(cr_Package *pkg, gint64 value)
{
    if (pkg) {
        pkg->time_build = value;
    }
}

gint64
cr_package_get_rpm_header_start(cr_Package *pkg)
{
    return pkg ? pkg->rpm_header_start : 0;
}

void
cr_package_set_rpm_header_start(cr_Package *pkg, gint64 value)
{
    if (pkg) {
        pkg->rpm_header_start = value;
    }
}

gint64
cr_package_get_rpm_header_end(cr_Package *pkg)
{
    return pkg ? pkg->rpm_header_end : 0;
}

void
cr_package_set_rpm_header_end(cr_Package *pkg, gint64 value)
{
    if (pkg) {
        pkg->rpm_header_end = value;
    }
}

gint64
cr_package_get_size_package(cr_Package *pkg)
{
    return pkg ? pkg->size_package : 0;
}

void
cr_package_set_size_package(cr_Package *pkg, gint64 value)
{
    if (pkg) {
        pkg->size_package = value;
    }
}

gint64
cr_package_get_size_installed(cr_Package *pkg)
{
    return pkg ? pkg->size_installed : 0;
}

void
cr_package_set_size_installed(cr_Package *pkg, gint64 value)
{
    if (pkg) {
        pkg->size_installed = value;
    }
}

gint64
cr_package_get_size_archive(cr_Package *pkg)
{
    return pkg ? pkg->size_archive : 0;
}

void
cr_package_set_size_archive(cr_Package *pkg, gint64 value)
{
    if (pkg) {
        pkg->size_archive = value;
    }
}

GSList *
cr_package_get_requires(cr_Package *pkg)
{
    return pkg ? pkg->requires : NULL;
}

void
cr_package_set_requires(cr_Package *pkg, GSList *value)
{
    if (pkg) {
        pkg->requires = value;
    }
}

GSList *
cr_package_get_provides(cr_Package *pkg)
{
    return pkg ? pkg->provides : NULL;
}

void
cr_package_set_provides(cr_Package *pkg, GSList *value)
{
    if (pkg) {
        pkg->provides = value;
    }
}

GSList *
cr_package_get_conflicts(cr_Package *pkg)
{
    return pkg ? pkg->conflicts : NULL;
}

void
cr_package_set_conflicts(cr_Package *pkg, GSList *value)
{
    if (pkg) {
        pkg->conflicts = value;
    }
}

GSList *
cr_package_get_obsoletes(cr_Package *pkg)
{
    return pkg ? pkg->obsoletes : NULL;
}

void
cr_package_set_obsoletes(cr_Package *pkg, GSList *value)
{
    if (pkg) {
        pkg->obsoletes = value;
    }
}

GSList *
cr_package_get_suggests(cr_Package *pkg)
{
    return pkg ? pkg->suggests : NULL;
}

void
cr_package_set_suggests(cr_Package *pkg, GSList *value)
{
    if (pkg) {
        pkg->suggests = value;
    }
}

GSList *
cr_package_get_enhances(cr_Package *pkg)
{
    return pkg ? pkg->enhances : NULL;
}

void
cr_package_set_enhances(cr_Package *pkg, GSList *value)
{
    if (pkg) {
        pkg->enhances = value;
    }
}

GSList *
cr_package_get_recommends(cr_Package *pkg)
{
    return pkg ? pkg->recommends : NULL;
}

void
cr_package_set_recommends(cr_Package *pkg, GSList *value)
{
    if (pkg) {
        pkg->recommends = value;
    }
}

GSList *
cr_package_get_supplements(cr_Package *pkg)
{
    return pkg ? pkg->supplements : NULL;
}

void
cr_package_set_supplements(cr_Package *pkg, GSList *value)
{
    if (pkg) {
        pkg->supplements = value;
    }
}

GSList *
cr_package_get_files(cr_Package *pkg)
{
    return pkg ? pkg->files : NULL;
}

void
cr_package_set_files(cr_Package *pkg, GSList *value)
{
    if (pkg) {
        pkg->files = value;
    }
}

GSList *
cr_package_get_changelogs(cr_Package *pkg)
{
    return pkg ? pkg->changelogs : NULL;
}

void
cr_package_set_changelogs(cr_Package *pkg, GSList *value)
{
    if (pkg) {
        pkg->changelogs = value;
    }
}

cr_PackageLoadingFlags
cr_package_get_loading_flags(cr_Package *pkg)
{
    return pkg ? pkg->loadingflags : 0;
}

void
cr_package_set_loading_flags(cr_Package *pkg, cr_PackageLoadingFlags flags)
{
    if (pkg) {
        pkg->loadingflags = flags;
    }
}

gboolean
cr_package_get_skip_dump(cr_Package *pkg)
{
    return pkg ? pkg->skip_dump : FALSE;
}

void
cr_package_set_skip_dump(cr_Package *pkg, gboolean skip_dump)
{
    if (pkg) {
        pkg->skip_dump = skip_dump;
    }
}

cr_BinaryData *
cr_package_get_siggpg(cr_Package *pkg)
{
    return pkg ? pkg->siggpg : NULL;
}

void
cr_package_set_siggpg(cr_Package *pkg, cr_BinaryData *data)
{
    if (pkg) {
        pkg->siggpg = data;
    }
}

cr_BinaryData *
cr_package_get_sigpgp(cr_Package *pkg)
{
    return pkg ? pkg->sigpgp : NULL;
}

void
cr_package_set_sigpgp(cr_Package *pkg, cr_BinaryData *data)
{
    if (pkg) {
        pkg->sigpgp = data;
    }
}

GStringChunk *
cr_package_get_chunk(cr_Package *pkg)
{
    return pkg ? pkg->chunk : NULL;
}

void
cr_package_set_chunk(cr_Package *pkg, GStringChunk *chunk)
{
    if (pkg) {
        pkg->chunk = chunk;
    }
}
