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
#include "updaterecord-py.h"

PyObject *
py_xml_dump_primary(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    PyObject *py_pkg, *py_str;
    char *xml;
    GError *err = NULL;

    if (!PyArg_ParseTuple(args, "O!:py_xml_dump_primary", &Package_Type, &py_pkg))
        return NULL;

    xml = cr_xml_dump_primary(Package_FromPyObject(py_pkg), &err);
    if (err) {
        nice_exception(&err, NULL);
        free(xml);
        return NULL;
    }

    py_str = PyUnicodeOrNone_FromString(xml);
    free(xml);
    return py_str;
}

PyObject *
py_xml_dump_filelists(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    PyObject *py_pkg, *py_str;
    char *xml;
    GError *err = NULL;

    if (!PyArg_ParseTuple(args, "O!:py_xml_dump_filelists", &Package_Type, &py_pkg))
        return NULL;

    xml = cr_xml_dump_filelists(Package_FromPyObject(py_pkg), &err);
    if (err) {
        nice_exception(&err, NULL);
        free(xml);
        return NULL;
    }

    py_str = PyUnicodeOrNone_FromString(xml);
    free(xml);
    return py_str;
}

PyObject *
py_xml_dump_filelists_ext(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    PyObject *py_pkg, *py_str;
    char *xml;
    GError *err = NULL;

    if (!PyArg_ParseTuple(args, "O!:py_xml_dump_filelists_ext", &Package_Type, &py_pkg))
        return NULL;

    xml = cr_xml_dump_filelists_ext(Package_FromPyObject(py_pkg), &err);
    if (err) {
        nice_exception(&err, NULL);
        free(xml);
        return NULL;
    }

    py_str = PyUnicodeOrNone_FromString(xml);
    free(xml);
    return py_str;
}

PyObject *
py_xml_dump_other(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    PyObject *py_pkg, *py_str;
    char *xml;
    GError *err = NULL;

    if (!PyArg_ParseTuple(args, "O!:py_xml_dump_other", &Package_Type, &py_pkg))
        return NULL;

    xml = cr_xml_dump_other(Package_FromPyObject(py_pkg), &err);
    if (err) {
        nice_exception(&err, NULL);
        free(xml);
        return NULL;
    }

    py_str = PyUnicodeOrNone_FromString(xml);
    free(xml);
    return py_str;
}

PyObject *
py_xml_dump(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    PyObject *py_pkg, *tuple;
    gboolean filelists_ext = FALSE;
    int tuple_index = 0, tuple_size = 3;
    struct cr_XmlStruct xml_res;
    GError *err = NULL;

    if (!PyArg_ParseTuple(args, "O!|p:py_xml_dump", &Package_Type, &py_pkg, &filelists_ext))
        return NULL;

    if (filelists_ext) {
        xml_res = cr_xml_dump_ext(Package_FromPyObject(py_pkg), &err);
    } else {
        xml_res = cr_xml_dump(Package_FromPyObject(py_pkg), &err);
    }
    if (err) {
        nice_exception(&err, NULL);
        return NULL;
    }

    if (filelists_ext)
        tuple_size = 4;

    if ((tuple = PyTuple_New(tuple_size)) == NULL)
        goto py_xml_dump_end; // Free xml_res and return NULL

    PyTuple_SetItem(tuple, tuple_index++, PyUnicodeOrNone_FromString(xml_res.primary));
    PyTuple_SetItem(tuple, tuple_index++, PyUnicodeOrNone_FromString(xml_res.filelists));
    if (filelists_ext)
        PyTuple_SetItem(tuple, tuple_index++, PyUnicodeOrNone_FromString(xml_res.filelists_ext));
    PyTuple_SetItem(tuple, tuple_index++, PyUnicodeOrNone_FromString(xml_res.other));

py_xml_dump_end:
    free(xml_res.primary);
    free(xml_res.filelists);
    free(xml_res.filelists_ext);
    free(xml_res.other);

    return tuple;
}

PyObject *
py_xml_dump_updaterecord(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    PyObject *py_rec, *py_str;
    char *xml = NULL;
    GError *err = NULL;

    if (!PyArg_ParseTuple(args, "O!:py_xml_dump_updaterecord",
                          &UpdateRecord_Type, &py_rec))
        return NULL;

    xml = cr_xml_dump_updaterecord(UpdateRecord_FromPyObject(py_rec), &err);
    if (err) {
        nice_exception(&err, NULL);
        free(xml);
        return NULL;
    }

    py_str = PyUnicodeOrNone_FromString(xml);
    free(xml);
    return py_str;
}
