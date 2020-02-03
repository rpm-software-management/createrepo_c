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

#ifndef __C_CREATEREPOLIB_UPDATEINFO_H__
#define __C_CREATEREPOLIB_UPDATEINFO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include "checksum.h"

/** \defgroup   updateinfo            Updateinfo API.
 *
 * Module for generating updateinfo.xml.
 *
 *  \addtogroup updateinfo
 *  @{
 */

typedef struct {
    gchar *name;
    gchar *version;
    gchar *release;
    gchar *epoch;
    gchar *arch;
    gchar *src;
    gchar *filename;
    gchar *sum;
    cr_ChecksumType sum_type;
    gboolean reboot_suggested;
    gboolean restart_suggested;
    gboolean relogin_suggested;

    GStringChunk *chunk;
} cr_UpdateCollectionPackage;

typedef struct {
    gchar *name;
    gchar *stream;
    guint64 version;
    gchar *context;
    gchar *arch;

    GStringChunk *chunk;
} cr_UpdateCollectionModule;

typedef struct {
    gchar *shortname;   /*!< e.g. rhn-tools-rhel-x86_64-server-6.5.aus */
    gchar *name;        /*!< e.g. RHN Tools for RHEL AUS (v. 6.5 for 64-bit x86_64) */
    cr_UpdateCollectionModule *module;
    GSList *packages;   /*!< List of cr_UpdateCollectionPackage */
    GStringChunk *chunk;
} cr_UpdateCollection;

typedef struct {
    gchar *href;    /*!< URL (e.g. to related bugzilla, errata, ...) */
    gchar *id;      /*!< id (e.g. 1035288, NULL for errata, ...) */
    gchar *type;    /*!< reference type ("self" for errata, "bugzilla", ...) */
    gchar *title;   /*!< Name of errata, name of bug, etc. */
    GStringChunk *chunk;
} cr_UpdateReference;

typedef struct {
    gchar *from;        /*!< Source of the update (e.g. security@redhat.com) */
    gchar *status;      /*!< Update status ("final", ...) */
    gchar *type;        /*!< Update type ("enhancement", "bugfix", ...) */
    gchar *version;     /*!< Update version (probably always an integer number) */
    gchar *id;          /*!< Update id (short update name, e.g. RHEA-2013:1777) */
    gchar *title;       /*!< Update name */
    gchar *issued_date; /*!< Date string (e.g. "2013-12-02 00:00:00") */
    gchar *updated_date;/*!< Date string */
    gchar *rights;      /*!< Copyright */
    gchar *release;     /*!< Release */
    gchar *pushcount;   /*!< Push count */
    gchar *severity;    /*!< Severity */
    gchar *summary;     /*!< Short summary */
    gchar *description; /*!< Update description */
    gchar *solution;    /*!< Solution */
    gboolean reboot_suggested; /*!< Reboot suggested */

    GSList *references; /*!< List of cr_UpdateReference */
    GSList *collections;/*!< List of cr_UpdateCollection */

    GStringChunk *chunk;/*!< String chunk */
} cr_UpdateRecord;

typedef struct {
    GSList *updates;    /*!< List of cr_UpdateRecord */
} cr_UpdateInfo;

/*
 * cr_UpdateCollectionPackage
 */

cr_UpdateCollectionPackage *
cr_updatecollectionpackage_new(void);

cr_UpdateCollectionPackage *
cr_updatecollectionpackage_copy(const cr_UpdateCollectionPackage *orig);

void
cr_updatecollectionpackage_free(cr_UpdateCollectionPackage *pkg);

/*
 * cr_UpdateCollectionModule
 */

cr_UpdateCollectionModule *
cr_updatecollectionmodule_new(void);

cr_UpdateCollectionModule *
cr_updatecollectionmodule_copy(const cr_UpdateCollectionModule *orig);

void
cr_updatecollectionmodule_free(cr_UpdateCollectionModule *pkg);

/*
 * cr_UpdateCollection
 */

cr_UpdateCollection *
cr_updatecollection_new(void);

cr_UpdateCollection *
cr_updatecollection_copy(const cr_UpdateCollection *orig);

void
cr_updatecollection_free(cr_UpdateCollection *collection);

void
cr_updatecollection_append_package(cr_UpdateCollection *collection,
                                   cr_UpdateCollectionPackage *pkg);

/*
 * cr_UpdateReference
 */

cr_UpdateReference *
cr_updatereference_new(void);

cr_UpdateReference *
cr_updatereference_copy(const cr_UpdateReference *orig);

void
cr_updatereference_free(cr_UpdateReference *ref);

/*
 * cr_UpdateRecord
 */

cr_UpdateRecord *
cr_updaterecord_new(void);

cr_UpdateRecord *
cr_updaterecord_copy(const cr_UpdateRecord *orig);

void
cr_updaterecord_free(cr_UpdateRecord *record);

void
cr_updaterecord_append_reference(cr_UpdateRecord *record,
                                 cr_UpdateReference *ref);

void
cr_updaterecord_append_collection(cr_UpdateRecord *record,
                                  cr_UpdateCollection *collection);

/*
 * cr_Updateinfo
 */

cr_UpdateInfo *
cr_updateinfo_new(void);

void
cr_updateinfo_free(cr_UpdateInfo *uinfo);

void
cr_updateinfo_apped_record(cr_UpdateInfo *uinfo, cr_UpdateRecord *record);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_UPDATEINFO_H__ */
