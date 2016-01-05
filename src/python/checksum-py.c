/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2013  Tomas Mlcoch
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

#include <Python.h>
#include <assert.h>
#include <stddef.h>

#include "src/createrepo_c.h"

#include "typeconversion.h"
#include "exception-py.h"

PyObject *
py_checksum_name_str(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    int type;

    if (!PyArg_ParseTuple(args, "i:py_checksum_name_Str", &type))
        return NULL;

    return PyUnicodeOrNone_FromString(cr_checksum_name_str(type));
}

PyObject *
py_checksum_type(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    char *type;

    if (!PyArg_ParseTuple(args, "s:py_checksum_type", &type))
        return NULL;

    return PyLong_FromLong((long) cr_checksum_type(type));
}
