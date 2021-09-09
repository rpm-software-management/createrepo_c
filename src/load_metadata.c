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

#ifdef WITH_LIBMODULEMD
#include <modulemd.h>
#endif /* WITH_LIBMODULEMD */

#include "error.h"
#include "package.h"
#include "misc.h"
#include "load_metadata.h"
#include "locate_metadata.h"
#include "xml_parser.h"

#define ERR_DOMAIN              CREATEREPO_C_ERROR
#define STRINGCHUNK_SIZE        16384

/** Structure for loaded metadata
 */
struct _cr_Metadata {
    cr_HashTableKey key;    /*!< key used in hashtable */
    GHashTable *ht;         /*!< hashtable with packages */
    GStringChunk *chunk;    /*!< NULL or string chunk with strings from htn */
    GHashTable *pkglist_ht; /*!< list of allowed package basenames to load */
    cr_HashTableKeyDupAction dupaction; /*!<
        How to behave in case of duplicated items */

#ifdef WITH_LIBMODULEMD
    ModulemdModuleIndex *moduleindex; /*!< Module metadata */
#endif /* WITH_LIBMODULEMD */
};

cr_HashTableKey
cr_metadata_key(cr_Metadata *md)
{
    assert(md);
    return md->key;
}

GHashTable *
cr_metadata_hashtable(cr_Metadata *md)
{
    assert(md);
    return md->ht;
}

#ifdef WITH_LIBMODULEMD
ModulemdModuleIndex *
cr_metadata_modulemd(cr_Metadata *md)
{
    assert(md);
    return md->moduleindex;
}
#endif /* WITH_LIBMODULEMD */

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

cr_Metadata *
cr_metadata_new(cr_HashTableKey key, int use_single_chunk, GSList *pkglist)
{
    cr_Metadata *md;

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

    md->dupaction = CR_HT_DUPACT_KEEPFIRST;

    return md;
}

void
cr_metadata_free(cr_Metadata *md)
{
    if (!md)
        return;

#ifdef WITH_LIBMODULEMD
    g_clear_pointer (&(md->moduleindex), g_object_unref);
#endif /* WITH_LIBMODULEMD */

    cr_destroy_metadata_hashtable(md->ht);
    if (md->chunk)
        g_string_chunk_free(md->chunk);
    if (md->pkglist_ht)
        g_hash_table_destroy(md->pkglist_ht);
    g_free(md);
}

gboolean
cr_metadata_set_dupaction(cr_Metadata *md, cr_HashTableKeyDupAction dupaction)
{
    if (!md || dupaction >= CR_HT_DUPACT_SENTINEL)
        return FALSE;
    md->dupaction = dupaction;
    return TRUE;
}

// Callbacks for XML parsers

typedef enum {
    PARSING_PRI,
    PARSING_FIL,
    PARSING_OTH,
} cr_ParsingState;

typedef struct {
    GHashTable      *ht;
    GStringChunk    *chunk;
    GHashTable      *pkglist_ht;
    GHashTable      *ignored_pkgIds; /*!< If there are multiple packages
        which have the same checksum (pkgId) but they are in fact different
        (they have different basenames, mtimes or sizes),
        then we want to ignore these packages during
        loading. It's because the pkgId is used to pair metadata from
        primary.xml with metadata from filelists.xml and other.xml and
        we want the pkgId to be unique.
        Key is pkgId and value is NULL. */
    cr_ParsingState state;
    gint64          pkgKey; /*!< basically order of the package */
} cr_CbData;

static int
primary_newpkgcb(cr_Package **pkg,
                 G_GNUC_UNUSED const char *pkgId,
                 G_GNUC_UNUSED const char *name,
                 G_GNUC_UNUSED const char *arch,
                 void *cbdata,
                 G_GNUC_UNUSED GError **err)
{
    cr_CbData *cb_data = cbdata;

    assert(*pkg == NULL);

    if (cb_data->chunk) {
        *pkg = cr_package_new_without_chunk();
        (*pkg)->chunk = cb_data->chunk;
        (*pkg)->loadingflags |= CR_PACKAGE_SINGLE_CHUNK;
    } else {
        *pkg = cr_package_new();
    }

    return CR_CB_RET_OK;
}

static int
primary_pkgcb(cr_Package *pkg, void *cbdata, G_GNUC_UNUSED GError **err)
{
    gboolean store_pkg = TRUE;
    cr_CbData *cb_data = cbdata;
    cr_Package *epkg;
    char *basename = cr_get_filename(pkg->location_href);

    assert(pkg);
    assert(pkg->pkgId);

    if (cb_data->chunk) {
        // Set pkg internal chunk to NULL,
        // if global chunk for all packages is used
        assert(pkg->chunk == cb_data->chunk);
        pkg->chunk = NULL;
    }

    if (cb_data->pkglist_ht && basename) {
        // If a pkglist was specified,
        // check if the package should be loaded or not
        store_pkg = g_hash_table_lookup_extended(cb_data->pkglist_ht,
                                                 basename, NULL, NULL);
    }

    if (store_pkg) {
        // Check if pkgId is not on the list of blocked Ids
        if (g_hash_table_lookup_extended(cb_data->ignored_pkgIds, pkg->pkgId,
                                         NULL, NULL))
            // We should ignore this pkgId (package's hash)
            store_pkg = FALSE;
    }

    if (!store_pkg) {
        // Drop the currently loaded package
        cr_package_free(pkg);
        return CR_CB_RET_OK;
    }

    epkg = g_hash_table_lookup(cb_data->ht, pkg->pkgId);

    if (!epkg) {
        // Store package into the hashtable
        pkg->loadingflags |= CR_PACKAGE_FROM_XML;
        pkg->loadingflags |= CR_PACKAGE_LOADED_PRI;
        g_hash_table_replace(cb_data->ht, pkg->pkgId, pkg);
    } else {
        // Package with the same pkgId (hash) already exists
        if (epkg->time_file == pkg->time_file
            && epkg->size_package == pkg->size_package
            && !g_strcmp0(cr_get_filename(pkg->location_href), basename))
        {
            // The existing package is the same as the current one.
            // This is ok
            g_debug("Multiple packages with the same checksum: %s. "
                    "Loading the info only once.", pkg->pkgId);
        } else {
            // The existing package is different. We have two different
            // packages with the same checksum -> drop both of them
            // and append this checksum to the ignored_pkgIds
            // XXX: Note that this constrain works only for single repo!
            //      If the same cr_Metadata are loaded from several different
            //      repos, than inter-repo packages with matching checksum
            //      are not checked.
            g_debug("Multiple different packages (basename, mtime or size "
                    "doesn't match) with the same checksum: %s. "
                    "Ignoring all packages with the checksum.", pkg->pkgId);
            g_hash_table_remove(cb_data->ht, pkg->pkgId);
            g_hash_table_replace(cb_data->ignored_pkgIds, g_strdup(pkg->pkgId), NULL);
        }

        // Drop the currently loaded package
        cr_package_free(pkg);
        return CR_CB_RET_OK;
    }

    ++cb_data->pkgKey;
    pkg->pkgKey = cb_data->pkgKey;

    return CR_CB_RET_OK;
}

static int
newpkgcb(cr_Package **pkg,
         const char *pkgId,
         G_GNUC_UNUSED const char *name,
         G_GNUC_UNUSED const char *arch,
         void *cbdata,
         G_GNUC_UNUSED GError **err)
{
    cr_CbData *cb_data = cbdata;

    assert(*pkg == NULL);
    assert(pkgId);

    *pkg = g_hash_table_lookup(cb_data->ht, pkgId);

    if (*pkg) {
        // If package with the pkgId was parsed from primary.xml, then...

        if (cb_data->state == PARSING_FIL) {
            if ((*pkg)->loadingflags & CR_PACKAGE_LOADED_FIL) {
                // For package with this checksum, the filelist was
                // already loaded.
                *pkg = NULL;
            } else {
                // Make a note that filelist is parsed
                (*pkg)->loadingflags |= CR_PACKAGE_LOADED_FIL;
            }
        }

        if (cb_data->state == PARSING_OTH) {
            if ((*pkg)->loadingflags & CR_PACKAGE_LOADED_OTH) {
                // For package with this checksum, the other (changelogs) were
                // already loaded.
                *pkg = NULL;
            } else {
                // Make a note that other is parsed
                (*pkg)->loadingflags |= CR_PACKAGE_LOADED_OTH;
            }
        }

        if (*pkg && cb_data->chunk) {
            assert(!(*pkg)->chunk);
            (*pkg)->chunk = cb_data->chunk;
        }
    }

    return CR_CB_RET_OK;
}

static int
pkgcb(cr_Package *pkg, void *cbdata, G_GNUC_UNUSED GError **err)
{
    cr_CbData *cb_data = cbdata;

    if (cb_data->chunk) {
        assert(pkg->chunk == cb_data->chunk);
        pkg->chunk = NULL;
    }

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
    cb_data.state           = PARSING_PRI;
    cb_data.ht              = hashtable;
    cb_data.chunk           = chunk;
    cb_data.pkglist_ht      = pkglist_ht;
    cb_data.ignored_pkgIds  = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                    g_free, NULL);
    cb_data.pkgKey          = G_GINT64_CONSTANT(0);

    cr_xml_parse_primary(primary_xml_path,
                         primary_newpkgcb,
                         &cb_data,
                         primary_pkgcb,
                         &cb_data,
                         cr_warning_cb,
                         "Primary XML parser",
                         (filelists_xml_path) ? 0 : 1,
                         &tmp_err);

    g_hash_table_destroy(cb_data.ignored_pkgIds);
    cb_data.ignored_pkgIds = NULL;

    if (tmp_err) {
        int code = tmp_err->code;
        g_debug("primary.xml parsing error: %s", tmp_err->message);
        g_propagate_prefixed_error(err, tmp_err, "primary.xml parsing: ");
        return code;
    }

    cb_data.state = PARSING_FIL;

    if (filelists_xml_path) {
        cr_xml_parse_filelists(filelists_xml_path,
                               newpkgcb,
                               &cb_data,
                               pkgcb,
                               &cb_data,
                               cr_warning_cb,
                               "Filelists XML parser",
                               &tmp_err);
        if (tmp_err) {
            int code = tmp_err->code;
            g_debug("filelists.xml parsing error: %s", tmp_err->message);
            g_propagate_prefixed_error(err, tmp_err, "filelists.xml parsing: ");
            return code;
        }
    }

    cb_data.state = PARSING_OTH;

    if (other_xml_path) {
        cr_xml_parse_other(other_xml_path,
                           newpkgcb,
                           &cb_data,
                           pkgcb,
                           &cb_data,
                           cr_warning_cb,
                           "Other XML parser",
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

static gint
module_read_fn (void *data,
                unsigned char *buffer,
                size_t size,
                size_t *size_read)
{
    int ret;
    GError *tmp_err = NULL;
    CR_FILE *cr_file = (CR_FILE *)data;

    ret = cr_read (cr_file, buffer, size, &tmp_err);
    if (ret == CR_CW_ERR) {
        g_clear_pointer (&tmp_err, g_error_free);
        return 0;
    }

    *size_read = ret;
    return 1;
}

#ifdef WITH_LIBMODULEMD
int
cr_metadata_load_modulemd(ModulemdModuleIndex **moduleindex,
                          gchar *path_to_md,
                          GError **err)
{
    int ret;
    gboolean result;
    GError *tmp_err = NULL;
    const GError *subdoc_error = NULL;
    CR_FILE *modulemd = NULL;
    g_autoptr (GPtrArray) failures = NULL;

    *moduleindex = modulemd_module_index_new();
    if (!*moduleindex) {
        g_set_error (err, ERR_DOMAIN, CRE_MEMORY,
                     "Could not allocate module index");
        return CRE_MEMORY;
    }

    /* Open the metadata location */
    modulemd = cr_open(path_to_md,
                       CR_CW_MODE_READ,
                       CR_CW_AUTO_DETECT_COMPRESSION,
                       &tmp_err);
    if (tmp_err) {
        int code = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Cannot open %s: ",
                                   path_to_md);
        return code;
    }

    result = modulemd_module_index_update_from_custom (*moduleindex,
                                                       module_read_fn,
                                                       modulemd,
                                                       TRUE,
                                                       &failures,
                                                       &tmp_err);
    if (!result) {
        if (!tmp_err){
            if (failures->len) {
                subdoc_error = modulemd_subdocument_info_get_gerror(g_ptr_array_index(failures, 0));
                if (subdoc_error)
                {
                    g_set_error(err, CRE_MODULEMD, CREATEREPO_C_ERROR,
                                "Error in \"%s\" : %s",
                                g_path_get_basename(path_to_md),
                                subdoc_error->message);
                    return CRE_MODULEMD;
                }
            }

            g_set_error(err, CRE_MODULEMD, CREATEREPO_C_ERROR,
                        "Unknown error in libmodulemd with %s",
                        path_to_md);
        }else{
            g_propagate_error (err, tmp_err);
        }
        return CRE_MODULEMD;
    }

    ret = CRE_OK;

    cr_close(modulemd, &tmp_err);
    if (tmp_err) {
        ret = tmp_err->code;
        g_propagate_prefixed_error(err, tmp_err, "Error while closing: ");

    }

    return ret;
}
#endif /* WITH_LIBMODULEMD */

int
cr_metadata_load_xml(cr_Metadata *md,
                     struct cr_MetadataLocation *ml,
                     GError **err)
{
    int result;
    GError *tmp_err = NULL;
    GHashTable *intern_hashtable;  // key is checksum (pkgId)
    cr_HashTableKeyDupAction dupaction = md->dupaction;

    assert(md);
    assert(ml);
    assert(!err || *err == NULL);

    if (!ml->pri_xml_href) {
        g_set_error(err, ERR_DOMAIN, CRE_BADARG,
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
    GHashTable *ignored_keys = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                     g_free, NULL);

    g_hash_table_iter_init (&iter, intern_hashtable);
    while (g_hash_table_iter_next (&iter, &p_key, &p_value)) {
        cr_Package *pkg = (cr_Package *) p_value;
        cr_Package *epkg;
        gpointer new_key;

        switch (md->key) {
            case CR_HT_KEY_FILENAME:
                new_key = cr_get_filename(pkg->location_href);
                break;
            case CR_HT_KEY_HREF:
                new_key = cr_get_cleaned_href(pkg->location_href);
                break;
            case CR_HT_KEY_HASH:
                new_key = pkg->pkgId;
                break;
            case CR_HT_KEY_NAME:
                new_key = pkg->name;
                break;
            default:
                // Well, this SHOULD never happend!
                // (md->key SHOULD be set only by cr_metadata_new()
                // and it SHOULD set only valid key values)
                g_critical("%s: Unknown hash table key selected", __func__);
                assert(0);
                g_set_error(err, ERR_DOMAIN, CRE_ASSERT,
                            "Bad db type");
                return CRE_ASSERT;
        }

        epkg = g_hash_table_lookup(md->ht, new_key);
        if (epkg) {
            // Such key already exists
            if (dupaction == CR_HT_DUPACT_KEEPFIRST) {
                g_debug("%s: Key \"%s\" already exists in hashtable - Keeping the first occurrence",
                        __func__, (char *) new_key);
            } else {
                if (pkg->time_file != epkg->time_file
                    || pkg->size_package != epkg->size_package
                    || g_strcmp0(pkg->pkgId, epkg->pkgId)
                    || g_strcmp0(cr_get_filename(pkg->location_href),
                                 cr_get_filename(epkg->location_href))
                    )
                {
                    // We got a key (checksum, filename, pkg name, ..)
                    // which has a multiple occurrences which are different.
                    // Ignore such key
                    g_debug("%s: Key \"%s\" is present multiple times and with "
                            "different values. Ignoring all occurrences. "
                            "[size_package: %"G_GINT64_FORMAT"|%"G_GINT64_FORMAT
                            "; time_file: %"G_GINT64_FORMAT"|%"G_GINT64_FORMAT
                            "; pkgId: %s|%s; basename: %s|%s]",
                            __func__, (gchar *) new_key,
                            pkg->size_package, epkg->size_package,
                            pkg->time_file, epkg->time_file,
                            pkg->pkgId, epkg->pkgId,
                            cr_get_filename(pkg->location_href),
                            cr_get_filename(epkg->location_href));
                    g_hash_table_insert(ignored_keys, g_strdup((gchar *) new_key), NULL);
                }
            }
            // Remove the package from the iterator anyway
            g_hash_table_iter_remove(&iter);
        } else {
            g_hash_table_insert(md->ht, new_key, p_value);
            g_hash_table_iter_steal(&iter);
        }
    }

    // Remove ignored_keys from resulting hashtable
    g_hash_table_iter_init(&iter, ignored_keys);
    while (g_hash_table_iter_next(&iter, &p_key, &p_value)) {
        char *key = (gchar *) p_key;
        g_hash_table_remove(md->ht, key);
    }

    // How much items we really use
    g_debug("%s: Really usable items: %d", __func__,
            g_hash_table_size(md->ht));

    // Cleanup

    g_hash_table_destroy(ignored_keys);
    cr_destroy_metadata_hashtable(intern_hashtable);

    result = CRE_OK;

#ifdef WITH_LIBMODULEMD
    if (g_slist_find_custom(ml->additional_metadata, "modules", cr_cmp_metadatum_type)){
      cr_Metadatum *modulemd_metadatum = g_slist_find_custom(ml->additional_metadata, "modules", cr_cmp_metadatum_type)->data;
      result = cr_metadata_load_modulemd(&(md->moduleindex), modulemd_metadatum->name, err);
    }
#endif /* WITH_LIBMODULEMD */

    return result;
}

int
cr_metadata_locate_and_load_xml(cr_Metadata *md,
                                const char *repopath,
                                GError **err)
{
    int ret;
    struct cr_MetadataLocation *ml;
    GError *tmp_err = NULL;

    assert(md);
    assert(repopath);

    ml = cr_locate_metadata(repopath, TRUE, &tmp_err);
    if (tmp_err) {
        g_clear_pointer(&ml, cr_metadatalocation_free);
        int code = tmp_err->code;
        g_propagate_error(err, tmp_err);
        return code;
    }

    ret = cr_metadata_load_xml(md, ml, err);

    cr_metadatalocation_free(ml);

    return ret;
}
