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
py_package_from_rpm(PyObject *self, PyObject *args)
{
    CR_UNUSED(self);

    PyObject *ret;
    cr_Package *pkg;
    int checksum_type, changelog_limit;
    char *filename, *location_href, *location_base;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sizzi:py_package_from_rpm",
                                         &filename,
                                         &checksum_type,
                                         &location_href,
                                         &location_base,
                                         &changelog_limit)) {
        return NULL;
    }

    if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        PyErr_Format(PyExc_IOError, "File %s doesn't exist", filename);
        return NULL;
    }

    pkg = cr_package_from_rpm(filename, checksum_type, location_href,
                              location_base, changelog_limit, NULL, &tmp_err);
    if (tmp_err) {
        PyErr_Format(CrErr_Exception, "Cannot load %s: %s",
                     filename, tmp_err->message);
        g_clear_error(&tmp_err);
        return NULL;
    }

    ret = Object_FromPackage(pkg, 1);
    return ret;
}

PyObject *
py_xml_from_rpm(PyObject *self, PyObject *args)
{
    CR_UNUSED(self);

    PyObject *tuple;
    int checksum_type, changelog_limit;
    char *filename, *location_href, *location_base;
    struct cr_XmlStruct xml_res;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sizzi:py_xml_from_rpm",
                                         &filename,
                                         &checksum_type,
                                         &location_href,
                                         &location_base,
                                         &changelog_limit)) {
        return NULL;
    }

    if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        PyErr_Format(PyExc_IOError, "File %s doesn't exist", filename);
        return NULL;
    }


    xml_res = cr_xml_from_rpm(filename, checksum_type, location_href,
                              location_base, changelog_limit, NULL, &tmp_err);
    if (tmp_err) {
        PyErr_Format(CrErr_Exception, "Cannot load %s: %s",
                     filename, tmp_err->message);
        g_clear_error(&tmp_err);
        return NULL;
    }

    if ((tuple = PyTuple_New(3)) == NULL)
        goto py_xml_from_rpm_end; // Free xml_res and return NULL

    PyTuple_SetItem(tuple, 0, PyStringOrNone_FromString(xml_res.primary));
    PyTuple_SetItem(tuple, 1, PyStringOrNone_FromString(xml_res.filelists));
    PyTuple_SetItem(tuple, 2, PyStringOrNone_FromString(xml_res.other));

py_xml_from_rpm_end:
    free(xml_res.primary);
    free(xml_res.filelists);
    free(xml_res.other);

    return tuple;
}

