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

#include "xml_file-py.h"
#include "package-py.h"
#include "exception-py.h"
#include "contentstat-py.h"
#include "typeconversion.h"

typedef struct {
    PyObject_HEAD
    cr_XmlFile *xmlfile;
    PyObject *py_stat;
} _XmlFileObject;

static PyObject * xmlfile_close(_XmlFileObject *self, void *nothing);

static int
check_XmlFileStatus(const _XmlFileObject *self)
{
    assert(self != NULL);
    assert(XmlFileObject_Check(self));
    if (self->xmlfile == NULL) {
        PyErr_SetString(CrErr_Exception,
            "Improper createrepo_c XmlFile object (Already closed file?).");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
xmlfile_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    CR_UNUSED(args);
    CR_UNUSED(kwds);
    _XmlFileObject *self = (_XmlFileObject *)type->tp_alloc(type, 0);
    if (self) {
        self->xmlfile = NULL;
        self->py_stat = NULL;
    }
    return (PyObject *)self;
}

static int
xmlfile_init(_XmlFileObject *self, PyObject *args, PyObject *kwds)
{
    char *path;
    int type, comtype;
    GError *err = NULL;
    PyObject *py_stat, *ret;
    cr_ContentStat *stat;

    CR_UNUSED(kwds);

    if (!PyArg_ParseTuple(args, "siiO|:xmlfile_init",
                          &path, &type, &comtype, &py_stat))
        return -1;

    /* Check arguments */
    if (type < 0 || type >= CR_XMLFILE_SENTINEL) {
        PyErr_SetString(PyExc_ValueError, "Unknown XML file type");
        return -1;
    }

    if (comtype < 0 || comtype >= CR_CW_COMPRESSION_SENTINEL) {
        PyErr_SetString(PyExc_ValueError, "Unknown compression type");
        return -1;
    }

    if (py_stat == Py_None) {
        stat = NULL;
    } else if (ContentStatObject_Check(py_stat)) {
        stat = ContentStat_FromPyObject(py_stat);
    } else {
        PyErr_SetString(PyExc_ValueError, "Use ContentStat or None");
        return -1;
    }

    /* Free all previous resources when reinitialization */
    ret = xmlfile_close(self, NULL);
    Py_XDECREF(ret);
    Py_XDECREF(self->py_stat);
    self->py_stat = NULL;
    if (ret == NULL) {
        // Error encountered!
        return -1;
    }

    /* Init */
    self->xmlfile = cr_xmlfile_sopen(path, type, comtype, stat, &err);
    if (err) {
        PyErr_Format(CrErr_Exception, "XmlFile initialization failed: %s", err->message);
        g_clear_error(&err);
        return -1;
    }

    self->py_stat = py_stat;
    Py_XINCREF(py_stat);

    return 0;
}

static void
xmlfile_dealloc(_XmlFileObject *self)
{
    cr_xmlfile_close(self->xmlfile, NULL);
    Py_XDECREF(self->py_stat);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
xmlfile_repr(_XmlFileObject *self)
{
    char *type;

    switch (self->xmlfile->type) {
        case CR_XMLFILE_PRIMARY:
            type = "Primary";
            break;
        case CR_XMLFILE_FILELISTS:
            type = "Filelists";
            break;
        case CR_XMLFILE_OTHER:
            type = "Other";
            break;
        default:
            type = "Unknown";
    }

    return PyString_FromFormat("<createrepo_c.XmlFile %s object>", type);
}

/* XmlFile methods */

static PyObject *
set_num_of_pkgs(_XmlFileObject *self, PyObject *args)
{
    long num;
    GError *err = NULL;

    if (!PyArg_ParseTuple(args, "l:set_num_of_pkgs", &num))
        return NULL;

    if (check_XmlFileStatus(self))
        return NULL;

    cr_xmlfile_set_num_of_pkgs(self->xmlfile, num, &err);
    if (err) {
        PyErr_Format(CrErr_Exception, "XmlFile set_num_of_pkgs error: %s", err->message);
        g_clear_error(&err);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
add_pkg(_XmlFileObject *self, PyObject *args)
{
    PyObject *py_pkg;
    GError *err = NULL;

    if (!PyArg_ParseTuple(args, "O!:add_pkg", &Package_Type, &py_pkg))
        return NULL;

    if (check_XmlFileStatus(self))
        return NULL;

    cr_xmlfile_add_pkg(self->xmlfile, Package_FromPyObject(py_pkg), &err);
    if (err) {
        PyErr_Format(CrErr_Exception, "Cannot add package: %s", err->message);
        g_clear_error(&err);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
add_chunk(_XmlFileObject *self, PyObject *args)
{
    char *chunk;
    GError *err = NULL;

    if (!PyArg_ParseTuple(args, "s:add_chunk", &chunk))
        return NULL;

    if (check_XmlFileStatus(self))
        return NULL;

    cr_xmlfile_add_chunk(self->xmlfile, chunk, &err);
    if (err) {
        PyErr_Format(CrErr_Exception, "Cannot add chunk: %s", err->message);
        g_clear_error(&err);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
xmlfile_close(_XmlFileObject *self, void *nothing)
{
    GError *err = NULL;

    CR_UNUSED(nothing);

    if (self->xmlfile) {
        cr_xmlfile_close(self->xmlfile, &err);
        self->xmlfile = NULL;
    }

    Py_XDECREF(self->py_stat);
    self->py_stat = NULL;

    if (err) {
        PyErr_Format(CrErr_Exception, "Error while closing: %s", err->message);
        g_clear_error(&err);
        return NULL;
    }

    Py_RETURN_NONE;
}

static struct PyMethodDef xmlfile_methods[] = {
    {"set_num_of_pkgs", (PyCFunction)set_num_of_pkgs, METH_VARARGS, NULL},
    {"add_pkg", (PyCFunction)add_pkg, METH_VARARGS, NULL},
    {"add_chunk", (PyCFunction)add_chunk, METH_VARARGS, NULL},
    {"close", (PyCFunction)xmlfile_close, METH_NOARGS, NULL},
    {NULL} /* sentinel */
};

PyTypeObject XmlFile_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "createrepo_c.XmlFile",         /* tp_name */
    sizeof(_XmlFileObject),         /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor) xmlfile_dealloc,   /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    (reprfunc) xmlfile_repr,        /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
    0,                              /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /* tp_flags */
    "XmlFile object",               /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    0,                              /* tp_iternext */
    xmlfile_methods,                /* tp_methods */
    0,                              /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    (initproc) xmlfile_init,        /* tp_init */
    0,                              /* tp_alloc */
    xmlfile_new,                    /* tp_new */
    0,                              /* tp_free */
    0,                              /* tp_is_gc */
};
