/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2014  Tomas Mlcoch
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
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "updateinfo.h"
#include "error.h"
#include "misc.h"
#include "checksum.h"


/*
 * cr_UpdateCollectionPackage
 */

cr_UpdateCollectionPackage *
cr_updatecollectionpackage_new(void)
{
    cr_UpdateCollectionPackage *pkg = g_malloc0(sizeof(*pkg));
    pkg->chunk = g_string_chunk_new(0);
    return pkg;
}

cr_UpdateCollectionPackage *
cr_updatecollectionpackage_copy(const cr_UpdateCollectionPackage *orig)
{
    cr_UpdateCollectionPackage *pkg;

    if (!orig) return NULL;

    pkg = cr_updatecollectionpackage_new();

    pkg->name     = cr_safe_string_chunk_insert(pkg->chunk, orig->name);
    pkg->version  = cr_safe_string_chunk_insert(pkg->chunk, orig->version);
    pkg->release  = cr_safe_string_chunk_insert(pkg->chunk, orig->release);
    pkg->epoch    = cr_safe_string_chunk_insert(pkg->chunk, orig->epoch);
    pkg->arch     = cr_safe_string_chunk_insert(pkg->chunk, orig->arch);
    pkg->src      = cr_safe_string_chunk_insert(pkg->chunk, orig->src);
    pkg->filename = cr_safe_string_chunk_insert(pkg->chunk, orig->filename);
    pkg->sum      = cr_safe_string_chunk_insert(pkg->chunk, orig->sum);

    pkg->sum_type = orig->sum_type;
    pkg->reboot_suggested = orig->reboot_suggested;
    pkg->restart_suggested = orig->restart_suggested;
    pkg->relogin_suggested = orig->relogin_suggested;

    return pkg;
}

void
cr_updatecollectionpackage_free(cr_UpdateCollectionPackage *pkg)
{
    if (!pkg)
        return;
    g_string_chunk_free(pkg->chunk);
    g_free(pkg);
}


/*
 * cr_UpdateCollectionModule
 */

cr_UpdateCollectionModule *
cr_updatecollectionmodule_new(void)
{
    cr_UpdateCollectionModule *module = g_malloc0(sizeof(*module));
    module->chunk = g_string_chunk_new(0);
    return module;
}

cr_UpdateCollectionModule *
cr_updatecollectionmodule_copy(const cr_UpdateCollectionModule *orig)
{
    cr_UpdateCollectionModule *module;

    if (!orig) return NULL;

    module = cr_updatecollectionmodule_new();

    module->name    = cr_safe_string_chunk_insert(module->chunk, orig->name);
    module->stream  = cr_safe_string_chunk_insert(module->chunk, orig->stream);
    module->version = orig->version;
    module->context = cr_safe_string_chunk_insert(module->chunk, orig->context);
    module->arch    = cr_safe_string_chunk_insert(module->chunk, orig->arch);

    return module;
}

void
cr_updatecollectionmodule_free(cr_UpdateCollectionModule *module)
{
    if (!module)
        return;
    g_string_chunk_free(module->chunk);
    g_free(module);
}


/*
 * cr_UpdateCollection
 */

cr_UpdateCollection *
cr_updatecollection_new(void)
{
    cr_UpdateCollection *collection = g_malloc0(sizeof(*collection));
    collection->chunk = g_string_chunk_new(0);
    return collection;
}

cr_UpdateCollection *
cr_updatecollection_copy(const cr_UpdateCollection *orig)
{
    cr_UpdateCollection *col;

    if (!orig) return NULL;

    col = cr_updatecollection_new();

    col->shortname = cr_safe_string_chunk_insert(col->chunk, orig->shortname);
    col->name      = cr_safe_string_chunk_insert(col->chunk, orig->name);

    if (orig->module) {
      col->module = cr_updatecollectionmodule_copy(orig->module);
    }

    if (orig->packages) {
        GSList *newlist = NULL;
        for (GSList *elem = orig->packages; elem; elem = g_slist_next(elem)) {
            cr_UpdateCollectionPackage *pkg = elem->data;
            newlist = g_slist_prepend(newlist,
                                      cr_updatecollectionpackage_copy(pkg));
        }
        col->packages = g_slist_reverse(newlist);
    }

    return col;
}

void
cr_updatecollection_free(cr_UpdateCollection *collection)
{
    if (!collection)
        return;
    cr_updatecollectionmodule_free(collection->module);
    cr_slist_free_full(collection->packages,
                       (GDestroyNotify) cr_updatecollectionpackage_free);
    g_string_chunk_free(collection->chunk);
    g_free(collection);
}

void
cr_updatecollection_append_package(cr_UpdateCollection *collection,
                                   cr_UpdateCollectionPackage *pkg)
{
    if (!collection || !pkg) return;
    collection->packages = g_slist_append(collection->packages, pkg);
}


/*
 * cr_UpdateReference
 */

cr_UpdateReference *
cr_updatereference_new(void)
{
    cr_UpdateReference *ref = g_malloc0(sizeof(*ref));
    ref->chunk = g_string_chunk_new(0);
    return ref;
}

cr_UpdateReference *
cr_updatereference_copy(const cr_UpdateReference *orig)
{
    cr_UpdateReference *ref;

    if (!orig) return NULL;

    ref = cr_updatereference_new();

    ref->href  = cr_safe_string_chunk_insert(ref->chunk, orig->href);
    ref->id    = cr_safe_string_chunk_insert(ref->chunk, orig->id);
    ref->type  = cr_safe_string_chunk_insert(ref->chunk, orig->type);
    ref->title = cr_safe_string_chunk_insert(ref->chunk, orig->title);

    return ref;
}

void
cr_updatereference_free(cr_UpdateReference *ref)
{
    if (!ref)
        return;
    g_string_chunk_free(ref->chunk);
    g_free(ref);
}


/*
 * cr_UpdateRecord
 */

cr_UpdateRecord *
cr_updaterecord_new(void)
{
    cr_UpdateRecord *rec = g_malloc0(sizeof(*rec));
    rec->chunk = g_string_chunk_new(0);
    return rec;
}

cr_UpdateRecord *
cr_updaterecord_copy(const cr_UpdateRecord *orig)
{
    cr_UpdateRecord *rec;

    if (!orig) return NULL;

    rec = cr_updaterecord_new();

    rec->from = cr_safe_string_chunk_insert(rec->chunk, orig->from);
    rec->status = cr_safe_string_chunk_insert(rec->chunk, orig->status);
    rec->type = cr_safe_string_chunk_insert(rec->chunk, orig->type);
    rec->version = cr_safe_string_chunk_insert(rec->chunk, orig->version);
    rec->id = cr_safe_string_chunk_insert(rec->chunk, orig->id);
    rec->title = cr_safe_string_chunk_insert(rec->chunk, orig->title);
    rec->issued_date = cr_safe_string_chunk_insert(rec->chunk, orig->issued_date);
    rec->updated_date = cr_safe_string_chunk_insert(rec->chunk, orig->updated_date);
    rec->rights = cr_safe_string_chunk_insert(rec->chunk, orig->rights);
    rec->release = cr_safe_string_chunk_insert(rec->chunk, orig->release);
    rec->pushcount = cr_safe_string_chunk_insert(rec->chunk, orig->pushcount);
    rec->severity = cr_safe_string_chunk_insert(rec->chunk, orig->severity);
    rec->summary = cr_safe_string_chunk_insert(rec->chunk, orig->summary);
    rec->description = cr_safe_string_chunk_insert(rec->chunk, orig->description);
    rec->solution = cr_safe_string_chunk_insert(rec->chunk, orig->solution);
    rec->reboot_suggested = orig->reboot_suggested;

    if (orig->references) {
        GSList *newlist = NULL;
        for (GSList *elem = orig->references; elem; elem = g_slist_next(elem)) {
            cr_UpdateReference *ref = elem->data;
            newlist = g_slist_prepend(newlist,
                                      cr_updatereference_copy(ref));
        }
        rec->references = g_slist_reverse(newlist);
    }

    if (orig->collections) {
        GSList *newlist = NULL;
        for (GSList *elem = orig->collections; elem; elem = g_slist_next(elem)) {
            cr_UpdateCollection *col = elem->data;
            newlist = g_slist_prepend(newlist,
                                      cr_updatecollection_copy(col));
        }
        rec->collections = g_slist_reverse(newlist);
    }

    return rec;
}

void
cr_updaterecord_free(cr_UpdateRecord *rec)
{
    if (!rec)
        return;
    cr_slist_free_full(rec->references, (GDestroyNotify) cr_updatereference_free);
    cr_slist_free_full(rec->collections, (GDestroyNotify) cr_updatecollection_free);
    g_string_chunk_free(rec->chunk);
    g_free(rec);
}

void
cr_updaterecord_append_reference(cr_UpdateRecord *record,
                                 cr_UpdateReference *ref)
{
    if (!record || !ref) return;
    record->references = g_slist_append(record->references, ref);
}

void
cr_updaterecord_append_collection(cr_UpdateRecord *record,
                                  cr_UpdateCollection *collection)
{
    if (!record || !collection) return;
    record->collections = g_slist_append(record->collections, collection);
}


/*
 * cr_Updateinfo
 */

cr_UpdateInfo *
cr_updateinfo_new(void)
{
    cr_UpdateInfo *uinfo = g_malloc0(sizeof(*uinfo));
    return uinfo;
}

void
cr_updateinfo_free(cr_UpdateInfo *uinfo)
{
    if (!uinfo)
        return;
    cr_slist_free_full(uinfo->updates, (GDestroyNotify) cr_updaterecord_free);
    g_free(uinfo);
}

void
cr_updateinfo_apped_record(cr_UpdateInfo *uinfo, cr_UpdateRecord *record)
{
    if (!uinfo || !record) return;
    uinfo->updates = g_slist_append(uinfo->updates, record);
}

