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

#include "updateinfo-py.h"
#include "updaterecord-py.h"
#include "exception-py.h"
#include "typeconversion.h"

typedef struct {
    PyObject_HEAD
    cr_UpdateInfo *updateinfo;
} _UpdateInfoObject;

cr_UpdateInfo *
UpdateInfo_FromPyObject(PyObject *o)
{
    if (!UpdateInfoObject_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "Expected a createrepo_c.UpdateInfo object.");
        return NULL;
    }
    return ((_UpdateInfoObject *)o)->updateinfo;
}

    static int
check_UpdateInfoStatus(const _UpdateInfoObject *self)
{
    assert(self != NULL);
    assert(UpdateInfoObject_Check(self));
    if (self->updateinfo == NULL) {
        PyErr_SetString(CrErr_Exception, "Improper createrepo_c UpdateInfo object.");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
updateinfo_new(PyTypeObject *type,
               G_GNUC_UNUSED PyObject *args,
               G_GNUC_UNUSED PyObject *kwds)
{
    _UpdateInfoObject *self = (_UpdateInfoObject *)type->tp_alloc(type, 0);
    if (self) {
        self->updateinfo = NULL;
    }
    return (PyObject *)self;
}

PyDoc_STRVAR(updateinfo_init__doc__,
"UpdateInfo object");

static int
updateinfo_init(_UpdateInfoObject *self,
                G_GNUC_UNUSED PyObject *args,
                G_GNUC_UNUSED PyObject *kwds)
{
    /* Free all previous resources when reinitialization */
    if (self->updateinfo) {
        cr_updateinfo_free(self->updateinfo);
    }

    /* Init */
    self->updateinfo = cr_updateinfo_new();
    if (self->updateinfo == NULL) {
        PyErr_SetString(CrErr_Exception, "UpdateInfo initialization failed");
        return -1;
    }

    return 0;
}

static void
updateinfo_dealloc(_UpdateInfoObject *self)
{
    if (self->updateinfo)
        cr_updateinfo_free(self->updateinfo);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
updateinfo_repr(G_GNUC_UNUSED _UpdateInfoObject *self)
{
    return PyUnicode_FromFormat("<createrepo_c.UpdateInfo object>");
}

/* UpdateInfo methods */

PyDoc_STRVAR(append__doc__,
"append(updaterecord) -> None\n\n"
"Append UpdateRecord");

static PyObject *
append(_UpdateInfoObject *self, PyObject *args)
{
    PyObject *record;
    cr_UpdateRecord *orig, *new;

    if (!PyArg_ParseTuple(args, "O!:append", &UpdateRecord_Type, &record))
        return NULL;
    if (check_UpdateInfoStatus(self))
        return NULL;

    orig = UpdateRecord_FromPyObject(record);
    new = cr_updaterecord_copy(orig);
    cr_updateinfo_apped_record(self->updateinfo, new);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(xml_dump__doc__,
"xml_dump() -> str\n\n"
"Generate xml representation of the updateinfo");

static PyObject *
xml_dump(_UpdateInfoObject *self, G_GNUC_UNUSED void *nothing)
{
    PyObject *py_str;
    GError *tmp_err = NULL;
    char *xml = cr_xml_dump_updateinfo(self->updateinfo, &tmp_err);
    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }
    py_str = PyUnicodeOrNone_FromString(xml);
    free(xml);
    return py_str;
}

static struct PyMethodDef updateinfo_methods[] = {
    {"append", (PyCFunction)append, METH_VARARGS,
        append__doc__},
    {"xml_dump", (PyCFunction)xml_dump, METH_NOARGS,
        xml_dump__doc__},
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
PyObject_FromUpdateRecord(cr_UpdateRecord *rec)
{
    return Object_FromUpdateRecord(cr_updaterecord_copy(rec));
}

typedef struct {
    size_t offset;          /*!< Ofset of the list in cr_UpdateInfo */
    ConversionFromFunc f;   /*!< Conversion func to PyObject from a C object */
    ConversionToCheckFunc t_check; /*!< Check func for a single element of list */
    ConversionToFunc t;     /*!< Conversion func to C object from PyObject */
} ListConvertor;

/** List of convertors for converting a lists in cr_Package. */
static ListConvertor list_convertors[] = {
    { offsetof(cr_UpdateInfo, updates),
      (ConversionFromFunc) PyObject_FromUpdateRecord,
      (ConversionToCheckFunc) NULL,
      (ConversionToFunc) NULL },
};

/* Getters */

static PyObject *
get_list(_UpdateInfoObject *self, void *conv)
{
    ListConvertor *convertor = conv;
    PyObject *list;
    cr_UpdateInfo *updateinfo = self->updateinfo;
    GSList *glist = *((GSList **) ((size_t) updateinfo + (size_t) convertor->offset));

    if (check_UpdateInfoStatus(self))
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

/* Setters */

/*
static int
set_list(_UpdateInfoObject *self, PyObject *list, void *conv)
{
    ListConvertor *convertor = conv;
    cr_UpdateInfo *updateinfo = self->updateinfo;
    GSList *glist = NULL;

    if (check_UpdateInfoStatus(self))
        return -1;

    if (!PyList_Check(list)) {
        PyErr_SetString(PyExc_TypeError, "List expected!");
        return -1;
    }

    Py_ssize_t len = PyList_Size(list);

    // Check all elements
    for (Py_ssize_t x = 0; x < len; x++) {
        PyObject *elem = PyList_GetItem(list, x);
        if (convertor->t_check && convertor->t_check(elem))
            return -1;
    }

    for (Py_ssize_t x = 0; x < len; x++) {
        glist = g_slist_prepend(glist,
                        convertor->t(PyList_GetItem(list, x), updateinfo->chunk));
    }

    *((GSList **) ((size_t) updateinfo + (size_t) convertor->offset)) = glist;
    return 0;
}
*/

/** Return offset of a selected member of cr_UpdateInfo structure. */
#define OFFSET(member) (void *) offsetof(cr_UpdateInfo, member)

static PyGetSetDef updateinfo_getsetters[] = {
    {"updates",          (getter)get_list, (setter)NULL,
        "List of UpdateRecords", &(list_convertors[0])},
    {NULL, NULL, NULL, NULL, NULL} /* sentinel */
};

/* Object */


PyTypeObject UpdateInfo_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "createrepo_c.UpdateInfo",
    .tp_basicsize = sizeof(_UpdateInfoObject),
    .tp_dealloc = (destructor) updateinfo_dealloc,
    .tp_repr = (reprfunc) updateinfo_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = updateinfo_init__doc__,
    .tp_iter = PyObject_SelfIter,
    .tp_methods = updateinfo_methods,
    .tp_getset = updateinfo_getsetters,
    .tp_init = (initproc) updateinfo_init,
    .tp_new = updateinfo_new,
};
