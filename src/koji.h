/*
 * Copyright (C) 2018 Red Hat, Inc.
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

#ifndef __C_CREATEREPOLIB_KOJI_H__
#define __C_CREATEREPOLIB_KOJI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "package.h"
#include "mergerepo_c.h"

// struct KojiMergedReposStuff
// contains information needed to simulate sort_and_filter() method from
// mergerepos script from Koji.
//
// sort_and_filter() method description:
// ------------------------------------
// For each package object, check if the srpm name has ever been seen before.
// If is has not, keep the package.  If it has, check if the srpm name was first
// seen in the same repo as the current package.  If so, keep the package from
// the srpm with the highest NVR.  If not, keep the packages from the first
// srpm we found, and delete packages from all other srpms.
//
// Packages with matching NVRs in multiple repos will be taken from the first
// repo.
//
// If the srpm name appears in the blocked package list, any packages generated
// from the srpm will be deleted from the package sack as well.
//
// This method will also generate a file called "pkgorigins" and add it to the
// repo metadata. This is a tab-separated map of package E:N-V-R.A to repo URL
// (as specified on the command-line). This allows a package to be tracked back
// to its origin, even if the location field in the repodata does not match the
// original repo location.

struct srpm_val {
    int repo_id;        // id of repository
    char *sourcerpm;    // pkg->rpm_sourcerpm
};

struct KojiMergedReposStuff {
    GHashTable *blocked_srpms;
    // blocked_srpms:
    // Names of sprms which will be skipped
    //   Key: srpm name
    //   Value: NULL (not important)
    GHashTable *include_srpms;
    // include_srpms:
    // Only packages from srpms included in this table will be included
    // in output merged metadata.
    //   Key: srpm name
    //   Value: struct srpm_val
    GHashTable *seen_rpms;
    // seen_rpms:
    // List of packages already included into the output metadata.
    // Purpose of this list is to avoid a duplicit packages in output.
    //   Key: string with package n-v-r.a
    //   Value: NULL (not important)
    CR_FILE *pkgorigins;
    // Every element has format: pkg_nvra\trepourl
    gboolean simple;
};

/* Limited version of koji_stuff_prepare() that sets up only pkgorigins */
int
pkgorigins_prepare(struct KojiMergedReposStuff **koji_stuff_ptr,
                   const gchar *tmpdir);

int
koji_stuff_prepare(struct KojiMergedReposStuff **koji_stuff_ptr,
                   struct CmdOptions *cmd_options,
                   GSList *repos);

void
koji_stuff_destroy(struct KojiMergedReposStuff **koji_stuff_ptr);

void
cr_srpm_val_destroy(gpointer data);

gboolean
koji_allowed(cr_Package *pkg, struct KojiMergedReposStuff *koji_stuff);

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_KOJI_H__ */
