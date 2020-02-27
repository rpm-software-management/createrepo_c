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

#include "koji.h"
#include "error.h"
#include "load_metadata.h"
#include "misc.h"

void
cr_srpm_val_destroy(gpointer data)
{
    struct srpm_val *val = data;
    g_free(val->sourcerpm);
    g_free(val);
}


void
koji_stuff_destroy(struct KojiMergedReposStuff **koji_stuff_ptr)
{
    struct KojiMergedReposStuff *koji_stuff;

    if (!koji_stuff_ptr || !*koji_stuff_ptr)
        return;

    koji_stuff = *koji_stuff_ptr;

    g_clear_pointer (&koji_stuff->blocked_srpms, g_hash_table_destroy);
    g_clear_pointer (&koji_stuff->include_srpms, g_hash_table_destroy);
    g_clear_pointer (&koji_stuff->seen_rpms, g_hash_table_destroy);

    cr_close(koji_stuff->pkgorigins, NULL);
    g_free(koji_stuff);
}


static int
pkgorigins_prepare_file (const gchar *tmpdir, CR_FILE **pkgorigins_file)
{
    gchar *pkgorigins_path = NULL;
    GError *tmp_err = NULL;

    pkgorigins_path = g_strconcat(tmpdir, "pkgorigins.gz", NULL);
    *pkgorigins_file = cr_open(pkgorigins_path,
                                     CR_CW_MODE_WRITE,
                                     CR_CW_GZ_COMPRESSION,
                                     &tmp_err);
    if (tmp_err) {
        g_critical("Cannot open %s: %s", pkgorigins_path, tmp_err->message);
        g_error_free(tmp_err);
        g_free(pkgorigins_path);
        return 1;
    }
    g_free(pkgorigins_path);

    return 0;
}


/* Limited version of koji_stuff_prepare() that sets up only pkgorigins */
int
pkgorigins_prepare(struct KojiMergedReposStuff **koji_stuff_ptr,
                   const gchar *tmpdir)
{
   int result;
    struct KojiMergedReposStuff *koji_stuff =
        g_malloc0(sizeof(struct KojiMergedReposStuff));

    // Prepare pkgorigin file
    result = pkgorigins_prepare_file (tmpdir, &koji_stuff->pkgorigins);
    if (result != 0) {
        g_free (koji_stuff);
        return result;
    }

    *koji_stuff_ptr = koji_stuff;
    return 0;  // All ok
}


int
koji_stuff_prepare(struct KojiMergedReposStuff **koji_stuff_ptr,
                   struct CmdOptions *cmd_options,
                   GSList *repos)
{
    struct KojiMergedReposStuff *koji_stuff;
    GSList *element;
    int repoid;
    int result;

    // Pointers to elements in the koji_stuff_ptr
    GHashTable *include_srpms = NULL; // XXX

    koji_stuff = g_malloc0(sizeof(struct KojiMergedReposStuff));
    *koji_stuff_ptr = koji_stuff;


    // Prepare hashtables

    koji_stuff->include_srpms = g_hash_table_new_full(g_str_hash,
                                                      g_str_equal,
                                                      g_free,
                                                      cr_srpm_val_destroy);
    koji_stuff->seen_rpms = g_hash_table_new_full(g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  NULL);
    include_srpms = koji_stuff->include_srpms;

    // Load list of blocked srpm packages

    if (cmd_options->blocked) {
        int x = 0;
        char *content = NULL;
        char **names;
        GError *err = NULL;

        if (!g_file_get_contents(cmd_options->blocked, &content, NULL, &err)) {
            g_critical("Error while reading blocked file: %s", err->message);
            g_error_free(err);
            g_free(content);
            return 1;
        }

        koji_stuff->blocked_srpms = g_hash_table_new_full(g_str_hash,
                                                          g_str_equal,
                                                          g_free,
                                                          NULL);
        names = g_strsplit(content, "\n", 0);
        while (names && names[x] != NULL) {
            if (strlen(names[x]))
                g_hash_table_replace(koji_stuff->blocked_srpms,
                                     g_strdup(names[x]),
                                     NULL);
            x++;
        }

        g_strfreev(names);
        g_free(content);
    }


    koji_stuff->simple = cmd_options->koji_simple;

    // Prepare pkgorigin file
    result = pkgorigins_prepare_file (cmd_options->tmp_out_repo,
                                      &koji_stuff->pkgorigins);
    if (result != 0) {
        return result;
    }


    // Iterate over every repo and fill include_srpms hashtable

    g_debug("Preparing list of allowed srpm builds");

    repoid = 0;
    for (element = repos; element; element = g_slist_next(element)) {
        struct cr_MetadataLocation *ml;
        cr_Metadata *metadata;
        GHashTableIter iter;
        gpointer key, void_pkg;

        ml = (struct cr_MetadataLocation *) element->data;
        if (!ml) {
            g_critical("Bad repo location");
            repoid++;
            break;
        }

        metadata = cr_metadata_new(CR_HT_KEY_HASH, 0, NULL);

        g_debug("Loading srpms from: %s", ml->original_url);
        if (cr_metadata_load_xml(metadata, ml, NULL) != CRE_OK) {
            cr_metadata_free(metadata);
            g_critical("Cannot load repo: \"%s\"", ml->original_url);
            repoid++;
            break;
        }

        // Iterate over every package in repo and what "builds"
        // we're allowing into the repo
        g_hash_table_iter_init(&iter, cr_metadata_hashtable(metadata));
        while (g_hash_table_iter_next(&iter, &key, &void_pkg)) {
            cr_Package *pkg = (cr_Package *) void_pkg;
            cr_NEVRA *nevra;
            gpointer data;
            struct srpm_val *srpm_value_new;

            if (!pkg->rpm_sourcerpm) {
                g_warning("Package '%s' from '%s' doesn't have specified source srpm",
                          pkg->location_href, ml->original_url);
                continue;
            }

            nevra = cr_split_rpm_filename(pkg->rpm_sourcerpm);

            if (!nevra) {
                g_debug("Srpm name is invalid: %s", pkg->rpm_sourcerpm);
                continue;
            }

            data = g_hash_table_lookup(include_srpms, nevra->name);
            if (data) {
                // We have already seen build with the same name

                int cmp;
                cr_NEVRA *nevra_existing;
                struct srpm_val *srpm_value_existing = data;

                if (srpm_value_existing->repo_id != repoid) {
                    // We found a rpm built from an srpm with the same name in
                    // a previous repo. The previous repo takes precendence,
                    // so ignore the srpm found here.
                    cr_nevra_free(nevra);
                    g_debug("Srpm already loaded from previous repo %s",
                            pkg->rpm_sourcerpm);
                    continue;
                }

                // We're in the same repo, so compare srpm NVRs
                nevra_existing = cr_split_rpm_filename(srpm_value_existing->sourcerpm);
                cmp = cr_cmp_nevra(nevra, nevra_existing);
                cr_nevra_free(nevra_existing);
                if (cmp < 1) {
                    // Existing package is from the newer srpm
                    cr_nevra_free(nevra);
                    g_debug("Srpm already exists in newer version %s",
                            pkg->rpm_sourcerpm);
                    continue;
                }
            }

            // The current package we're processing is from a newer srpm
            // than the existing srpm in the dict, so update the dict
            // OR
            // We found a new build so we add it to the dict

            g_debug("Adding srpm: %s", pkg->rpm_sourcerpm);
            srpm_value_new = g_malloc0(sizeof(struct srpm_val));
            srpm_value_new->repo_id = repoid;
            srpm_value_new->sourcerpm = g_strdup(pkg->rpm_sourcerpm);
            g_hash_table_replace(include_srpms,
                                 g_strdup(nevra->name),
                                 srpm_value_new);
            cr_nevra_free(nevra);
        }

        cr_metadata_free(metadata);
        repoid++;
    }


    return 0;  // All ok
}

gboolean
koji_allowed(cr_Package *pkg, struct KojiMergedReposStuff *koji_stuff)
{
    gchar *nvra;
    gboolean seen;

    if (pkg->rpm_sourcerpm) {
        // Sometimes, there are metadata that don't contain sourcerpm
        // items for their packages.
        // I don't know if better is to include or exclude such packages.
        // Original mergerepos script doesn't expect such situation.
        // So for now, include them. But it can be changed anytime
        // in future.
        cr_NEVRA *nevra = cr_split_rpm_filename(pkg->rpm_sourcerpm);
        if (!nevra) {
            g_debug("Package %s has invalid srpm %s", pkg->name,
                    pkg->rpm_sourcerpm);
            return 0;
        }

        if (koji_stuff->blocked_srpms) {
            gboolean is_blocked;
            is_blocked = g_hash_table_lookup_extended(koji_stuff->blocked_srpms, nevra->name, NULL, NULL);
            if (is_blocked) {
                // Srpm of the package is not allowed
                g_debug("Package %s has blocked srpm %s", pkg->name,
                        pkg->rpm_sourcerpm);
                cr_nevra_free(nevra);
                return 0;
            }
        }

        if (!koji_stuff->simple && koji_stuff->include_srpms) {
            struct srpm_val *value;
            value = g_hash_table_lookup(koji_stuff->include_srpms, nevra->name);
            if (!value || g_strcmp0(pkg->rpm_sourcerpm, value->sourcerpm)) {
                // Srpm of the package is not allowed
                g_debug("Package %s has forbidden srpm %s", pkg->name,
                        pkg->rpm_sourcerpm);
                cr_nevra_free(nevra);
                return 0;
            }
        }
        cr_nevra_free(nevra);
    }

    if (!koji_stuff->simple && koji_stuff->seen_rpms) {
        // Check if we have already seen this package before
        nvra = cr_package_nvra(pkg);
        seen = g_hash_table_lookup_extended(koji_stuff->seen_rpms,
                nvra,
                NULL,
                NULL);
        if (seen) {
            // Similar package has been already added
            g_debug("Package with same nvra (%s) has been already added",
                    nvra);
            g_free(nvra);
            return 0;
        }
        // Make a note that we have seen this package
        g_hash_table_replace(koji_stuff->seen_rpms, nvra, NULL);
    }

    return 1;
}
