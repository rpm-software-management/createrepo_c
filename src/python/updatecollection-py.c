/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2014  Tomas Mlcoch
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

#include "updatecollection-py.h"
#include "updatecollectionpackage-py.h"
#include "exception-py.h"
#include "typeconversion.h"
#include "contentstat-py.h"

typedef struct {
    PyObject_HEAD
    cr_UpdateCollection *collection;
} _UpdateCollectionObject;

PyObject *
Object_FromUpdateCollection(cr_UpdateCollection *rec)
{
    PyObject *py_rec;

    if (!rec) {
        PyErr_SetString(PyExc_ValueError, "Expected a cr_UpdateCollection pointer not NULL.");
        return NULL;
    }

    py_rec = PyObject_CallObject((PyObject *) &UpdateCollection_Type, NULL);
    cr_updatecollection_free(((_UpdateCollectionObject *)py_rec)->collection);
    ((_UpdateCollectionObject *)py_rec)->collection = rec;

    return py_rec;
}

cr_UpdateCollection *
UpdateCollection_FromPyObject(PyObject *o)
{
    if (!UpdateCollectionObject_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "Expected a UpdateCollection object.");
        return NULL;
    }
    return ((_UpdateCollectionObject *)o)->collection;
}

static int
check_UpdateCollectionStatus(const _UpdateCollectionObject *self)
{
    assert(self != NULL);
    assert(UpdateCollectionObject_Check(self));
    if (self->collection == NULL) {
        PyErr_SetString(CrErr_Exception, "Improper createrepo_c UpdateCollection object.");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
updatecollection_new(PyTypeObject *type,
                     G_GNUC_UNUSED PyObject *args,
                     G_GNUC_UNUSED PyObject *kwds)
{
    _UpdateCollectionObject *self = (_UpdateCollectionObject *)type->tp_alloc(type, 0);
    if (self) {
        self->collection = NULL;
    }
    return (PyObject *)self;
}

PyDoc_STRVAR(updatecollection_init__doc__,
".. method:: __init__()\n\n");

static int
updatecollection_init(_UpdateCollectionObject *self,
                      G_GNUC_UNUSED PyObject *args,
                      G_GNUC_UNUSED PyObject *kwds)
{
    /* Free all previous resources when reinitialization */
    if (self->collection)
        cr_updatecollection_free(self->collection);

    /* Init */
    self->collection = cr_updatecollection_new();
    if (self->collection == NULL) {
        PyErr_SetString(CrErr_Exception, "UpdateCollection initialization failed");
        return -1;
    }

    return 0;
}

static void
updatecollection_dealloc(_UpdateCollectionObject *self)
{
    if (self->collection)
        cr_updatecollection_free(self->collection);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
updatecollection_repr(G_GNUC_UNUSED _UpdateCollectionObject *self)
{
    return PyBytes_FromFormat("<createrepo_c.UpdateCollection object>");
}

/* UpdateCollection methods */

PyDoc_STRVAR(append__doc__,
"append(updatecollectionpackage) -> None\n\n"
"Add UpdateCollectionPackage");

static PyObject *
append(_UpdateCollectionObject *self, PyObject *args)
{
    PyObject *pkg;
    cr_UpdateCollectionPackage *orig, *new;

    if (!PyArg_ParseTuple(args, "O!:append", &UpdateCollectionPackage_Type,
                          &pkg))
        return NULL;
    if (check_UpdateCollectionStatus(self))
        return NULL;

    orig = UpdateCollectionPackage_FromPyObject(pkg);
    new = cr_updatecollectionpackage_copy(orig);
    cr_updatecollection_append_package(self->collection, new);
    Py_RETURN_NONE;
}


PyDoc_STRVAR(copy__doc__,
"copy() -> UpdateCollection\n\n"
"Return copy of the UpdateCollection object");

static PyObject *
copy_updatecollection(_UpdateCollectionObject *self,
                      G_GNUC_UNUSED void *nothing)
{
    if (check_UpdateCollectionStatus(self))
        return NULL;
    return Object_FromUpdateCollection(cr_updatecollection_copy(self->collection));
}

static struct PyMethodDef updatecollection_methods[] = {
    {"append", (PyCFunction)append, METH_VARARGS,
        append__doc__},
    {"copy", (PyCFunction)copy_updatecollection, METH_NOARGS,
        copy__doc__},
    {NULL} /* sentinel */
};

/* Convertors for getsetters */

/** Convert C object to PyObject.
 * @param       C object
 * @return      PyObject representation
 */
typedef PyObject *(*ConversionFromFunc)(void *);

/** Check an element from a list if has a valid format.
 * @param       a single list element
 * @return      0 if ok, 1 otherwise
 */
typedef int (*ConversionToCheckFunc)(PyObject *);

/** Convert PyObject to C representation.
 * @param       PyObject
 * @return      C representation
 */
typedef void *(*ConversionToFunc)(PyObject *, GStringChunk *);

PyObject *
PyObject_FromUpdateCollectionPackage(cr_UpdateCollectionPackage *pkg)
{
    return Object_FromUpdateCollectionPackage(
                        cr_updatecollectionpackage_copy(pkg));
}

typedef struct {
    size_t offset;          /*!< Ofset of the list in cr_UpdateInfo */
    ConversionFromFunc f;   /*!< Conversion func to PyObject from a C object */
    ConversionToCheckFunc t_check; /*!< Check func for a single element of list */
    ConversionToFunc t;     /*!< Conversion func to C object from PyObject */
} ListConvertor;

/** List of convertors for converting a lists in cr_Package. */
static ListConvertor list_convertors[] = {
    { offsetof(cr_UpdateCollection, packages),
      (ConversionFromFunc) PyObject_FromUpdateCollectionPackage,
      (ConversionToCheckFunc) NULL,
      (ConversionToFunc) NULL },
};

/* getsetters */

#define OFFSET(member) (void *) offsetof(cr_UpdateCollection, member)

static PyObject *
get_str(_UpdateCollectionObject *self, void *member_offset)
{
    if (check_UpdateCollectionStatus(self))
        return NULL;
    cr_UpdateCollection *rec = self->collection;
    char *str = *((char **) ((size_t) rec + (size_t) member_offset));
    if (str == NULL)
        Py_RETURN_NONE;
    return PyBytes_FromString(str);
}

static PyObject *
get_list(_UpdateCollectionObject *self, void *conv)
{
    ListConvertor *convertor = conv;
    PyObject *list;
    cr_UpdateCollection *collection = self->collection;
    GSList *glist = *((GSList **) ((size_t) collection + (size_t) convertor->offset));

    if (check_UpdateCollectionStatus(self))
        return NULL;

    if ((list = PyList_New(0)) == NULL)
        return NULL;

    for (GSList *elem = glist; elem; elem = g_slist_next(elem)) {
        PyObject *obj = convertor->f(elem->data);
        if (!obj) continue;
        PyList_Append(list, obj);
        Py_DECREF(obj);
    }

    return list;
}

static int
set_str(_UpdateCollectionObject *self, PyObject *value, void *member_offset)
{
    if (check_UpdateCollectionStatus(self))
        return -1;
    if (!PyBytes_Check(value) && value != Py_None) {
        PyErr_SetString(PyExc_TypeError, "String or None expected!");
        return -1;
    }
    cr_UpdateCollection *rec = self->collection;
    char *str = cr_safe_string_chunk_insert(rec->chunk,
                                            PyObject_ToStrOrNull(value));
    *((char **) ((size_t) rec + (size_t) member_offset)) = str;
    return 0;
}

static PyGetSetDef updatecollection_getsetters[] = {
    {"shortname",     (getter)get_str, (setter)set_str,
        "Short name", OFFSET(shortname)},
    {"name",          (getter)get_str, (setter)set_str,
        "Name of the collection", OFFSET(name)},
    {"packages",       (getter)get_list, (setter)NULL,
        "List of packages", &(list_convertors[0])},
    {NULL, NULL, NULL, NULL, NULL} /* sentinel */
};

/* Object */

PyTypeObject UpdateCollection_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "createrepo_c.UpdateCollection",/* tp_name */
    sizeof(_UpdateCollectionObject),/* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor) updatecollection_dealloc, /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    (reprfunc) updatecollection_repr,/* tp_repr */
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
    updatecollection_init__doc__,   /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    0,                              /* tp_iternext */
    updatecollection_methods,       /* tp_methods */
    0,                              /* tp_members */
    updatecollection_getsetters,    /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    (initproc) updatecollection_init,/* tp_init */
    0,                              /* tp_alloc */
    updatecollection_new,           /* tp_new */
    0,                              /* tp_free */
    0,                              /* tp_is_gc */
};
