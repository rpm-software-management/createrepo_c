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

typedef struct {
    PyObject_HEAD
    PyObject *newpkgcb;
    PyObject *pkgcb;
    PyObject *warningcb;
} _XmlParserObject;

static int
check_XmlParserStatus(const _XmlParserObject *self)
{
    assert(self != NULL);
    assert(XmlParserObject_Check(self));
    return 0;
}

/* Functions on the type */

static PyObject *
xmlparser_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    CR_UNUSED(args);
    CR_UNUSED(kwds);
    _XmlParserObject *self = (_XmlParserObject *)type->tp_alloc(type, 0);
    if (self) {
        self->newpkgcb  = NULL;
        self->pkgcb     = NULL;
        self->warningcb = NULL;
    }
    return (PyObject *)self;
}

static int
xmlparser_init(_XmlParserObject *self, PyObject *args, PyObject *kwds)
{
    CR_UNUSED(args);
    CR_UNUSED(kwds);

    //if (!PyArg_ParseTuple(args, "|:xmlparser_init"))
    //    return -1;

    /* Free all previous resources when reinitialization */
    Py_XDECREF(self->newpkgcb);
    Py_XDECREF(self->pkgcb);
    Py_XDECREF(self->warningcb);

    self->newpkgcb  = NULL;
    self->pkgcb     = NULL;
    self->warningcb = NULL;

    return 0;
}

static void
xmlparser_dealloc(_XmlParserObject *self)
{
    Py_XDECREF(self->newpkgcb);
    Py_XDECREF(self->pkgcb);
    Py_XDECREF(self->warningcb);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
xmlparser_repr(_XmlParserObject *self)
{
    return PyString_FromFormat("<createrepo_c.XmlParser object>");
}

/* XmlParser methods */

static int
c_newpkgcb(cr_Package **pkg,
           const char *pkgId,
           const char *name,
           const char *arch,
           void *cbdata,
           GError **err)
{
    PyObject *arglist, *result;

    arglist = Py_BuildValue("(sss)", pkgId, name, arch);
    result = PyObject_CallObject(cbdata, arglist);
    Py_DECREF(arglist);
    // XXX Process and decref result
    return CR_CB_RET_OK;
}

static int
c_pkgcb(cr_Package *pkg, void *cbdata, GError **err)
{
    PyObject *py_pkg, *arglist, *result;

    arglist = Py_BuildValue("(O)", py_pkg);
    result = PyObject_CallObject(cbdata, arglist);
    Py_DECREF(arglist);
    // XXX Process and decref result
    return CR_CB_RET_OK;
}

static int
c_warningcb(cr_XmlParserWarningType type,
            char *msg,
            void *cbdata,
            GError **err)
{
    PyObject *arglist, *result;

    arglist = Py_BuildValue("(is)", type, msg);
    result = PyObject_CallObject(cbdata, arglist);
    Py_DECREF(arglist);
    // XXX Process and decref result
    return CR_CB_RET_OK;
}

PyObject *
py_xml_parse_primary(PyObject *self, PyObject *args)
{
    CR_UNUSED(self);

    char *filename;
    int do_files;
    PyObject *newpkgcb, *pkgcb, *warningcb;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sOOOi:py_xml_parse_primary",
                                         &filename,
                                         &newpkgcb,
                                         &pkgcb,
                                         &warningcb,
                                         &do_files)) {
        return NULL;
    }

    if (!PyCallable_Check(newpkgcb) || newpkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(pkgcb) || pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(warningcb) || warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    /*
    Py_XDECREF(self->newpkgcb);
    Py_XDECREF(self->pkgcb);
    Py_XDECREF(self->warningcb);
    */

    cr_XmlParserNewPkgCb    *ptr_c_newpkgcb  = NULL;
    cr_XmlParserPkgCb       *ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   *ptr_c_warningcb = NULL;

    if (newpkgcb != Py_None)
        ptr_c_newpkgcb = c_newpkgcb;
    if (pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (newpkgcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    /*
    self->newpkgcb  = (newpkgcb == Py_None) ? NULL : newpkgcb;
    Py_XINCREF(self->newpkgcb);
    self->pkgcb     = (pkgcb == Py_None) ? NULL : pkgcb;
    Py_XINCREF(self->pkgcb);
    self->warningcb = (warningcb == Py_None) ? NULL : warningcb;
    Py_XINCREF(self->warningcb);
    */

    cr_xml_parse_primary(filename,
                         c_newpkgcb,
                         &newpkgcb,
                         c_pkgcb,
                         &pkgcb,
                         c_warningcb,
                         &warningcb,
                         do_files,
                         &tmp_err);
    if (tmp_err) {
        PyErr_Format(CrErr_Exception, "%s", tmp_err->message);
        g_clear_error(&tmp_err);
        return NULL;
    }

    Py_RETURN_NONE;
}

static struct PyMethodDef xmlparser_methods[] = {
    {"parse_primary", (PyCFunction)py_xml_parse_primary, METH_VARARGS, NULL},
    {NULL} /* sentinel */
};

PyTypeObject XmlParser_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "_librepo.XmlParser",           /* tp_name */
    sizeof(_XmlParserObject),       /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor) xmlparser_dealloc, /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    (reprfunc) xmlparser_repr,      /* tp_repr */
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
    "XmlParser object",             /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    0,                              /* tp_iternext */
    xmlparser_methods,              /* tp_methods */
    0,                              /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    (initproc) xmlparser_init,      /* tp_init */
    0,                              /* tp_alloc */
    xmlparser_new,                  /* tp_new */
    0,                              /* tp_free */
    0,                              /* tp_is_gc */
};
