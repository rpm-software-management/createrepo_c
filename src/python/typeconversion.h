/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012-2013  Tomas Mlcoch
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

#ifndef CR_TYPECONVERSION_PY_H
#define CR_TYPECONVERSION_PY_H

#include <glib.h>
#include "src/createrepo_c.h"

// Clears the current Python Exception and return its representation in GError
void PyErr_ToGError(GError **err);

PyObject *PyUnicodeOrNone_FromString(const char *str);
PyObject *PyObject_ToPyBytesOrNull(PyObject *pyobj);
char *PyObject_ToChunkedString(PyObject *pyobj, GStringChunk *chunk);

PyObject *PyObject_FromDependency(cr_Dependency *dep);
cr_Dependency *PyObject_ToDependency(PyObject *tuple, GStringChunk *chunk);

PyObject *PyObject_FromPackageFile(cr_PackageFile *file);
cr_PackageFile *PyObject_ToPackageFile(PyObject *tuple, GStringChunk *chunk);

PyObject *PyObject_FromChangelogEntry(cr_ChangelogEntry *log);
cr_ChangelogEntry *PyObject_ToChangelogEntry(PyObject *tuple, GStringChunk *chunk);

PyObject *PyObject_FromDistroTag(cr_DistroTag *tag);
cr_DistroTag *PyObject_ToDistroTag(PyObject *tuple, GStringChunk *chunk);

GSList *GSList_FromPyList_Str(PyObject *py_list);

#endif
