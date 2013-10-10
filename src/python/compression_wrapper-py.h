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

#ifndef CR_COMPRESSION_WRAPPER_PY_H
#define CR_COMPRESSION_WRAPPER_PY_H

#include "src/createrepo_c.h"

extern PyTypeObject CrFile_Type;

#define CrFileObject_Check(o)   PyObject_TypeCheck(o, &CrFile_Type)

PyDoc_STRVAR(compression_suffix__doc__,
"compression_suffix(compression_type) -> str or None\n\n"
"Compression suffix for the compression type");

PyObject *py_compression_suffix(PyObject *self, PyObject *args);

PyDoc_STRVAR(detect_compression__doc__,
"detect_compression(path) -> long\n\n"
"Detect compression type used on the file");

PyObject *py_detect_compression(PyObject *self, PyObject *args);

PyDoc_STRVAR(compression_type__doc__,
"compression_type(string) -> int\n\n"
"Compression type value");

PyObject *py_compression_type(PyObject *self, PyObject *args);


#endif
