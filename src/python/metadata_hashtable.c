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
#include <glib.h>
#include <assert.h>
#include <stddef.h>

#include "metadata_hashtable.h"
#include "package-py.h"
#include "exception-py.h"
#include "typeconversion.h"

/* TODO:
 * Add support for iteration
 **/

typedef struct {
    PyObject_HEAD
    PyObject *owner;
    GHashTable *ht;
} _MetadataHashtableObject;

PyObject *
Object_FromGHashtable(PyObject *owner, GHashTable *ht)
{
    PyObject *pyht;

    if (!ht) {
        PyErr_SetString(PyExc_TypeError, "Expected a hash table pointer not NULL.");
        return NULL;
    }

    Py_XINCREF(owner);
    ((_MetadataHashtableObject *) pyht)->owner = owner;
    pyht = PyObject_CallObject((PyObject*)&Package_Type, NULL);
    ((_MetadataHashtableObject *) pyht)->ht = ht;
    return pyht;
}

static int
check_MetadataHashtableStatus(const _MetadataHashtableObject *self)
{
    assert(self != NULL);
    assert(MetadataHashtableObject_Check(self));
    if (self->ht == NULL) {
        PyErr_SetString(CrErr_Exception, "Improper createrepo_c MetadataHashtable object.");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
metadata_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    CR_UNUSED(args);
    CR_UNUSED(kwds);

    _MetadataHashtableObject *self = (_MetadataHashtableObject *)type->tp_alloc(type, 0);
    if (self) {
        self->ht = NULL;
    }
    return (PyObject *)self;
}

static int
metadata_init(_MetadataHashtableObject *self, PyObject *args, PyObject *kwds)
{
    char *kwlist[] = {NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|:metadata_init", kwlist))
        return -1;

    /* Free all previous resources when reinitialization */
    if (self->ht)
        g_hash_table_destroy(self->ht);
    Py_XDECREF(self->owner);
    self->owner = NULL;
    self->ht = NULL;
    return 0;
}

static void
metadata_dealloc(_MetadataHashtableObject *self)
{
    if (self->ht)
        g_hash_table_destroy(self->ht);
    Py_XDECREF(self->owner);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
metadata_repr(_MetadataHashtableObject *self)
{
    CR_UNUSED(self);
    return PyString_FromFormat("<createrepo_c.MetadataHashtable object>");
}

/* MetadataHashtable methods */

static PyObject *
len(_MetadataHashtableObject *self, PyObject *noarg)
{
    CR_UNUSED(noarg);
    unsigned long len = 0;
    if (self->ht)
        len = (unsigned long) g_hash_table_size(self->ht);
    return PyLong_FromUnsignedLong(len);
}

/*
static PyObject *
add(_MetadataHashtableObject *self, PyObject *args)
{
    char *key;
    PyObject *py_pkg;
    cr_Package *pkg;

    if (!PyArg_ParseTuple(args, "sO!:add", &key, &Package_Type, &pkg))
        return NULL;
    if (check_MetadataHashtableStatus(self))
        return NULL;

    pkg = Package_FromPyObject(pkg);
    if (!pkg)
        Py_RETURN_NONE;

    Py_XINCREF(py_pkg);
    // XXX: Store referenced object for Py_XDECREF!!!!!
    g_hash_table_replace(self->ht, key, pkg);
    Py_RETURN_NONE;
}
*/

static PyObject *
has_key(_MetadataHashtableObject *self, PyObject *args)
{
    char *key;

    if (!PyArg_ParseTuple(args, "s:has_key", &key))
        return NULL;
    if (check_MetadataHashtableStatus(self))
        return NULL;

    if (g_hash_table_lookup(self->ht, key))
        Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static PyObject *
keys(_MetadataHashtableObject *self, PyObject *args)
{
    CR_UNUSED(args);

    GList *keys = g_hash_table_get_values(self->ht);
    PyObject *list = PyList_New(0);
    for (GList *elem = keys; elem; elem = g_list_next(elem)) {
        PyObject *py_str = PyString_FromString(elem->data);
        assert(py_str);
        if (PyList_Append(list, py_str) == -1) {
            Py_XDECREF(list);
            return NULL;
        }
        Py_DECREF(py_str);
    }
    return list;
}

static PyObject *
del(_MetadataHashtableObject *self, PyObject *args)
{
    char *key;

    if (!PyArg_ParseTuple(args, "s:del", &key))
        return NULL;
    if (check_MetadataHashtableStatus(self))
        return NULL;

    if (g_hash_table_remove(self->ht, key))
        Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static struct PyMethodDef metadata_methods[] = {
    {"len", (PyCFunction)len, METH_NOARGS, NULL},
//    {"add", (PyCFunction)add, METH_VARARGS, NULL},
    {"has_key", (PyCFunction)has_key, METH_VARARGS, NULL},
    {"keys", (PyCFunction)keys, METH_NOARGS, NULL},
    {"del", (PyCFunction)del, METH_VARARGS, NULL},
    {NULL} /* sentinel */
};

/* Object */

PyTypeObject MetadataHashtable_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "_librepo.MetadataHashtable",   /* tp_name */
    sizeof(_MetadataHashtableObject),/* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor)metadata_dealloc,   /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    (reprfunc)metadata_repr,        /* tp_repr */
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
    "MetadataHashtable object",     /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    0,                              /* tp_iternext */
    metadata_methods,               /* tp_methods */
    0,                              /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    (initproc)metadata_init,        /* tp_init */
    0,                              /* tp_alloc */
    metadata_new,                   /* tp_new */
    0,                              /* tp_free */
    0,                              /* tp_is_gc */
};
