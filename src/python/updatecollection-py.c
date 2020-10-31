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
#include "updatecollectionmodule-py.h"
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
    return PyUnicode_FromFormat("<createrepo_c.UpdateCollection object>");
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
    {NULL, NULL, 0, NULL} /* sentinel */
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
PyObject_FromUpdateCollectionModule(cr_UpdateCollectionModule *module)
{
    return Object_FromUpdateCollectionModule(
                        cr_updatecollectionmodule_copy(module));
}

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
    return PyUnicode_FromString(str);
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

static PyObject *
get_module(_UpdateCollectionObject *self, void *member_offset)
{
    if (check_UpdateCollectionStatus(self))
        return NULL;

    cr_UpdateCollection *collection = self->collection;

    cr_UpdateCollectionModule *module = *((cr_UpdateCollectionModule **) ((size_t) collection + (size_t) member_offset));
    if (module == NULL)
        Py_RETURN_NONE;

    PyObject *py_module = PyObject_FromUpdateCollectionModule(module);

    return py_module;
}

static int
set_str(_UpdateCollectionObject *self, PyObject *value, void *member_offset)
{
    if (check_UpdateCollectionStatus(self))
        return -1;
    if (!PyUnicode_Check(value) && !PyBytes_Check(value) && value != Py_None) {
        PyErr_SetString(PyExc_TypeError, "Unicode, bytes, or None expected!");
        return -1;
    }
    cr_UpdateCollection *rec = self->collection;
    char *str = PyObject_ToChunkedString(value, rec->chunk);
    *((char **) ((size_t) rec + (size_t) member_offset)) = str;
    return 0;
}

static int
set_module(_UpdateCollectionObject *self, PyObject *value, void *member_offset)
{
    cr_UpdateCollectionModule *orig, *new;

    if (check_UpdateCollectionStatus(self))
        return -1;
    if (!UpdateCollectionModuleObject_Check(value) && value != Py_None) {
        PyErr_SetString(PyExc_TypeError, "Module or None expected!");
        return -1;
    }
    orig = UpdateCollectionModule_FromPyObject(value);
    new = cr_updatecollectionmodule_copy(orig);

    cr_UpdateCollection *collection = self->collection;
    *((cr_UpdateCollectionModule **) ((size_t) collection + (size_t) member_offset)) = new;

    return 0;
}

static PyGetSetDef updatecollection_getsetters[] = {
    {"shortname",     (getter)get_str, (setter)set_str,
        "Short name", OFFSET(shortname)},
    {"name",          (getter)get_str, (setter)set_str,
        "Name of the collection", OFFSET(name)},
    {"module",        (getter)get_module, (setter)set_module,
        "Module information", OFFSET(module)},
    {"packages",       (getter)get_list, (setter)NULL,
        "List of packages", &(list_convertors[0])},
    {NULL, NULL, NULL, NULL, NULL} /* sentinel */
};

/* Object */

PyTypeObject UpdateCollection_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "createrepo_c.UpdateCollection",
    .tp_basicsize = sizeof(_UpdateCollectionObject),
    .tp_dealloc = (destructor) updatecollection_dealloc,
    .tp_repr = (reprfunc) updatecollection_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = updatecollection_init__doc__,
    .tp_iter = PyObject_SelfIter,
    .tp_methods = updatecollection_methods,
    .tp_getset = updatecollection_getsetters,
    .tp_init = (initproc) updatecollection_init,
    .tp_new = updatecollection_new,
};
