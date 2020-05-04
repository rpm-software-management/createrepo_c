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

#include "load_metadata-py.h"
#include "locate_metadata-py.h"
#include "package-py.h"
#include "exception-py.h"
#include "typeconversion.h"

/* TODO:
 * keys() and records() method (same method - alias only)
 **/

typedef struct {
    PyObject_HEAD
    cr_Metadata *md;
} _MetadataObject;

static int
check_MetadataStatus(const _MetadataObject *self)
{
    assert(self != NULL);
    assert(MetadataObject_Check(self));
    if (self->md == NULL) {
        PyErr_SetString(PyExc_TypeError, "Improper createrepo_c Metadata object.");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
metadata_new(PyTypeObject *type,
             G_GNUC_UNUSED PyObject *args,
             G_GNUC_UNUSED PyObject *kwds)
{
    _MetadataObject *self = (_MetadataObject *)type->tp_alloc(type, 0);
    if (self)
        self->md = NULL;
    return (PyObject *)self;
}

PyDoc_STRVAR(metadata_init__doc__,
".. method:: __init__(key=HT_KEY_DEFAULT, use_single_chunk=False, pkglist=[])\n\n"
"    :arg key: Which value should be used as a key. One of HT_KEY_* constants.\n"
"    :arg use_single_chunk: Specify if all package strings should be stored\n"
"        in metadata object instead of package iself. This save some\n"
"        space if you need to have a all packages loaded into a memory.\n"
"    :arg pkglist: Package list that specify which packages shloud be\n"
"        loaded. Use its base filename (e.g.\n"
"        \"GConf2-3.2.6-6.fc19.i686.rpm\").\n");

static int
metadata_init(_MetadataObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = { "key", "use_single_chunk", "pkglist", NULL };
    int key = CR_HT_KEY_DEFAULT;
    int use_single_chunk = 0;
    PyObject *py_pkglist = NULL;
    GSList *pkglist = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iiO!:metadata_init", kwlist,
                          &key, &use_single_chunk, &PyList_Type, &py_pkglist))
        return -1;

    /* Free all previous resources when reinitialization */
    if (self->md) {
        cr_metadata_free(self->md);
    }

    /* Init */
    pkglist = GSList_FromPyList_Str(py_pkglist);
    self->md = cr_metadata_new(key, use_single_chunk, pkglist);
    g_slist_free(pkglist);
    if (self->md == NULL) {
        PyErr_SetString(CrErr_Exception, "Metadata initialization failed");
        return -1;
    }
    return 0;
}

static void
metadata_dealloc(_MetadataObject *self)
{
    if (self->md)
        cr_metadata_free(self->md);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
metadata_repr(G_GNUC_UNUSED _MetadataObject *self)
{
    return PyUnicode_FromFormat("<createrepo_c.Metadata object>");
}

/* Getters */

static PyObject *
get_key(_MetadataObject *self, G_GNUC_UNUSED void *nothing)
{
    if (check_MetadataStatus(self))
        return NULL;
    cr_HashTableKey val = cr_metadata_key(self->md);
    return PyLong_FromLong((long) val);
}

static PyGetSetDef metadata_getsetters[] = {
    {"key", (getter)get_key, NULL, "Type of used key", NULL},
    {NULL, NULL, NULL, NULL, NULL} /* sentinel */
};

/* Metadata methods */

PyDoc_STRVAR(load_xml__doc__,
"load_xml(metadata_location_object) -> None\n\n"
"Load XML specified by MetadataLocation Object");

static PyObject *
load_xml(_MetadataObject *self, PyObject *args)
{
    PyObject *ml;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "O!:load_xml", &MetadataLocation_Type, &ml))
        return NULL;

    if (check_MetadataStatus(self))
        return NULL;

    if (cr_metadata_load_xml(self->md, MetadataLocation_FromPyObject(ml), &tmp_err) != CRE_OK) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyDoc_STRVAR(locate_and_load_xml__doc__,
"locate_and_load_xml(path) -> None"
"Load XML specified by path");

static PyObject *
locate_and_load_xml(_MetadataObject *self, PyObject *args)
{
    char *path;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "s:locate_and_load_xml", &path))
        return NULL;

    if (check_MetadataStatus(self))
        return NULL;

    cr_metadata_locate_and_load_xml(self->md, path, &tmp_err);
    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Hashtable methods */

PyDoc_STRVAR(len__doc__,
"len() -> long\n\n"
"Number of packages");

static PyObject *
ht_len(_MetadataObject *self, G_GNUC_UNUSED PyObject *noarg)
{
    unsigned long len = 0;
    if (check_MetadataStatus(self))
        return NULL;
    GHashTable *ht = cr_metadata_hashtable(self->md);
    if (ht)
        len = (unsigned long) g_hash_table_size(ht);
    return PyLong_FromUnsignedLong(len);
}

/*
static PyObject *
ht_add(_MetadataObject *self, PyObject *args)
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
    g_hash_table_replace(self->md->ht, key, pkg);
    Py_RETURN_NONE;
}
*/

PyDoc_STRVAR(has_key__doc__,
"has_key(key) -> bool\n\n"
"Test if metadata contains the key");

static PyObject *
ht_has_key(_MetadataObject *self, PyObject *args)
{
    char *key;

    if (!PyArg_ParseTuple(args, "s:has_key", &key))
        return NULL;
    if (check_MetadataStatus(self))
        return NULL;

    if (g_hash_table_lookup(cr_metadata_hashtable(self->md), key))
        Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

PyDoc_STRVAR(keys__doc__,
"keys() -> list\n\n"
"List of all keys");

static PyObject *
ht_keys(_MetadataObject *self, G_GNUC_UNUSED PyObject *args)
{
    if (check_MetadataStatus(self))
        return NULL;

    GList *keys = g_hash_table_get_keys(cr_metadata_hashtable(self->md));
    PyObject *list = PyList_New(0);

    for (GList *elem = keys; elem; elem = g_list_next(elem)) {
        PyObject *py_str = PyUnicode_FromString(elem->data);
        assert(py_str);
        if (PyList_Append(list, py_str) == -1) {
            Py_XDECREF(list);
            g_list_free(keys);
            return NULL;
        }
        Py_DECREF(py_str);
    }

    g_list_free(keys);
    return list;
}

PyDoc_STRVAR(remove__doc__,
"remove(key) -> bool\n\n"
"Remove package which has a key key from the metadata");

static PyObject *
ht_remove(_MetadataObject *self, PyObject *args)
{
    char *key;

    if (!PyArg_ParseTuple(args, "s:del", &key))
        return NULL;
    if (check_MetadataStatus(self))
        return NULL;

    if (g_hash_table_remove(cr_metadata_hashtable(self->md), key))
        Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

PyDoc_STRVAR(get__doc__,
"get(key) -> Package\n\n"
"Get Package which has a key key");

static PyObject *
ht_get(_MetadataObject *self, PyObject *args)
{
    char *key;

    if (!PyArg_ParseTuple(args, "s:get", &key))
        return NULL;
    if (check_MetadataStatus(self))
        return NULL;

    cr_Package *pkg = g_hash_table_lookup(cr_metadata_hashtable(self->md), key);
    if (!pkg)
        Py_RETURN_NONE;
    return (Object_FromPackage_WithParent(pkg, 0, (PyObject *) self));
}

PyDoc_STRVAR(metadata_dupaction__doc__,
".. method:: dupaction(dupaction)\n\n"
"    :arg dupation: What to do when we encounter already existing key.\n"
"         use constants prefixed with HT_DUPACT_. I.e. \n"
"         HT_DUPACT_KEEPFIRST, HT_DUPACT_REMOVEALL.\n");

static PyObject *
metadata_dupaction(_MetadataObject *self, PyObject *args)
{
    int dupaction;
    gboolean res = TRUE;

    if (!PyArg_ParseTuple(args, "i:dupaction", &dupaction))
        return NULL;

    res = cr_metadata_set_dupaction(self->md, dupaction);
    if (!res) {
        PyErr_SetString(CrErr_Exception, "Cannot set specified action");
        return NULL;
    }

    Py_RETURN_NONE;
}

static struct PyMethodDef metadata_methods[] = {
    {"load_xml", (PyCFunction)load_xml, METH_VARARGS,
        load_xml__doc__},
    {"locate_and_load_xml", (PyCFunction)locate_and_load_xml, METH_VARARGS,
        locate_and_load_xml__doc__},
    {"len",     (PyCFunction)ht_len, METH_NOARGS, len__doc__},
//    {"add",     (PyCFunction)ht_add, METH_VARARGS, NULL},
    {"has_key", (PyCFunction)ht_has_key, METH_VARARGS, has_key__doc__},
    {"keys",    (PyCFunction)ht_keys, METH_NOARGS, keys__doc__},
    {"remove",  (PyCFunction)ht_remove, METH_VARARGS, remove__doc__},
    {"get",     (PyCFunction)ht_get, METH_VARARGS, get__doc__},
    {"dupaction",(PyCFunction)metadata_dupaction, METH_VARARGS, metadata_dupaction__doc__},
    {NULL, NULL, 0, NULL} /* sentinel */
};

/* Object */

PyTypeObject Metadata_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "createrepo_c.Metadata",
    .tp_basicsize = sizeof(_MetadataObject),
    .tp_dealloc = (destructor)metadata_dealloc,
    .tp_repr = (reprfunc)metadata_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = metadata_init__doc__,
    .tp_iter = PyObject_SelfIter,
    .tp_methods = metadata_methods,
    .tp_getset = metadata_getsetters,
    .tp_init = (initproc)metadata_init,
    .tp_new = metadata_new,
};
