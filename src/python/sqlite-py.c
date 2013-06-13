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

#include "sqlite-py.h"
#include "package-py.h"
#include "exception-py.h"
#include "typeconversion.h"

typedef struct {
    PyObject_HEAD
    cr_SqliteDb *db;
} _SqliteObject;

// Forward declaration
static PyObject *close_db(_SqliteObject *self, void *nothing);


static int
check_SqliteStatus(const _SqliteObject *self)
{
    assert(self != NULL);
    assert(SqliteObject_Check(self));
    if (self->db == NULL) {
        PyErr_SetString(CrErr_Exception,
            "Improper createrepo_c Sqlite object (Already closed db?)");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
sqlite_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    CR_UNUSED(args);
    CR_UNUSED(kwds);
    _SqliteObject *self = (_SqliteObject *)type->tp_alloc(type, 0);
    if (self)
        self->db = NULL;
    return (PyObject *)self;
}

static int
sqlite_init(_SqliteObject *self, PyObject *args, PyObject *kwds)
{
    char *path;
    int db_type;
    GError *err = NULL;
    PyObject *ret;

    CR_UNUSED(kwds);

    if (!PyArg_ParseTuple(args, "si|:sqlite_init", &path, &db_type))
        return -1;

    /* Check arguments */
    if (db_type < CR_DB_PRIMARY || db_type >= CR_DB_SENTINEL) {
        PyErr_SetString(PyExc_ValueError, "Unknown db type");
        return -1;
    }

    /* Free all previous resources when reinitialization */
    ret = close_db(self, NULL);
    Py_XDECREF(ret);
    if (ret == NULL) {
        // Error encountered!
        return -1;
    }

    /* Init */
    self->db = cr_db_open(path, db_type, &err);
    if (err) {
        nice_exception(&err, NULL);
        return -1;
    }

    return 0;
}

static void
sqlite_dealloc(_SqliteObject *self)
{
    if (self->db)
        cr_db_close(self->db, NULL);

    Py_TYPE(self)->tp_free(self);
}

static PyObject *
sqlite_repr(_SqliteObject *self)
{
    char *type;
    if (self->db->type == CR_DB_PRIMARY)        type = "PrimaryDb";
    else if (self->db->type == CR_DB_FILELISTS) type = "FilelistsDb";
    else if (self->db->type == CR_DB_OTHER)     type = "OtherDb";
    else                                        type = "UnknownDb";
    return PyString_FromFormat("<createrepo_c.Sqlite %s object>", type);
}

/* getsetters */

/* TODO:
 * Export sqlite object - Maybe in future version?
 */

/* Sqlite methods */

static PyObject *
add_pkg(_SqliteObject *self, PyObject *args)
{
    PyObject *py_pkg;
    GError *err = NULL;

    if (!PyArg_ParseTuple(args, "O!:add_pkg", &Package_Type, &py_pkg))
        return NULL;

    if (check_SqliteStatus(self))
        return NULL;

    cr_db_add_pkg(self->db, Package_FromPyObject(py_pkg), &err);
    if (err) {
        nice_exception(&err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
dbinfo_update(_SqliteObject *self, PyObject *args)
{
    char *checksum;
    GError *err = NULL;

    if (!PyArg_ParseTuple(args, "s:dbinfo_update", &checksum))
        return NULL;

    if (check_SqliteStatus(self))
        return NULL;

    cr_db_dbinfo_update(self->db, checksum, &err);
    if (err) {
        nice_exception(&err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
close_db(_SqliteObject *self, void *nothing)
{
    GError *err = NULL;

    CR_UNUSED(nothing);

    if (self->db) {
        cr_db_close(self->db, &err);
        self->db = NULL;
        if (err) {
            nice_exception(&err, NULL);
            return NULL;
        }
    }

    Py_RETURN_NONE;
}

static struct PyMethodDef sqlite_methods[] = {
    {"add_pkg", (PyCFunction)add_pkg, METH_VARARGS, NULL},
    {"dbinfo_update", (PyCFunction)dbinfo_update, METH_VARARGS, NULL},
    {"close", (PyCFunction)close_db, METH_NOARGS, NULL},
    {NULL} /* sentinel */
};

PyTypeObject Sqlite_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "createrepo_c.Sqlite",          /* tp_name */
    sizeof(_SqliteObject),          /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor) sqlite_dealloc,    /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    (reprfunc) sqlite_repr,         /* tp_repr */
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
    "Sqlite object",                /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    0,                              /* tp_iternext */
    sqlite_methods,                 /* tp_methods */
    0,                              /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    (initproc) sqlite_init,         /* tp_init */
    0,                              /* tp_alloc */
    sqlite_new,                     /* tp_new */
    0,                              /* tp_free */
    0,                              /* tp_is_gc */
};
