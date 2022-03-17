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
#include "parsepkg-py.h"
#include "package-py.h"
#include "exception-py.h"

PyObject *
py_package_from_rpm(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    PyObject *ret;
    cr_Package *pkg;
    int checksum_type, changelog_limit;
    char *filename, *location_href, *location_base;
    GError *tmp_err = NULL;
    cr_HeaderReadingFlags flags = CR_HDRR_NONE; // TODO - support for flags

    if (!PyArg_ParseTuple(args, "sizzi:py_package_from_rpm",
                                         &filename,
                                         &checksum_type,
                                         &location_href,
                                         &location_base,
                                         &changelog_limit)) {
        return NULL;
    }

    pkg = cr_package_from_rpm(filename, checksum_type, location_href,
                              location_base, changelog_limit, NULL,
                              flags, &tmp_err);
    if (tmp_err) {
        cr_package_free(pkg);
        nice_exception(&tmp_err, "Cannot load %s: ", filename);
        return NULL;
    }

    ret = Object_FromPackage(pkg, 1);
    return ret;
}

PyObject *
py_xml_from_rpm(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    PyObject *tuple;
    int checksum_type, changelog_limit;
    char *filename, *location_href, *location_base;
    gboolean filelists_ext = FALSE;
    int tuple_index = 0, tuple_size = 3;
    struct cr_XmlStruct xml_res;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sizzi|p:py_xml_from_rpm",
                                         &filename,
                                         &checksum_type,
                                         &location_href,
                                         &location_base,
                                         &changelog_limit,
                                         &filelists_ext)) {
        return NULL;
    }

    if (filelists_ext) {
        xml_res = cr_xml_from_rpm_ext(filename, checksum_type, location_href,
                                      location_base, changelog_limit, NULL, &tmp_err);
    } else {
        xml_res = cr_xml_from_rpm(filename, checksum_type, location_href,
                                  location_base, changelog_limit, NULL, &tmp_err);
    }
    if (tmp_err) {
        nice_exception(&tmp_err, "Cannot load %s: ", filename);
        return NULL;
    }

    if (filelists_ext)
        tuple_size = 4;

    if ((tuple = PyTuple_New(tuple_size)) == NULL)
        goto py_xml_from_rpm_end; // Free xml_res and return NULL

    PyTuple_SetItem(tuple, tuple_index++, PyUnicodeOrNone_FromString(xml_res.primary));
    PyTuple_SetItem(tuple, tuple_index++, PyUnicodeOrNone_FromString(xml_res.filelists));
    if (filelists_ext)
        PyTuple_SetItem(tuple, tuple_index++, PyUnicodeOrNone_FromString(xml_res.filelists_ext));
    PyTuple_SetItem(tuple, tuple_index++, PyUnicodeOrNone_FromString(xml_res.other));

py_xml_from_rpm_end:
    free(xml_res.primary);
    free(xml_res.filelists);
    free(xml_res.filelists_ext);
    free(xml_res.other);

    return tuple;
}

