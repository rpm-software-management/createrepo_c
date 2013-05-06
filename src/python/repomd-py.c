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

#include "repomd-py.h"
#include "repomdrecord-py.h"
#include "exception-py.h"
#include "typeconversion.h"

typedef struct {
    PyObject_HEAD
    cr_Repomd repomd;
} _RepomdObject;

static int
check_RepomdStatus(const _RepomdObject *self)
{
    assert(self != NULL);
    assert(RepomdObject_Check(self));
    if (self->repomd == NULL) {
        PyErr_SetString(CrErr_Exception, "Improper createrepo_c Repomd object.");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
repomd_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    CR_UNUSED(args);
    CR_UNUSED(kwds);

    _RepomdObject *self = (_RepomdObject *)type->tp_alloc(type, 0);
    if (self) {
        self->repomd = NULL;
    }
    return (PyObject *)self;
}

static int
repomd_init(_RepomdObject *self, PyObject *args, PyObject *kwds)
{
    CR_UNUSED(args);
    CR_UNUSED(kwds);

    /* Free all previous resources when reinitialization */
    if (self->repomd) {
        cr_repomd_free(self->repomd);
    }

    /* Init */
    self->repomd = cr_repomd_new();
    if (self->repomd == NULL) {
        PyErr_SetString(CrErr_Exception, "Repomd initialization failed");
        return -1;
    }
    return 0;
}

static void
repomd_dealloc(_RepomdObject *self)
{
    if (self->repomd)
        cr_repomd_free(self->repomd);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
repomd_repr(_RepomdObject *self)
{
    CR_UNUSED(self);
    return PyString_FromFormat("<createrepo_c.Repomd object>");
}

/* Repomd methods */

static PyObject *
set_record(_RepomdObject *self, PyObject *args)
{
    PyObject *record;
    char *type;

    if (!PyArg_ParseTuple(args, "O!s:set_record", &RepomdRecord_Type, &record, &type))
        return NULL;
    if (check_RepomdStatus(self))
        return NULL;
    cr_repomd_set_record(self->repomd, RepomdRecord_FromPyObject(record), type);
    Py_XINCREF(record);
    Py_RETURN_NONE;
}

static PyObject *
set_revision(_RepomdObject *self, PyObject *args)
{
    char *revision;
    if (!PyArg_ParseTuple(args, "s:set_revision", &revision))
        return NULL;
    if (check_RepomdStatus(self))
        return NULL;
    cr_repomd_set_revision(self->repomd, revision);
    Py_RETURN_NONE;
}

static PyObject *
add_distro_tag(_RepomdObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "tag", "cpeid", NULL };

    char *tag = NULL, *cpeid = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|z:add_distro_tag",
                                     kwlist, &tag, &cpeid))
        return NULL;
    if (check_RepomdStatus(self))
        return NULL;
    cr_repomd_add_distro_tag(self->repomd, cpeid, tag);
    Py_RETURN_NONE;
}

static PyObject *
add_repo_tag(_RepomdObject *self, PyObject *args)
{
    char *tag;
    if (!PyArg_ParseTuple(args, "s:add_repo_tag", &tag))
        return NULL;
    if (check_RepomdStatus(self))
        return NULL;
    cr_repomd_add_repo_tag(self->repomd, tag);
    Py_RETURN_NONE;
}

static PyObject *
add_content_tag(_RepomdObject *self, PyObject *args)
{
    char *tag;
    if (!PyArg_ParseTuple(args, "s:add_content_tag", &tag))
        return NULL;
    if (check_RepomdStatus(self))
        return NULL;
    cr_repomd_add_content_tag(self->repomd, tag);
    Py_RETURN_NONE;
}

static PyObject *
xml_dump(_RepomdObject *self, void *nothing)
{
    CR_UNUSED(nothing);
    PyObject *py_str;
    char *xml = cr_repomd_xml_dump(self->repomd);
    py_str = PyStringOrNone_FromString(xml);
    free(xml);
    return py_str;
}

static struct PyMethodDef repomd_methods[] = {
    {"set_record", (PyCFunction)set_record, METH_VARARGS, NULL},
    {"set_revision", (PyCFunction)set_revision, METH_VARARGS, NULL},
    {"add_distro_tag", (PyCFunction)add_distro_tag, METH_VARARGS|METH_KEYWORDS, NULL},
    {"add_repo_tag", (PyCFunction)add_repo_tag, METH_VARARGS, NULL},
    {"add_content_tag", (PyCFunction)add_content_tag, METH_VARARGS, NULL},
    {"xml_dump", (PyCFunction)xml_dump, METH_NOARGS, NULL},
    {NULL} /* sentinel */
};

PyTypeObject Repomd_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "_librepo.Repomd",              /* tp_name */
    sizeof(_RepomdObject),          /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor) repomd_dealloc,    /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    (reprfunc) repomd_repr,         /* tp_repr */
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
    "Repomd object",                /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    0,                              /* tp_iternext */
    repomd_methods,                 /* tp_methods */
    0,                              /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    (initproc) repomd_init,         /* tp_init */
    0,                              /* tp_alloc */
    repomd_new,                     /* tp_new */
    0,                              /* tp_free */
    0,                              /* tp_is_gc */
};
