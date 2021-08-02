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

#ifndef CR_XML_PARSER_PY_H
#define CR_XML_PARSER_PY_H

#include "src/createrepo_c.h"

PyDoc_STRVAR(xml_parse_primary__doc__,
"xml_parse_primary(filename, newpkgcb, pkgcb, warningcb, do_files) -> None\n\n"
"Parse primary.xml");
PyDoc_STRVAR(xml_parse_primary_snippet__doc__,
"xml_parse_primary_snippet(snippet, newpkgcb, pkgcb, warningcb, do_files) -> None\n\n"
"Parse primary xml snippet");

PyObject *py_xml_parse_primary(PyObject *self, PyObject *args);
PyObject *py_xml_parse_primary_snippet(PyObject *self, PyObject *args);

PyDoc_STRVAR(xml_parse_filelists__doc__,
"xml_parse_filelists(filename, newpkgcb, pkgcb, warningcb) -> None\n\n"
"Parse filelists.xml");
PyDoc_STRVAR(xml_parse_filelists_snippet__doc__,
"xml_parse_filelists_snippet(snippet, newpkgcb, pkgcb, warningcb) -> None\n\n"
"Parse filelists xml snippet");

PyObject *py_xml_parse_filelists(PyObject *self, PyObject *args);
PyObject *py_xml_parse_filelists_snippet(PyObject *self, PyObject *args);

PyDoc_STRVAR(xml_parse_other__doc__,
"xml_parse_other(filename, newpkgcb, pkgcb, warningcb) -> None\n\n"
"Parse other.xml");
PyDoc_STRVAR(xml_parse_other_snippet__doc__,
"xml_parse_other_snippet(snippet, newpkgcb, pkgcb, warningcb) -> None\n\n"
"Parse other xml snippet");

PyObject *py_xml_parse_other(PyObject *self, PyObject *args);
PyObject *py_xml_parse_other_snippet(PyObject *self, PyObject *args);

PyDoc_STRVAR(xml_parse_repomd__doc__,
"xml_parse_repomd(filename, repomd_object, warningcb) -> None\n\n"
"Parse repomd.xml");

PyObject *py_xml_parse_repomd(PyObject *self, PyObject *args);

PyDoc_STRVAR(xml_parse_updateinfo__doc__,
"xml_parse_updateinfo(filename, updateinfo_object, warningcb) -> None\n\n"
"Parse updateinfo.xml");

PyObject *py_xml_parse_updateinfo(PyObject *self, PyObject *args);

PyDoc_STRVAR(xml_parse_main_metadata_together__doc__,
"xml_parse_main_metadata_together(primary_filename, filelists_filename, other_filename, newpkgcb, pkgcb, warningcb) -> None\n\n"
"Parse primary.xml, filelists.xml and other.xml together at the same time."
"- It can handle if packages are not in the same order in all 3 files but memory requirements grow."
"- It is not guaranteed that newpkgcb is always followed by pkgcb for the given package, it is possible newpkgcb will be called several times for different packages and only after that pkgcbs will be called.");

PyObject *py_xml_parse_main_metadata_together(PyObject *self, PyObject *args, PyObject *kwargs);

#endif
