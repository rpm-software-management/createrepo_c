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
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include "error.h"
#include "package.h"
#include "logging.h"
#include "misc.h"
#include "load_metadata.h"
#include "locate_metadata.h"
#include "xml_parser.h"

/** TODO:
 * - Add support for single chunk (?)
 * - Support for warning cbs
 */

#define STRINGCHUNK_SIZE        16384

void
cr_free_values(gpointer data)
{
    cr_package_free((cr_Package *) data);
}

GHashTable *
cr_new_metadata_hashtable()
{
    GHashTable *hashtable = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  NULL, cr_free_values);
    return hashtable;
}

void
cr_destroy_metadata_hashtable(GHashTable *hashtable)
{
    if (hashtable)
        g_hash_table_destroy (hashtable);
}

cr_Metadata
cr_metadata_new(cr_HashTableKey key, int use_single_chunk, GSList *pkglist)
{
    cr_Metadata md;

    assert(key < CR_HT_KEY_SENTINEL);

    md = g_malloc0(sizeof(*md));
    md->key = key;
    md->ht = cr_new_metadata_hashtable();
    if (use_single_chunk)
        md->chunk = g_string_chunk_new(STRINGCHUNK_SIZE);

    if (pkglist) {
        // Create hashtable from pkglist
        // This hashtable is used for checking if the metadata of the package
        // should be included.
        // Purpose is to save memory - We load only metadata about
        // packages which we will probably use.
        // This hashtable is modified "on the fly" - If we found and load
        // a metadata about the package, we remove its record from the hashtable.
        // So if we met the metadata for this package again we will ignore it.
        md->pkglist_ht = g_hash_table_new_full(g_str_hash,
                                               g_str_equal,
                                               g_free,
                                               NULL);

       for (GSList *elem = pkglist; elem; elem = g_slist_next(elem))
            g_hash_table_insert(md->pkglist_ht, g_strdup(elem->data), NULL);
    }

    return md;
}

void
cr_metadata_free(cr_Metadata md)
{
    if (!md)
        return;

    cr_destroy_metadata_hashtable(md->ht);
    if (md->chunk)
        g_string_chunk_free(md->chunk);
    if (md->pkglist_ht)
        g_hash_table_destroy(md->pkglist_ht);
    g_free(md);
}

// Callbacks for XML parsers

typedef struct {
    GHashTable      *ht;
    GStringChunk    *chunk;
    GHashTable      *pkglist_ht;
} cr_CbData;

static int
pkgcb(cr_Package *pkg, void *cbdata, GError **err)
{
    gboolean store_pkg = TRUE;
    cr_CbData *cb_data = cbdata;

    CR_UNUSED(err);

    assert(pkg);
    assert(pkg->pkgId);

    if (cb_data->pkglist_ht) {
        char *basename = cr_get_filename(pkg->location_href);
        if (basename)
            store_pkg = g_hash_table_lookup_extended(cb_data->pkglist_ht,
                                                     basename,
                                                     NULL, NULL);
    }

    // TODO: Remove pointer to shared string chunk

    if (!store_pkg) {
        cr_package_free(pkg);
        return CR_CB_RET_OK;
    }

    // Store package into the hashtable
    g_hash_table_replace(cb_data->ht, pkg->pkgId, pkg);

    return CR_CB_RET_OK;
}

static int
newpkgcb(cr_Package **pkg,
         const char *pkgId,
         const char *name,
         const char *arch,
         void *cbdata,
         GError **err)
{
    cr_CbData *cb_data = cbdata;

    CR_UNUSED(name);
    CR_UNUSED(arch);
    CR_UNUSED(err);

    assert(*pkg == NULL);
    assert(pkgId);

    *pkg = g_hash_table_lookup(cb_data->ht, pkgId);

    return CR_CB_RET_OK;
}

static int
cr_load_xml_files(GHashTable *hashtable,
                  const char *primary_xml_path,
                  const char *filelists_xml_path,
                  const char *other_xml_path,
                  GStringChunk *chunk,
                  GHashTable *pkglist_ht,
                  GError **err)
{
    cr_CbData cb_data;
    GError *tmp_err = NULL;

    assert(hashtable);

    // Prepare cb data
    cb_data.ht          = hashtable;
    cb_data.chunk       = chunk;
    cb_data.pkglist_ht  = pkglist_ht;

    cr_xml_parse_primary(primary_xml_path,
                         NULL,
                         NULL,
                         pkgcb,
                         &cb_data,
                         NULL,
                         NULL,
                         (filelists_xml_path) ? 0 : 1,
                         &tmp_err);
    if (tmp_err) {
        int code = tmp_err->code;
        g_debug("primary.xml parsing error: %s", tmp_err->message);
        g_propagate_prefixed_error(err, tmp_err, "primary.xml parsing: ");
        return code;
    }

    if (filelists_xml_path) {
        cr_xml_parse_filelists(filelists_xml_path,
                               newpkgcb,
                               &cb_data,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               &tmp_err);
        if (tmp_err) {
            int code = tmp_err->code;
            g_debug("filelists.xml parsing error: %s", tmp_err->message);
            g_propagate_prefixed_error(err, tmp_err, "filelists.xml parsing: ");
            return code;
        }
    }

    if (other_xml_path) {
        cr_xml_parse_other(other_xml_path,
                           newpkgcb,
                           &cb_data,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           &tmp_err);
        if (tmp_err) {
            int code = tmp_err->code;
            g_debug("other.xml parsing error: %s", tmp_err->message);
            g_propagate_prefixed_error(err, tmp_err, "other.xml parsing: ");
            return code;
        }
    }

    return CRE_OK;
}

int
cr_metadata_load_xml(cr_Metadata md,
                     struct cr_MetadataLocation *ml,
                     GError **err)
{
    int result;
    GError *tmp_err = NULL;
    GHashTable *intern_hashtable;  // key is checksum (pkgId)

    assert(md);
    assert(ml);
    assert(!err || *err == NULL);

    if (!ml->pri_xml_href) {
        g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_BADARG,
                    "primary.xml file is missing");
        return CRE_BADARG;
    }

    // Load metadata
    intern_hashtable = cr_new_metadata_hashtable();
    result = cr_load_xml_files(intern_hashtable,
                               ml->pri_xml_href,
                               ml->fil_xml_href,
                               ml->oth_xml_href,
                               md->chunk,
                               md->pkglist_ht,
                               &tmp_err);

    if (result != CRE_OK) {
        g_critical("%s: Error encountered while parsing", __func__);
        g_propagate_prefixed_error(err, tmp_err,
                                   "Error encountered while parsing:");
        cr_destroy_metadata_hashtable(intern_hashtable);
        return result;
    }

    g_debug("%s: Parsed items: %d", __func__,
            g_hash_table_size(intern_hashtable));

    // Fill user hashtable and use user selected key

    GHashTableIter iter;
    gpointer p_key, p_value;

    g_hash_table_iter_init (&iter, intern_hashtable);
    while (g_hash_table_iter_next (&iter, &p_key, &p_value)) {
        cr_Package *pkg = (cr_Package *) p_value;
        gpointer new_key;

        switch (md->key) {
            case CR_HT_KEY_FILENAME:
                new_key = cr_get_filename(pkg->location_href);
                break;
            case CR_HT_KEY_HASH:
                new_key = pkg->pkgId;
                break;
            case CR_HT_KEY_NAME:
                new_key = pkg->name;
                break;
            default:
                // Well, this SHOULD never happend!
                // (md->key SHOULD be setted only by cr_metadata_new()
                // and it SHOULD set only valid key values)
                g_critical("%s: Unknown hash table key selected", __func__);
                assert(0);
                return CRE_ERROR;
        }

        if (g_hash_table_lookup(md->ht, new_key)) {
            g_debug("%s: Key \"%s\" already exists in hashtable",
                    __func__, (char *) new_key);
            g_hash_table_iter_remove(&iter);
        } else {
            g_hash_table_insert(md->ht, new_key, p_value);
            g_hash_table_iter_steal(&iter);
        }
    }


    // Cleanup

    cr_destroy_metadata_hashtable(intern_hashtable);

    return CRE_OK;
}

int
cr_metadata_locate_and_load_xml(cr_Metadata md,
                                const char *repopath,
                                GError **err)
{
    int ret;
    struct cr_MetadataLocation *ml;
    GError *tmp_err = NULL;

    assert(md);
    assert(repopath);

    ml = cr_locate_metadata(repopath, 1, &tmp_err);
    if (tmp_err) {
        int code = tmp_err->code;
        g_propagate_error(err, tmp_err);
        return code;
    }

    ret = cr_metadata_load_xml(md, ml, err);
    cr_metadatalocation_free(ml);

    return ret;
}
