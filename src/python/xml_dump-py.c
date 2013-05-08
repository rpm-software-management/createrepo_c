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
#include "package-py.h"
#include "exception-py.h"

PyObject *
py_xml_dump_primary(PyObject *self, PyObject *args)
{
    PyObject *py_pkg, *py_str;
    char *xml;
    GError *err = NULL;

    CR_UNUSED(self);

    if (!PyArg_ParseTuple(args, "O!:py_xml_dump_primary", &Package_Type, &py_pkg))
        return NULL;

    xml = cr_xml_dump_primary(Package_FromPyObject(py_pkg), &err);
    if (err) {
        PyErr_Format(CrErr_Exception, "Error while dumping primary xml: %s",
                     err->message);
        g_clear_error(&err);
        return NULL;
    }

    py_str = PyStringOrNone_FromString(xml);
    free(xml);
    return py_str;
}

PyObject *
py_xml_dump_filelists(PyObject *self, PyObject *args)
{
    PyObject *py_pkg, *py_str;
    char *xml;
    GError *err = NULL;

    CR_UNUSED(self);

    if (!PyArg_ParseTuple(args, "O!:py_xml_dump_filelists", &Package_Type, &py_pkg))
        return NULL;

    xml = cr_xml_dump_filelists(Package_FromPyObject(py_pkg), &err);
    if (err) {
        PyErr_Format(CrErr_Exception, "Error while dumping filelists xml: %s",
                     err->message);
        g_clear_error(&err);
        return NULL;
    }

    py_str = PyStringOrNone_FromString(xml);
    free(xml);
    return py_str;
}

PyObject *
py_xml_dump_other(PyObject *self, PyObject *args)
{
    PyObject *py_pkg, *py_str;
    char *xml;
    GError *err = NULL;

    CR_UNUSED(self);

    if (!PyArg_ParseTuple(args, "O!:py_xml_dump_other", &Package_Type, &py_pkg))
        return NULL;

    xml = cr_xml_dump_other(Package_FromPyObject(py_pkg), &err);
    if (err) {
        PyErr_Format(CrErr_Exception, "Error while dumping other xml: %s",
                     err->message);
        g_clear_error(&err);
        return NULL;
    }

    py_str = PyStringOrNone_FromString(xml);
    free(xml);
    return py_str;
}

PyObject *
py_xml_dump(PyObject *self, PyObject *args)
{
    PyObject *py_pkg, *tuple;
    struct cr_XmlStruct xml_res;
    GError *err = NULL;

    CR_UNUSED(self);

    if (!PyArg_ParseTuple(args, "O!:py_xml_dump", &Package_Type, &py_pkg))
        return NULL;


    xml_res = cr_xml_dump(Package_FromPyObject(py_pkg), &err);
    if (err) {
        PyErr_Format(CrErr_Exception, "Error while dumping xml: %s",
                     err->message);
        g_clear_error(&err);
        return NULL;
    }

    if ((tuple = PyTuple_New(3)) == NULL) {
        free(xml_res.primary);
        free(xml_res.filelists);
        free(xml_res.other);
        return NULL;
    }

    PyTuple_SetItem(tuple, 0, PyStringOrNone_FromString(xml_res.primary));
    PyTuple_SetItem(tuple, 1, PyStringOrNone_FromString(xml_res.filelists));
    PyTuple_SetItem(tuple, 2, PyStringOrNone_FromString(xml_res.other));

    free(xml_res.primary);
    free(xml_res.filelists);
    free(xml_res.other);

    return tuple;
}
