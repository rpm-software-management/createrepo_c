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
sqlite_new(PyTypeObject *type,
           G_GNUC_UNUSED PyObject *args,
           G_GNUC_UNUSED PyObject *kwds)
{
    _SqliteObject *self = (_SqliteObject *)type->tp_alloc(type, 0);
    if (self)
        self->db = NULL;
    return (PyObject *)self;
}

PyDoc_STRVAR(sqlite_init__doc__,
"Sqlite object\n\n"
".. method:: __init__(path, db_type)\n\n"
"    :arg path: Path to the database\n"
"    :arg db_type: One from DB_PRIMARY, DB_FILELISTS, DB_OTHER constans\n");

static int
sqlite_init(_SqliteObject *self, PyObject *args, G_GNUC_UNUSED PyObject *kwds)
{
    char *path;
    int db_type;
    GError *err = NULL;
    PyObject *ret;

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

    if (self->db) {
        if (self->db->type == CR_DB_PRIMARY)        type = "PrimaryDb";
        else if (self->db->type == CR_DB_FILELISTS) type = "FilelistsDb";
        else if (self->db->type == CR_DB_OTHER)     type = "OtherDb";
        else                                        type = "UnknownDb";
    } else {
        type = "Closed";
    }

    return PyUnicode_FromFormat("<createrepo_c.Sqlite %s object>", type);
}

/* Sqlite methods */

PyDoc_STRVAR(add_pkg__doc__,
"add_pkg(Package) -> None\n\n"
"Add Package to the database");

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

PyDoc_STRVAR(dbinfo_update__doc__,
"dbinfo_update(checksum) -> None\n\n"
"Set checksum of the xml file representing same data");

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

PyDoc_STRVAR(close__doc__,
"close() -> None\n\n"
"Close the sqlite database");

static PyObject *
close_db(_SqliteObject *self, G_GNUC_UNUSED void *nothing)
{
    GError *err = NULL;

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
    {"add_pkg", (PyCFunction)add_pkg, METH_VARARGS,
        add_pkg__doc__},
    {"dbinfo_update", (PyCFunction)dbinfo_update, METH_VARARGS,
        dbinfo_update__doc__},
    {"close", (PyCFunction)close_db, METH_NOARGS,
        close__doc__},
    {NULL, NULL, 0, NULL} /* sentinel */
};

PyTypeObject Sqlite_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "createrepo_c.Sqlite",
    .tp_basicsize = sizeof(_SqliteObject),
    .tp_dealloc = (destructor) sqlite_dealloc,
    .tp_repr = (reprfunc) sqlite_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = sqlite_init__doc__,
    .tp_iter = PyObject_SelfIter,
    .tp_methods = sqlite_methods,
    .tp_init = (initproc) sqlite_init,
    .tp_new = sqlite_new,
};
