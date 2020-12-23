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

#include "contentstat-py.h"
#include "exception-py.h"
#include "typeconversion.h"

typedef struct {
    PyObject_HEAD
    cr_ContentStat *stat;
} _ContentStatObject;

cr_ContentStat *
ContentStat_FromPyObject(PyObject *o)
{
    if (!ContentStatObject_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "Expected a ContentStat object.");
        return NULL;
    }
    return ((_ContentStatObject *)o)->stat;
}

static int
check_ContentStatStatus(const _ContentStatObject *self)
{
    assert(self != NULL);
    assert(ContentStatObject_Check(self));
    if (self->stat == NULL) {
        PyErr_SetString(CrErr_Exception, "Improper createrepo_c ContentStat object.");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
contentstat_new(PyTypeObject *type,
                G_GNUC_UNUSED PyObject *args,
                G_GNUC_UNUSED PyObject *kwds)
{
    _ContentStatObject *self = (_ContentStatObject *)type->tp_alloc(type, 0);
    if (self)
        self->stat = NULL;
    return (PyObject *)self;
}

PyDoc_STRVAR(contentstat_init__doc__,
"ContentStat object representing statistical information about content\n\n"
".. method:: __init__(checksum_type)\n\n"
"    :arg checksum_type: Type of checksum that should be used\n");

static int
contentstat_init(_ContentStatObject *self,
                 PyObject *args,
                 G_GNUC_UNUSED PyObject *kwds)
{
    int type;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "i:contentstat_init", &type))
        return -1;

    /* Free all previous resources when reinitialization */
    if (self->stat)
        cr_contentstat_free(self->stat, NULL);

    /* Init */
    self->stat = cr_contentstat_new(type, &tmp_err);
    if (tmp_err) {
        nice_exception(&tmp_err, "ContentStat init failed: ");
        return -1;
    }

    return 0;
}

static void
contentstat_dealloc(_ContentStatObject *self)
{
    if (self->stat)
        cr_contentstat_free(self->stat, NULL);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
contentstat_repr(G_GNUC_UNUSED _ContentStatObject *self)
{
    return PyUnicode_FromFormat("<createrepo_c.ContentStat object>");
}

/* getsetters */

#define OFFSET(member) (void *) offsetof(cr_ContentStat, member)

static PyObject *
get_num(_ContentStatObject *self, void *member_offset)
{
    if (check_ContentStatStatus(self))
        return NULL;
    cr_ContentStat *rec = self->stat;
    gint64 val = (gint64) *((gint64 *) ((size_t)rec + (size_t) member_offset));
    return PyLong_FromLongLong((long long) val);
}

static PyObject *
get_int(_ContentStatObject *self, void *member_offset)
{
    if (check_ContentStatStatus(self))
        return NULL;
    cr_ContentStat *rec = self->stat;
    gint64 val = (gint64) *((int *) ((size_t)rec + (size_t) member_offset));
    return PyLong_FromLongLong((long long) val);
}

static PyObject *
get_str(_ContentStatObject *self, void *member_offset)
{
    if (check_ContentStatStatus(self))
        return NULL;
    cr_ContentStat *rec = self->stat;
    char *str = *((char **) ((size_t) rec + (size_t) member_offset));
    if (str == NULL)
        Py_RETURN_NONE;
    return PyUnicode_FromString(str);
}

static int
set_num(_ContentStatObject *self, PyObject *value, void *member_offset)
{
    gint64 val;
    if (check_ContentStatStatus(self))
        return -1;
    if (PyLong_Check(value)) {
        val = (gint64) PyLong_AsLong(value);
    } else if (PyFloat_Check(value)) {
        val = (gint64) PyFloat_AS_DOUBLE(value);
    } else {
        PyErr_SetString(PyExc_TypeError, "Number expected!");
        return -1;
    }
    cr_ContentStat *rec = self->stat;
    *((gint64 *) ((size_t) rec + (size_t) member_offset)) = val;
    return 0;
}

static int
set_int(_ContentStatObject *self, PyObject *value, void *member_offset)
{
    long val;
    if (check_ContentStatStatus(self))
        return -1;
    if (PyLong_Check(value)) {
        val = PyLong_AsLong(value);
    } else if (PyFloat_Check(value)) {
        val = (gint64) PyFloat_AS_DOUBLE(value);
    } else {
        PyErr_SetString(PyExc_TypeError, "Number expected!");
        return -1;
    }
    cr_ContentStat *rec = self->stat;
    *((int *) ((size_t) rec + (size_t) member_offset)) = (int) val;
    return 0;
}

static int
set_str(_ContentStatObject *self, PyObject *value, void *member_offset)
{
    if (check_ContentStatStatus(self))
        return -1;
    if (!PyUnicode_Check(value) && !PyBytes_Check(value) && value != Py_None) {
        PyErr_SetString(PyExc_TypeError, "Unicode, bytes, or None expected!");
        return -1;
    }
    cr_ContentStat *rec = self->stat;
    PyObject *pybytes = PyObject_ToPyBytesOrNull(value);
    char *str = g_strdup(PyBytes_AsString(pybytes));
    Py_XDECREF(pybytes);
    *((char **) ((size_t) rec + (size_t) member_offset)) = str;
    return 0;
}

static PyGetSetDef contentstat_getsetters[] = {
    {"size",            (getter)get_num, (setter)set_num,
        "Number of uncompressed bytes written", OFFSET(size)},
    {"checksum_type",   (getter)get_int, (setter)set_int,
        "Type of used checksum", OFFSET(checksum_type)},
    {"checksum",        (getter)get_str, (setter)set_str,
        "Calculated checksum", OFFSET(checksum)},
    {NULL, NULL, NULL, NULL, NULL} /* sentinel */
};

/* Object */

PyTypeObject ContentStat_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "createrepo_c.ContentStat",
    .tp_basicsize = sizeof(_ContentStatObject),
    .tp_dealloc = (destructor) contentstat_dealloc,
    .tp_repr = (reprfunc) contentstat_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = contentstat_init__doc__,
    .tp_iter = PyObject_SelfIter,
    .tp_getset = contentstat_getsetters,
    .tp_init = (initproc) contentstat_init,
    .tp_new = contentstat_new,
};
