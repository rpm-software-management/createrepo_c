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

#ifndef CR_UPDATECOLLECTION_PY_H
#define CR_UPDATECOLLECTION_PY_H

#include "src/createrepo_c.h"

extern PyTypeObject UpdateCollection_Type;

#define UpdateCollectionObject_Check(o) \
            PyObject_TypeCheck(o, &UpdateCollection_Type)

PyObject *Object_FromUpdateCollection(cr_UpdateCollection *rec);
cr_UpdateCollection *UpdateCollection_FromPyObject(PyObject *o);

#endif
