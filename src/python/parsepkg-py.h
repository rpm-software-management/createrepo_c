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

#ifndef CR_PARSEPKG_PY_H
#define CR_PARSEPKG_PY_H

#include "src/createrepo_c.h"

PyDoc_STRVAR(package_from_rpm__doc__,
"package_from_rpm(filename, checksum_type, location_href, "
"location_base, changelog_limit) -> Package\n\n"
"Package object from the rpm package");

PyObject *py_package_from_rpm(PyObject *self, PyObject *args);

PyDoc_STRVAR(xml_from_rpm__doc__,
"xml_from_rpm(filename, checksum_type, location_href, "
"location_base, changelog_limit) -> (str, str, str)\n\n"
"XML for the package rpm package");

PyObject *py_xml_from_rpm(PyObject *self, PyObject *args);

#endif
