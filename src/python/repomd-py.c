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
    cr_Repomd *repomd;
} _RepomdObject;

cr_Repomd *
Repomd_FromPyObject(PyObject *o)
{
    if (!RepomdObject_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "Expected a createrepo_c.Repomd object.");
        return NULL;
    }
    return ((_RepomdObject *)o)->repomd;
}

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
    cr_RepomdRecord *orig, *new;

    if (!PyArg_ParseTuple(args, "O!:set_record", &RepomdRecord_Type, &record))
        return NULL;
    if (check_RepomdStatus(self))
        return NULL;

    orig = RepomdRecord_FromPyObject(record);
    new = cr_repomd_record_copy(orig);
    cr_repomd_set_record(self->repomd, new);
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

static int
CheckPyString(PyObject *dep)
{
    if (!PyString_Check(dep)) {
        PyErr_SetString(PyExc_ValueError, "Element of list has to be a string");
        return 1;
    }
    return 0;
}

static int
CheckPyDistroTag(PyObject *dep)
{
    if (!PyTuple_Check(dep) || PyTuple_Size(dep) != 2) {
        PyErr_SetString(PyExc_ValueError, "Element of list has to be a tuple with 2 items.");
        return 1;
    }
    return 0;
}

PyObject *
PyObject_FromRepomdRecord(cr_RepomdRecord *rec)
{
    return Object_FromRepomdRecord(cr_repomd_record_copy(rec));
}

typedef struct {
    size_t offset;          /*!< Ofset of the list in cr_Repomd */
    ConversionFromFunc f;   /*!< Conversion func to PyObject from a C object */
    ConversionToCheckFunc t_check; /*!< Check func for a single element of list */
    ConversionToFunc t;     /*!< Conversion func to C object from PyObject */
} ListConvertor;

/** List of convertors for converting a lists in cr_Package. */
static ListConvertor list_convertors[] = {
    { offsetof(cr_Repomd, repo_tags),    PyStringOrNone_FromString,
      CheckPyString, PyObject_ToChunkedString },
    { offsetof(cr_Repomd, distro_tags),  PyObject_FromDistroTag,
      CheckPyDistroTag, PyObject_ToDistroTag },
    { offsetof(cr_Repomd, content_tags), PyStringOrNone_FromString,
      CheckPyString, PyObject_ToChunkedString },
    { offsetof(cr_Repomd, records),      PyObject_FromRepomdRecord,
      NULL, NULL },
};

/* Getters */

static PyObject *
get_str(_RepomdObject *self, void *member_offset)
{
    if (check_RepomdStatus(self))
        return NULL;
    cr_Repomd *repomd = self->repomd;
    char *str = *((char **) ((size_t) repomd + (size_t) member_offset));
    if (str == NULL)
        Py_RETURN_NONE;
    return PyString_FromString(str);
}

static PyObject *
get_list(_RepomdObject *self, void *conv)
{
    ListConvertor *convertor = conv;
    PyObject *list;
    cr_Repomd *repomd = self->repomd;
    GSList *glist = *((GSList **) ((size_t) repomd + (size_t) convertor->offset));

    if (check_RepomdStatus(self))
        return NULL;

    if ((list = PyList_New(0)) == NULL)
        return NULL;

    for (GSList *elem = glist; elem; elem = g_slist_next(elem))
        PyList_Append(list, convertor->f(elem->data));

    return list;
}

/* Setters */

static int
set_str(_RepomdObject *self, PyObject *value, void *member_offset)
{
    if (check_RepomdStatus(self))
        return -1;
    if (!PyString_Check(value)) {
        PyErr_SetString(PyExc_ValueError, "String expected!");
        return -1;
    }
    cr_Repomd *repomd = self->repomd;

    char *str = g_string_chunk_insert(repomd->chunk, PyString_AsString(value));
    *((char **) ((size_t) repomd + (size_t) member_offset)) = str;
    return 0;
}

static int
set_list(_RepomdObject *self, PyObject *list, void *conv)
{
    ListConvertor *convertor = conv;
    cr_Repomd *repomd = self->repomd;
    GSList *glist = NULL;

    if (check_RepomdStatus(self))
        return -1;

    if (!PyList_Check(list)) {
        PyErr_SetString(PyExc_ValueError, "List expected!");
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
                        convertor->t(PyList_GetItem(list, x), repomd->chunk));
    }

    *((GSList **) ((size_t) repomd + (size_t) convertor->offset)) = glist;
    return 0;
}

/** Return offset of a selected member of cr_Repomd structure. */
#define OFFSET(member) (void *) offsetof(cr_Repomd, member)

static PyGetSetDef repomd_getsetters[] = {
    {"revision",         (getter)get_str,  (setter)set_str,  NULL, OFFSET(revision)},
    {"repo_tags",        (getter)get_list, (setter)set_list, NULL, &(list_convertors[0])},
    {"distro_tags",      (getter)get_list, (setter)set_list, NULL, &(list_convertors[1])},
    {"content_tags",     (getter)get_list, (setter)set_list, NULL, &(list_convertors[2])},
    {"records",          (getter)get_list, (setter)NULL,     NULL, &(list_convertors[3])},
    {NULL, NULL, NULL, NULL, NULL} /* sentinel */
};

/* Object */


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
    repomd_getsetters,              /* tp_getset */
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
