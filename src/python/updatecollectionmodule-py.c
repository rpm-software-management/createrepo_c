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

#include "updatecollectionmodule-py.h"
#include "exception-py.h"
#include "typeconversion.h"
#include "contentstat-py.h"

typedef struct {
    PyObject_HEAD
    cr_UpdateCollectionModule *module;
} _UpdateCollectionModuleObject;

PyObject *
Object_FromUpdateCollectionModule(cr_UpdateCollectionModule *mod)
{
    PyObject *py_rec;

    if (!mod) {
        PyErr_SetString(PyExc_ValueError, "Expected a cr_UpdateCollectionModule pointer not NULL.");
        return NULL;
    }

    py_rec = PyObject_CallObject((PyObject *) &UpdateCollectionModule_Type, NULL);
    cr_updatecollectionmodule_free(((_UpdateCollectionModuleObject *)py_rec)->module);
    ((_UpdateCollectionModuleObject *)py_rec)->module = mod;

    return py_rec;
}

cr_UpdateCollectionModule *
UpdateCollectionModule_FromPyObject(PyObject *o)
{
    if (!UpdateCollectionModuleObject_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "Expected a UpdateCollectionModule object.");
        return NULL;
    }
    return ((_UpdateCollectionModuleObject *)o)->module;
}

static int
check_UpdateCollectionModuleStatus(const _UpdateCollectionModuleObject *self)
{
    assert(self != NULL);
    assert(UpdateCollectionModuleObject_Check(self));
    if (self->module == NULL) {
        PyErr_SetString(CrErr_Exception, "Improper createrepo_c UpdateCollectionModule object.");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
updatecollectionmodule_new(PyTypeObject *type,
                    G_GNUC_UNUSED PyObject *args,
                    G_GNUC_UNUSED PyObject *kwds)
{
    _UpdateCollectionModuleObject *self = (_UpdateCollectionModuleObject *)type->tp_alloc(type, 0);
    if (self) {
        self->module = NULL;
    }
    return (PyObject *)self;
}

PyDoc_STRVAR(updatecollectionmodule_init__doc__,
".. method:: __init__()\n\n");

static int
updatecollectionmodule_init(_UpdateCollectionModuleObject *self,
                     G_GNUC_UNUSED PyObject *args,
                     G_GNUC_UNUSED PyObject *kwds)
{
    /* Free all previous resources when reinitialization */
    if (self->module)
        cr_updatecollectionmodule_free(self->module);

    /* Init */
    self->module = cr_updatecollectionmodule_new();
    if (self->module == NULL) {
        PyErr_SetString(CrErr_Exception, "UpdateCollectionModule initialization failed");
        return -1;
    }

    return 0;
}

static void
updatecollectionmodule_dealloc(_UpdateCollectionModuleObject *self)
{
    if (self->module)
        cr_updatecollectionmodule_free(self->module);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
updatecollectionmodule_repr(G_GNUC_UNUSED _UpdateCollectionModuleObject *self)
{
    return PyUnicode_FromFormat("<createrepo_c.UpdateCollectionModule object>");
}

/* UpdateCollectionModule methods */

PyDoc_STRVAR(copy__doc__,
"copy() -> UpdateCollectionModule\n\n"
"Return copy of the UpdateCollectionModule object");

static PyObject *
copy_updatecollectionmodule(_UpdateCollectionModuleObject *self,
                             G_GNUC_UNUSED void *nothing)
{
    if (check_UpdateCollectionModuleStatus(self))
        return NULL;
    return Object_FromUpdateCollectionModule(cr_updatecollectionmodule_copy(self->module));
}

static struct PyMethodDef updatecollectionmodule_methods[] = {
    {"copy", (PyCFunction)copy_updatecollectionmodule, METH_NOARGS,
        copy__doc__},
    {NULL, NULL, 0, NULL} /* sentinel */
};

/* getsetters */

#define OFFSET(member) (void *) offsetof(cr_UpdateCollectionModule, member)

static PyObject *
get_str(_UpdateCollectionModuleObject *self, void *member_offset)
{
    if (check_UpdateCollectionModuleStatus(self))
        return NULL;
    cr_UpdateCollectionModule *module = self->module;
    char *str = *((char **) ((size_t) module + (size_t) member_offset));
    if (str == NULL)
        Py_RETURN_NONE;
    return PyUnicode_FromString(str);
}

static PyObject *
get_uint(_UpdateCollectionModuleObject *self, void *member_offset)
{
    if (check_UpdateCollectionModuleStatus(self))
        return NULL;
    cr_UpdateCollectionModule *module = self->module;
    guint64 val = *((guint64 *) ((size_t) module + (size_t) member_offset));
    return PyLong_FromUnsignedLongLong((guint64) val);
}

static int
set_str(_UpdateCollectionModuleObject *self, PyObject *value, void *member_offset)
{
    if (check_UpdateCollectionModuleStatus(self))
        return -1;
    if (!PyUnicode_Check(value) && !PyBytes_Check(value) && value != Py_None) {
        PyErr_SetString(PyExc_TypeError, "Unicode, bytes, or None expected!");
        return -1;
    }

    cr_UpdateCollectionModule *module = self->module;
    char *str = PyObject_ToChunkedString(value, module->chunk);
    *((char **) ((size_t) module + (size_t) member_offset)) = str;
    return 0;
}

static int
set_uint(_UpdateCollectionModuleObject *self, PyObject *value, void *member_offset)
{
    if (check_UpdateCollectionModuleStatus(self))
        return -1;
    guint64 val;

    if (PyLong_Check(value)) {
        val = PyLong_AsUnsignedLongLong(value);
    } else if (PyFloat_Check(value)) {
        val = (guint64) PyFloat_AS_DOUBLE(value);
    } else {
        PyErr_SetString(PyExc_TypeError, "Number expected!");
        return -1;
    }

    cr_UpdateCollectionModule *module = self->module;
    *((guint64 *) ((size_t) module + (size_t) member_offset)) = (guint64) val;
    return 0;
}

static PyGetSetDef updatecollectionmodule_getsetters[] = {
    {"name",               (getter)get_str, (setter)set_str,
        "Name",            OFFSET(name)},
    {"stream",             (getter)get_str, (setter)set_str,
        "Stream",          OFFSET(stream)},
    {"version",            (getter)get_uint, (setter)set_uint,
        "Version",         OFFSET(version)},
    {"context",            (getter)get_str, (setter)set_str,
        "Context",         OFFSET(context)},
    {"arch",               (getter)get_str, (setter)set_str,
        "Arch",            OFFSET(arch)},
    {NULL, NULL, NULL, NULL, NULL} /* sentinel */
};

/* Object */

PyTypeObject UpdateCollectionModule_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "createrepo_c.UpdateCollectionModule",
    .tp_basicsize = sizeof(_UpdateCollectionModuleObject),
    .tp_dealloc = (destructor) updatecollectionmodule_dealloc,
    .tp_repr = (reprfunc) updatecollectionmodule_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = updatecollectionmodule_init__doc__,
    .tp_iter = PyObject_SelfIter,
    .tp_methods = updatecollectionmodule_methods,
    .tp_getset = updatecollectionmodule_getsetters,
    .tp_init = (initproc) updatecollectionmodule_init,
    .tp_new = updatecollectionmodule_new,
};
