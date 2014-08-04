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

#ifndef CR_XML_DUMP_PY_H
#define CR_XML_DUMP_PY_H

#include "src/createrepo_c.h"

PyDoc_STRVAR(xml_dump_primary__doc__,
"xml_dump_primary(pkg) -> str\n\n"
"Generate primary xml chunk from the package");

PyObject *py_xml_dump_primary(PyObject *self, PyObject *args);

PyDoc_STRVAR(xml_dump_filelists__doc__,
"xml_dump_filelists(pkg) -> str\n\n"
"Generate filelists xml chunk from the package");

PyObject *py_xml_dump_filelists(PyObject *self, PyObject *args);

PyDoc_STRVAR(xml_dump_other__doc__,
"xml_dump_other(pkg) -> str\n\n"
"Generate other xml chunk from the package");

PyObject *py_xml_dump_other(PyObject *self, PyObject *args);

PyDoc_STRVAR(xml_dump__doc__,
"xml_dump(pkg) -> (str, str, str)\n\n"
"Generate primary, filelists and other xml chunks from the package");

PyObject *py_xml_dump(PyObject *self, PyObject *args);

PyDoc_STRVAR(xml_dump_updaterecord__doc__,
"xml_dump_updaterecord(pkg) -> str\n\n"
"Generate xml chunk from UpdateRecord");

PyObject *py_xml_dump_updaterecord(PyObject *self, PyObject *args);

#endif
