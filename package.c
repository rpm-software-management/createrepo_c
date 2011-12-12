/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "package.h"

#define PACKAGE_CHUNK_SIZE 2048

Dependency *
dependency_new (void)
{
    Dependency *dep;

    dep = g_new0 (Dependency, 1);

    return dep;
}

PackageFile *
package_file_new (void)
{
    PackageFile *file;

    file = g_new0 (PackageFile, 1);

    return file;
}

ChangelogEntry *
changelog_entry_new (void)
{
    ChangelogEntry *entry;

    entry = g_new0 (ChangelogEntry, 1);

    return entry;
}

Package *
package_new (void)
{
    Package *package;

    package = g_new0 (Package, 1);
    package->chunk = g_string_chunk_new (PACKAGE_CHUNK_SIZE);

    return package;
}

void
package_free (Package *package)
{
    g_string_chunk_free (package->chunk);

    if (package->requires) {
        g_slist_foreach (package->requires, (GFunc) g_free, NULL);
        g_slist_free (package->requires);
    }

    if (package->provides) {
        g_slist_foreach (package->provides, (GFunc) g_free, NULL);
        g_slist_free (package->provides);
    }

    if (package->conflicts) {
        g_slist_foreach (package->conflicts, (GFunc) g_free, NULL);
        g_slist_free (package->conflicts);
    }

    if (package->obsoletes) {
        g_slist_foreach (package->obsoletes, (GFunc) g_free, NULL);
        g_slist_free (package->obsoletes);
    }

    if (package->files) {
        g_slist_foreach (package->files, (GFunc) g_free, NULL);
        g_slist_free (package->files);
    }

    if (package->changelogs) {
        g_slist_foreach (package->changelogs, (GFunc) g_free, NULL);
        g_slist_free (package->changelogs);
    }

    g_free (package);
}
