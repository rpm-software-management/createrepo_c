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

#ifndef CR_CHECKSUM_PY_H
#define CR_CHECKSUM_PY_H

#include "src/createrepo_c.h"

PyDoc_STRVAR(checksum_name_str__doc__,
"checksum_name_str(checksum_type) -> str\n\n"
"Checksum name from checksum type constant");

PyObject *py_checksum_name_str(PyObject *self, PyObject *args);

PyDoc_STRVAR(checksum_type__doc__,
"checksum_type(checksum_name) -> long\n\n"
"Checksum type from checksum name");

PyObject *py_checksum_type(PyObject *self, PyObject *args);

#endif
