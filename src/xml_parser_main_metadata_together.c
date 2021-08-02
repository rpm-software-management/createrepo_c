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

#include <glib.h>
#include <glib/gprintf.h>
#include <assert.h>
#include <errno.h>
#include "error.h"
#include "xml_parser.h"
#include "xml_parser_internal.h"
#include "package_internal.h"
#include "misc.h"

#define ERR_DOMAIN      CREATEREPO_C_ERROR

typedef struct {
    GHashTable           *in_progress_pkgs_hash; //used only when allowing out of order pkgs
    GSList               *in_progress_pkgs_list; // used only when not allowing out of order pkgs
    int                  in_progress_count_primary;
    int                  in_progress_count_filelists;
    int                  in_progress_count_other;
    cr_XmlParserNewPkgCb newpkgcb;      // newpkgcb passed in from user
    void                 *newpkgcb_data;// newpkgcb data passed in from user
    cr_XmlParserPkgCb    pkgcb;         // pkgcb passed in from user
    void                 *pkgcb_data;   // pkgcb data passed in from user
} cr_CbData;

static int
call_user_callback_if_package_finished(cr_Package *pkg, cr_CbData *cb_data, GError **err)
{
    if (pkg && (pkg->loadingflags & CR_PACKAGE_LOADED_PRI) && (pkg->loadingflags & CR_PACKAGE_LOADED_OTH) &&
        (pkg->loadingflags & CR_PACKAGE_LOADED_FIL))
    {
        if (cb_data->in_progress_pkgs_hash) {
            g_hash_table_remove(cb_data->in_progress_pkgs_hash, pkg->pkgId);
        } else {
            //remove first element in the list
            cb_data->in_progress_pkgs_list = cb_data->in_progress_pkgs_list->next;
        }

        // One package was fully finished
        cb_data->in_progress_count_primary--;
        cb_data->in_progress_count_filelists--;
        cb_data->in_progress_count_other--;

        // call user package callback
        GError *tmp_err = NULL;
        if (cb_data->pkgcb && cb_data->pkgcb(pkg, cb_data->pkgcb_data, &tmp_err)) {
            if (tmp_err)
                g_propagate_prefixed_error(err, tmp_err, "Parsing interrupted: ");
            else
                g_set_error(err, ERR_DOMAIN, CRE_CBINTERRUPTED, "Parsing interrupted");
            return CR_CB_RET_ERR;
        } else {
            // If callback return CRE_OK but it simultaneously set
            // the tmp_err then it's a programming error.
            assert(tmp_err == NULL);
        };
    }
    return CR_CB_RET_OK;
}

static cr_Package*
find_in_progress_pkg(cr_CbData *cb_data, const char *pkgId, int in_progress_pkg_index, GError **err)
{
    gpointer pval = NULL;
    if (cb_data->in_progress_pkgs_hash) {
        if (!g_hash_table_lookup_extended(cb_data->in_progress_pkgs_hash, pkgId, NULL, &pval)) {
            pval = NULL;
        }
    } else {
        // This is checking out of order pkgs because if we don't have in_progress_pkgs_hash -> we enforce
        // order by using a list
        pval = g_slist_nth_data(cb_data->in_progress_pkgs_list, in_progress_pkg_index);
        if (pval && g_strcmp0(((cr_Package *) pval)->pkgId, pkgId)) {
            g_set_error(err, ERR_DOMAIN, CRE_XMLPARSER,
                        "Out of order metadata: %s vs %s.", ((cr_Package *) pval)->pkgId, pkgId);
            pval = NULL;
        }
    }

    return pval;
}

static void
store_in_progress_pkg(cr_CbData *cb_data, cr_Package *pkg, const char *pkgId)
{
    if (!pkg) {
        return;
    }
    if (cb_data->in_progress_pkgs_hash) {
        g_hash_table_insert(cb_data->in_progress_pkgs_hash, g_strdup(pkgId), pkg);
    } else {
        cb_data->in_progress_pkgs_list = g_slist_append(cb_data->in_progress_pkgs_list, pkg);
    }
}

static int
newpkgcb_primary(cr_Package **pkg,
                    G_GNUC_UNUSED const char *pkgId,
                    G_GNUC_UNUSED const char *name,
                    G_GNUC_UNUSED const char *arch,
                    G_GNUC_UNUSED void *cbdata,
                    GError **err)
{
    assert(pkg && *pkg == NULL);
    assert(!err || *err == NULL);

    // This callback is called when parsing of the opening element of a package
    // is done. However because the opening element doesn't contain pkgId
    // (instead it looks like: <package type="rpm">) we cannot check if we
    // already have this package.
    // The only option is to create a new package and after its fully
    // parsed (in pkgcb_primary) either use this package or copy its data
    // into an already existing one.
    // Filelists and other have pkgId present in the opening element so we can
    // avoid this overhead.
    *pkg = cr_package_new();

    return CR_CB_RET_OK;
}

static int
newpkg_general(cr_Package **pkg,
                 const char *pkgId,
                 const char *name,
                 const char *arch,
                 void *cbdata,
                 int in_progress_count,
                 GError **err)
{
    assert(pkg && *pkg == NULL);
    assert(!err || *err == NULL);

    cr_CbData *cb_data = cbdata;

    GError *out_of_order_err = NULL;
    *pkg = find_in_progress_pkg(cb_data, pkgId, in_progress_count, &out_of_order_err);

    if (!*pkg) {
        // we are handling never before seen package

        if (cb_data->newpkgcb) {
            // user specified their own new function: call it
            if (cb_data->newpkgcb(pkg, pkgId, name, arch, cb_data->newpkgcb_data, err)) {
                return CR_CB_RET_ERR;
            }
            if (!*pkg) {
                // when the user callback doesn't return a pkg we should skip it,
                // this means out of order error doesn't apply
                g_clear_error(&out_of_order_err);
            }
        } else {
            *pkg = cr_package_new();
        }

        store_in_progress_pkg(cb_data, *pkg, pkgId);
    }

    if (*err) {
        return CR_CB_RET_ERR;
    }

    if (out_of_order_err) {
        g_propagate_error(err, out_of_order_err);
        return CR_CB_RET_ERR;
    }

    return CR_CB_RET_OK;
}

static int
newpkgcb_filelists(cr_Package **pkg,
                   const char *pkgId,
                   G_GNUC_UNUSED const char *name,
                   G_GNUC_UNUSED const char *arch,
                   void *cbdata,
                   GError **err)
{
    cr_CbData *cb_data = cbdata;
    return newpkg_general(pkg, pkgId, name, arch, cbdata, cb_data->in_progress_count_filelists, err);
}

static int
newpkgcb_other(cr_Package **pkg,
               const char *pkgId,
               G_GNUC_UNUSED const char *name,
               G_GNUC_UNUSED const char *arch,
               void *cbdata,
               GError **err)
{
    cr_CbData *cb_data = cbdata;
    return newpkg_general(pkg, pkgId, name, arch, cbdata, cb_data->in_progress_count_other, err);
}

static int
pkgcb_filelists(cr_Package *pkg, void *cbdata, G_GNUC_UNUSED GError **err)
{
    cr_CbData *cb_data = cbdata;
    cb_data->in_progress_count_filelists++;
    pkg->loadingflags |= CR_PACKAGE_LOADED_FIL;
    return call_user_callback_if_package_finished(pkg, cb_data, err);
}

static int
pkgcb_other(cr_Package *pkg, void *cbdata, G_GNUC_UNUSED GError **err)
{
    cr_CbData *cb_data = cbdata;
    cb_data->in_progress_count_other++;
    pkg->loadingflags |= CR_PACKAGE_LOADED_OTH;
    return call_user_callback_if_package_finished(pkg, cb_data, err);
}

static int
pkgcb_primary(cr_Package *pkg, void *cbdata, G_GNUC_UNUSED GError **err)
{
    cr_CbData *cb_data = cbdata;

    GError *out_of_order_err = NULL;
    cr_Package *in_progress_pkg = find_in_progress_pkg(cb_data, pkg->pkgId, cb_data->in_progress_count_primary,
                                                       &out_of_order_err);
    if (in_progress_pkg) {
        // package was already encountered in some other metadata type

        cr_package_copy_into(pkg, in_progress_pkg);
        cr_package_free(pkg);
        pkg = in_progress_pkg;
    } else {
        // we are handling never before seen package

        if (cb_data->newpkgcb) {
            // user specified their own new function: call it and copy package data into user_created_pkg
            cr_Package *user_created_pkg = NULL;
            if (cb_data->newpkgcb(&user_created_pkg, pkg->pkgId, pkg->name, pkg->arch, cb_data->newpkgcb_data, err)) {
                return CR_CB_RET_ERR;
            } else {
                if (user_created_pkg) {
                    cr_package_copy_into(pkg, user_created_pkg);
                }
                // user_created_pkg can be NULL if newpkgcb returns OK but
                // not an allocated pkg -> this means we should skip it
                store_in_progress_pkg(cb_data, user_created_pkg, pkg->pkgId);
                cr_package_free(pkg);
                pkg = user_created_pkg;
            }
            if (!pkg) {
                // when the user callback doesn't return a pkg we should skip it,
                // this means out of order error doesn't apply
                g_clear_error(&out_of_order_err);
            }
        } else {
            store_in_progress_pkg(cb_data, pkg, pkg->pkgId);
        }

    }

    if (*err) {
        return CR_CB_RET_ERR;
    }

    if (out_of_order_err) {
        g_propagate_error(err, out_of_order_err);
        return CR_CB_RET_ERR;
    }


    if (pkg) {
        cb_data->in_progress_count_primary++;
        pkg->loadingflags |= CR_PACKAGE_LOADED_PRI;
        pkg->loadingflags |= CR_PACKAGE_FROM_XML;
    }

    return call_user_callback_if_package_finished(pkg, cb_data, err);
}

static gboolean
parse_next_section(CR_FILE *target_file, const char *path, cr_ParserData *pd, GError **err)
{
    char buf[XML_BUFFER_SIZE];
    GError *tmp_err = NULL;
    int parsed_len = cr_read(target_file, buf, XML_BUFFER_SIZE, &tmp_err);
    if (tmp_err) {
        g_critical("%s: Error while reading xml '%s': %s", __func__, path, tmp_err->message);
        g_propagate_prefixed_error(err, tmp_err, "Read error: ");
        return FALSE;
    }
    int done = parsed_len == 0;
    if (xmlParseChunk(pd->parser, buf, parsed_len, done)) {
        xmlErrorPtr xml_err = xmlCtxtGetLastError(pd->parser);
        g_critical("%s: parsing error '%s': %s", __func__, path,
                   (xml_err) ? xml_err->message : "UNKNOWN_ERROR");
        g_set_error(err, ERR_DOMAIN, CRE_XMLPARSER,
                    "Parse error '%s' at line: %d (%s)",
                    path,
                    (xml_err) ? (int) xml_err->line : 0,
                    (xml_err) ? (char *) xml_err->message : "UNKNOWN_ERROR");
        return FALSE;
    }

    if (pd->err) {
        g_propagate_error(err, pd->err);
        return FALSE;
    }

    return done;
}

//TODO(amatej): there is quite some overlap with this and cr_load_xml_files,
//              we could use this api and just wrap it in cr_loax_xml_files?
int cr_xml_parse_main_metadata_together(const char *primary_path,
                                        const char *filelists_path,
                                        const char *other_path,
                                        cr_XmlParserNewPkgCb newpkgcb,
                                        void *newpkgcb_data,
                                        cr_XmlParserPkgCb pkgcb,
                                        void *pkgcb_data,
                                        cr_XmlParserWarningCb warningcb,
                                        void *warningcb_data,
                                        gboolean allow_out_of_order,
                                        GError **err)
{
    int ret = CRE_OK;
    CR_FILE *primary_f = NULL;
    CR_FILE *filelists_f = NULL;
    CR_FILE *other_f = NULL;
    GError *tmp_err = NULL;

    cr_CbData cbdata;
    cbdata.in_progress_pkgs_list = NULL;
    cbdata.in_progress_pkgs_hash = NULL;
    cbdata.newpkgcb = newpkgcb;
    cbdata.newpkgcb_data = newpkgcb_data;
    cbdata.pkgcb = pkgcb;
    cbdata.pkgcb_data = pkgcb_data;

    if (allow_out_of_order) {
        cbdata.in_progress_pkgs_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    }

    assert(primary_path);
    assert(filelists_path);
    assert(other_path);
    assert(newpkgcb || pkgcb);
    assert(!err || *err == NULL);

    cr_ParserData *primary_pd = NULL;
    cr_ParserData *filelists_pd = NULL;
    cr_ParserData *other_pd = NULL;

    primary_f = cr_open(primary_path, CR_CW_MODE_READ, CR_CW_AUTO_DETECT_COMPRESSION, &tmp_err);
    if (tmp_err) {
        ret = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Cannot open %s: ", primary_path);
        g_clear_error(&tmp_err);
        goto out;
    }
    filelists_f = cr_open(filelists_path, CR_CW_MODE_READ, CR_CW_AUTO_DETECT_COMPRESSION, &tmp_err);
    if (tmp_err) {
        ret = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Cannot open %s: ", filelists_path);
        g_clear_error(&tmp_err);
        goto out;
    }
    other_f = cr_open(other_path, CR_CW_MODE_READ, CR_CW_AUTO_DETECT_COMPRESSION, &tmp_err);
    if (tmp_err) {
        ret = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Cannot open %s: ", other_path);
        g_clear_error(&tmp_err);
        goto out;
    }

    //TODO(amatej): In the future we could make filelists/other optional if there is a need for it. That would mean we
    //              should replace the last 0 in primary_parser_data_new depending on whether we have filelists or not.
    primary_pd = primary_parser_data_new(newpkgcb_primary, &cbdata, pkgcb_primary, &cbdata, warningcb, warningcb_data, 0);
    filelists_pd = filelists_parser_data_new(newpkgcb_filelists, &cbdata, pkgcb_filelists, &cbdata, warningcb, warningcb_data);
    other_pd = other_parser_data_new(newpkgcb_other, &cbdata, pkgcb_other, &cbdata, warningcb, warningcb_data);

    gboolean primary_is_done = 0;
    gboolean filelists_is_done = 0;
    gboolean other_is_done = 0;
    cbdata.in_progress_count_primary = 0;
    cbdata.in_progress_count_filelists = 0;
    cbdata.in_progress_count_other = 0;
    while (!primary_is_done || !filelists_is_done || !other_is_done) {
        while ((cbdata.in_progress_count_primary <= cbdata.in_progress_count_filelists ||
                cbdata.in_progress_count_primary <= cbdata.in_progress_count_other) &&
               !primary_is_done)
        {
            primary_is_done = parse_next_section(primary_f, primary_path, primary_pd, err);
            if (*err) {
                ret = (*err)->code;
                goto out;
            }

        }

        while ((cbdata.in_progress_count_filelists <= cbdata.in_progress_count_primary ||
                cbdata.in_progress_count_filelists <= cbdata.in_progress_count_other) &&
               !filelists_is_done)
        {
            filelists_is_done = parse_next_section(filelists_f, filelists_path, filelists_pd, err);
            if (*err) {
                ret = (*err)->code;
                goto out;
            }
        }

        while ((cbdata.in_progress_count_other <= cbdata.in_progress_count_filelists ||
                cbdata.in_progress_count_other <= cbdata.in_progress_count_primary) &&
               !other_is_done)
        {
            other_is_done = parse_next_section(other_f, other_path, other_pd, err);
            if (*err) {
                ret = (*err)->code;
                goto out;
            }
        }
    }

out:
    if (ret != CRE_OK) {
        // An error already encountered
        // just close the file without error checking
        cr_close(primary_f, NULL);
        cr_close(filelists_f, NULL);
        cr_close(other_f, NULL);
    } else {
        // No error encountered yet
        cr_close(primary_f, &tmp_err);
        if (!tmp_err)
            cr_close(filelists_f, &tmp_err);
        if (!tmp_err)
            cr_close(other_f, &tmp_err);
        if (tmp_err) {
            ret = tmp_err->code;
            g_propagate_prefixed_error(err, tmp_err, "Error while closing: ");
        }
    }

    cr_xml_parser_data_free(primary_pd);
    cr_xml_parser_data_free(filelists_pd);
    cr_xml_parser_data_free(other_pd);

    if (allow_out_of_order) {
        g_hash_table_destroy(cbdata.in_progress_pkgs_hash);
    } else {
        cr_slist_free_full(cbdata.in_progress_pkgs_list, (GDestroyNotify) cr_package_free);
    }

    return ret;
}
