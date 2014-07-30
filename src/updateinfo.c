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
 * cr_UpdateCollection
 */

cr_UpdateCollection *
cr_updatecollection_new(void)
{
    cr_UpdateCollection *collection = g_malloc0(sizeof(*collection));
    return collection;
}

void
cr_updatecollection_free(cr_UpdateCollection *collection)
{
    if (!collection)
        return;
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
    record->collections = g_slist_append(record->collections, record);
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

