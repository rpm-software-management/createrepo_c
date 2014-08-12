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
#include <python2.7/datetime.h>
#include <assert.h>
#include <stddef.h>
#include <time.h>

#include "updaterecord-py.h"
#include "updatereference-py.h"
#include "updatecollection-py.h"
#include "exception-py.h"
#include "typeconversion.h"
#include "contentstat-py.h"

typedef struct {
    PyObject_HEAD
    cr_UpdateRecord *record;
} _UpdateRecordObject;

PyObject *
Object_FromUpdateRecord(cr_UpdateRecord *rec)
{
    PyObject *py_rec;

    if (!rec) {
        PyErr_SetString(PyExc_ValueError, "Expected a cr_UpdateRecord pointer not NULL.");
        return NULL;
    }

    py_rec = PyObject_CallObject((PyObject *) &UpdateRecord_Type, NULL);
    cr_updaterecord_free(((_UpdateRecordObject *)py_rec)->record);
    ((_UpdateRecordObject *)py_rec)->record = rec;

    return py_rec;
}

cr_UpdateRecord *
UpdateRecord_FromPyObject(PyObject *o)
{
    if (!UpdateRecordObject_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "Expected a UpdateRecord object.");
        return NULL;
    }
    return ((_UpdateRecordObject *)o)->record;
}

static int
check_UpdateRecordStatus(const _UpdateRecordObject *self)
{
    assert(self != NULL);
    assert(UpdateRecordObject_Check(self));
    if (self->record == NULL) {
        PyErr_SetString(CrErr_Exception, "Improper createrepo_c UpdateRecord object.");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
updaterecord_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    /* Python macro to use datetime objects */
    PyDateTime_IMPORT;

    CR_UNUSED(args);
    CR_UNUSED(kwds);
    _UpdateRecordObject *self = (_UpdateRecordObject *)type->tp_alloc(type, 0);
    if (self) {
        self->record = NULL;
    }
    return (PyObject *)self;
}

PyDoc_STRVAR(updaterecord_init__doc__,
".. method:: __init__()\n\n");

static int
updaterecord_init(_UpdateRecordObject *self, PyObject *args, PyObject *kwds)
{
    CR_UNUSED(args);
    CR_UNUSED(kwds);

    /* Free all previous resources when reinitialization */
    if (self->record)
        cr_updaterecord_free(self->record);

    /* Init */
    self->record = cr_updaterecord_new();
    if (self->record == NULL) {
        PyErr_SetString(CrErr_Exception, "UpdateRecord initialization failed");
        return -1;
    }

    return 0;
}

static void
updaterecord_dealloc(_UpdateRecordObject *self)
{
    if (self->record)
        cr_updaterecord_free(self->record);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
updaterecord_repr(_UpdateRecordObject *self)
{
    CR_UNUSED(self);
    return PyString_FromFormat("<createrepo_c.UpdateRecord object>");
}

/* UpdateRecord methods */

PyDoc_STRVAR(append_reference__doc__,
"append_reference(reference) -> None\n\n"
"Append UpdateReference");

static PyObject *
append_reference(_UpdateRecordObject *self, PyObject *args)
{
    PyObject *pkg;
    cr_UpdateReference *orig, *new;

    if (!PyArg_ParseTuple(args, "O!:append_reference",
                          &UpdateReference_Type, &pkg))
        return NULL;
    if (check_UpdateRecordStatus(self))
        return NULL;

    orig = UpdateReference_FromPyObject(pkg);
    new = cr_updatereference_copy(orig);
    cr_updaterecord_append_reference(self->record, new);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(append_collection__doc__,
"append_collection(collection) -> None\n\n"
"Append UpdateCollection");

static PyObject *
append_collection(_UpdateRecordObject *self, PyObject *args)
{
    PyObject *pkg;
    cr_UpdateCollection *orig, *new;

    if (!PyArg_ParseTuple(args, "O!:append_collection",
                          &UpdateCollection_Type, &pkg))
        return NULL;
    if (check_UpdateRecordStatus(self))
        return NULL;

    orig = UpdateCollection_FromPyObject(pkg);
    new = cr_updatecollection_copy(orig);
    cr_updaterecord_append_collection(self->record, new);
    Py_RETURN_NONE;
}


PyDoc_STRVAR(copy__doc__,
"copy() -> UpdateRecord\n\n"
"Return copy of the UpdateRecord object");

static PyObject *
copy_updaterecord(_UpdateRecordObject *self, void *nothing)
{
    CR_UNUSED(nothing);
    if (check_UpdateRecordStatus(self))
        return NULL;
    return Object_FromUpdateRecord(cr_updaterecord_copy(self->record));
}

static struct PyMethodDef updaterecord_methods[] = {
    {"append_reference", (PyCFunction)append_reference, METH_VARARGS,
        append_reference__doc__},
    {"append_collection", (PyCFunction)append_collection, METH_VARARGS,
        append_collection__doc__},
    {"copy", (PyCFunction)copy_updaterecord, METH_NOARGS,
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
PyObject_FromUpdateReference(cr_UpdateReference *ref)
{
    return Object_FromUpdateReference(cr_updatereference_copy(ref));
}

PyObject *
PyObject_FromUpdateCollection(cr_UpdateCollection *col)
{
    return Object_FromUpdateCollection(cr_updatecollection_copy(col));
}

typedef struct {
    size_t offset;          /*!< Ofset of the list in cr_UpdateInfo */
    ConversionFromFunc f;   /*!< Conversion func to PyObject from a C object */
    ConversionToCheckFunc t_check; /*!< Check func for a single element of list */
    ConversionToFunc t;     /*!< Conversion func to C object from PyObject */
} ListConvertor;

/** List of convertors for converting a lists in cr_Package. */
static ListConvertor list_convertors[] = {
    { offsetof(cr_UpdateRecord, references),
      (ConversionFromFunc) PyObject_FromUpdateReference,
      (ConversionToCheckFunc) NULL,
      (ConversionToFunc) NULL },
    { offsetof(cr_UpdateRecord, collections),
      (ConversionFromFunc) PyObject_FromUpdateCollection,
      (ConversionToCheckFunc) NULL,
      (ConversionToFunc) NULL },
};

/* getsetters */

#define OFFSET(member) (void *) offsetof(cr_UpdateRecord, member)

static PyObject *
get_str(_UpdateRecordObject *self, void *member_offset)
{
    if (check_UpdateRecordStatus(self))
        return NULL;
    cr_UpdateRecord *rec = self->record;
    char *str = *((char **) ((size_t) rec + (size_t) member_offset));
    if (str == NULL)
        Py_RETURN_NONE;
    return PyString_FromString(str);
}

static PyObject *
get_datetime(_UpdateRecordObject *self, void *member_offset)
{
    if (check_UpdateRecordStatus(self))
        return NULL;
    cr_UpdateRecord *rec = self->record;
    char *str = *((char **) ((size_t) rec + (size_t) member_offset));
    if (str == NULL)
        Py_RETURN_NONE;

    struct tm *dt = malloc(sizeof(struct tm));
    char *res = strptime(str, "%Y-%m-%d %H:%M:%S", dt);
    if (res == NULL)
        PyErr_SetString(CrErr_Exception, "Invalid date");

    return PyDateTime_FromDateAndTime(dt->tm_year + 1900, dt->tm_mon + 1, dt->tm_mday,
                                      dt->tm_hour, dt->tm_min, dt->tm_sec, 0);
}

static PyObject *
get_list(_UpdateRecordObject *self, void *conv)
{
    ListConvertor *convertor = conv;
    PyObject *list;
    cr_UpdateRecord *rec = self->record;
    GSList *glist = *((GSList **) ((size_t) rec + (size_t) convertor->offset));

    if (check_UpdateRecordStatus(self))
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
set_str(_UpdateRecordObject *self, PyObject *value, void *member_offset)
{
    if (check_UpdateRecordStatus(self))
        return -1;
    if (!PyString_Check(value) && value != Py_None) {
        PyErr_SetString(PyExc_TypeError, "String or None expected!");
        return -1;
    }
    cr_UpdateRecord *rec = self->record;
    char *str = cr_safe_string_chunk_insert(rec->chunk,
                                            PyObject_ToStrOrNull(value));
    *((char **) ((size_t) rec + (size_t) member_offset)) = str;
    return 0;
}

static int
set_datetime(_UpdateRecordObject *self, PyObject *value, void *member_offset)
{
    if (check_UpdateRecordStatus(self))
        return -1;
    if (!PyDateTime_Check(value) && value != Py_None) {
        PyErr_SetString(PyExc_TypeError, "DateTime or None expected!");
        return -1;
    }
    cr_UpdateRecord *rec = self->record;

    /* Length is 20: yyyy-mm-dd HH:MM:SS */
    char *date = malloc(20 * sizeof(char));
    snprintf(date, 20, "%04d-%02d-%02d %02d:%02d:%02d",
             PyDateTime_GET_YEAR(value), PyDateTime_GET_MONTH(value),
             PyDateTime_GET_DAY(value), PyDateTime_DATE_GET_HOUR(value),
             PyDateTime_DATE_GET_MINUTE(value), PyDateTime_DATE_GET_SECOND(value));

    char *str = cr_safe_string_chunk_insert(rec->chunk, date);
    free(date);
    *((char **) ((size_t) rec + (size_t) member_offset)) = str;
    return 0;
}

static PyGetSetDef updaterecord_getsetters[] = {
    {"fromstr",                     (getter)get_str, (setter)set_str,
        "Who issued this update",   OFFSET(from)},
    {"status",                      (getter)get_str, (setter)set_str,
        "Status of the update",     OFFSET(status)},
    {"type",                        (getter)get_str, (setter)set_str,
        "Update type",              OFFSET(type)},
    {"version",                     (getter)get_str, (setter)set_str,
        "Version of update",        OFFSET(version)},
    {"id",                          (getter)get_str, (setter)set_str,
        "Update id",                OFFSET(id)},
    {"title",                       (getter)get_str, (setter)set_str,
        "Update title",             OFFSET(title)},
    {"issued_date",                 (getter)get_datetime, (setter)set_datetime,
        "Date when the update was issued", OFFSET(issued_date)},
    {"updated_date",                (getter)get_datetime, (setter)set_datetime,
        "Date when the update was updated", OFFSET(updated_date)},
    {"rights",                      (getter)get_str, (setter)set_str,
        "Copyrights",               OFFSET(rights)},
    {"release",                     (getter)get_str, (setter)set_str,
        "Update relase",            OFFSET(release)},
    {"pushcount",                   (getter)get_str, (setter)set_str,
        "Pushcount",                OFFSET(pushcount)},
    {"severity",                    (getter)get_str, (setter)set_str,
        "Severity",                 OFFSET(severity)},
    {"summary",                     (getter)get_str, (setter)set_str,
        "Short summary",            OFFSET(summary)},
    {"description",                 (getter)get_str, (setter)set_str,
        "Description",              OFFSET(description)},
    {"solution",                    (getter)get_str, (setter)set_str,
        "Solution",                 OFFSET(solution)},
    {"references",                  (getter)get_list, (setter)NULL,
        "List of UpdateReferences", &(list_convertors[0])},
    {"collections",                 (getter)get_list, (setter)NULL,
        "List of UpdateCollections", &(list_convertors[1])},
    {NULL, NULL, NULL, NULL, NULL} /* sentinel */
};

/* Object */

PyTypeObject UpdateRecord_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "createrepo_c.UpdateRecord",    /* tp_name */
    sizeof(_UpdateRecordObject),    /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor) updaterecord_dealloc, /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    (reprfunc) updaterecord_repr,   /* tp_repr */
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
    updaterecord_init__doc__,       /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    0,                              /* tp_iternext */
    updaterecord_methods,           /* tp_methods */
    0,                              /* tp_members */
    updaterecord_getsetters,        /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    (initproc) updaterecord_init,   /* tp_init */
    0,                              /* tp_alloc */
    updaterecord_new,               /* tp_new */
    0,                              /* tp_free */
    0,                              /* tp_is_gc */
};
