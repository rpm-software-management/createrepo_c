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

#include "updatecollectionpackage-py.h"
#include "exception-py.h"
#include "typeconversion.h"
#include "contentstat-py.h"

typedef struct {
    PyObject_HEAD
    cr_UpdateCollectionPackage *pkg;
} _UpdateCollectionPackageObject;

PyObject *
Object_FromUpdateCollectionPackage(cr_UpdateCollectionPackage *pkg)
{
    PyObject *py_rec;

    if (!pkg) {
        PyErr_SetString(PyExc_ValueError, "Expected a cr_UpdateCollectionPackage pointer not NULL.");
        return NULL;
    }

    py_rec = PyObject_CallObject((PyObject *) &UpdateCollectionPackage_Type, NULL);
    cr_updatecollectionpackage_free(((_UpdateCollectionPackageObject *)py_rec)->pkg);
    ((_UpdateCollectionPackageObject *)py_rec)->pkg = pkg;

    return py_rec;
}

cr_UpdateCollectionPackage *
UpdateCollectionPackage_FromPyObject(PyObject *o)
{
    if (!UpdateCollectionPackageObject_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "Expected a UpdateCollectionPackage object.");
        return NULL;
    }
    return ((_UpdateCollectionPackageObject *)o)->pkg;
}

static int
check_UpdateCollectionPackageStatus(const _UpdateCollectionPackageObject *self)
{
    assert(self != NULL);
    assert(UpdateCollectionPackageObject_Check(self));
    if (self->pkg == NULL) {
        PyErr_SetString(CrErr_Exception, "Improper createrepo_c UpdateCollectionPackage object.");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
updatecollectionpackage_new(PyTypeObject *type,
                            G_GNUC_UNUSED PyObject *args,
                            G_GNUC_UNUSED PyObject *kwds)
{
    _UpdateCollectionPackageObject *self = (_UpdateCollectionPackageObject *)type->tp_alloc(type, 0);
    if (self) {
        self->pkg = NULL;
    }
    return (PyObject *)self;
}

PyDoc_STRVAR(updatecollectionpackage_init__doc__,
".. method:: __init__()\n\n");

static int
updatecollectionpackage_init(_UpdateCollectionPackageObject *self,
                             G_GNUC_UNUSED PyObject *args,
                             G_GNUC_UNUSED PyObject *kwds)
{
    /* Free all previous resources when reinitialization */
    if (self->pkg)
        cr_updatecollectionpackage_free(self->pkg);

    /* Init */
    self->pkg = cr_updatecollectionpackage_new();
    if (self->pkg == NULL) {
        PyErr_SetString(CrErr_Exception, "UpdateCollectionPackage initialization failed");
        return -1;
    }

    return 0;
}

static void
updatecollectionpackage_dealloc(_UpdateCollectionPackageObject *self)
{
    if (self->pkg)
        cr_updatecollectionpackage_free(self->pkg);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
updatecollectionpackage_repr(G_GNUC_UNUSED _UpdateCollectionPackageObject *self)
{
    return PyUnicode_FromFormat("<createrepo_c.UpdateCollectionPackage object>");
}

/* UpdateCollectionPackage methods */

PyDoc_STRVAR(copy__doc__,
"copy() -> UpdateCollectionPackage\n\n"
"Return copy of the UpdateCollectionPackage object");

static PyObject *
copy_updatecollectionpackage(_UpdateCollectionPackageObject *self,
                             G_GNUC_UNUSED void *nothing)
{
    if (check_UpdateCollectionPackageStatus(self))
        return NULL;
    return Object_FromUpdateCollectionPackage(cr_updatecollectionpackage_copy(self->pkg));
}

static struct PyMethodDef updatecollectionpackage_methods[] = {
    {"copy", (PyCFunction)copy_updatecollectionpackage, METH_NOARGS,
        copy__doc__},
    {NULL, NULL, 0, NULL} /* sentinel */
};

/* getsetters */

#define OFFSET(member) (void *) offsetof(cr_UpdateCollectionPackage, member)

static PyObject *
get_int(_UpdateCollectionPackageObject *self, void *member_offset)
{
    if (check_UpdateCollectionPackageStatus(self))
        return NULL;
    cr_UpdateCollectionPackage *pkg = self->pkg;
    gint64 val = *((int *) ((size_t)pkg + (size_t) member_offset));
    return PyLong_FromLongLong((long long) val);
}

static PyObject *
get_str(_UpdateCollectionPackageObject *self, void *member_offset)
{
    if (check_UpdateCollectionPackageStatus(self))
        return NULL;
    cr_UpdateCollectionPackage *pkg = self->pkg;
    char *str = *((char **) ((size_t) pkg + (size_t) member_offset));
    if (str == NULL)
        Py_RETURN_NONE;
    return PyUnicode_FromString(str);
}

static int
set_int(_UpdateCollectionPackageObject *self, PyObject *value, void *member_offset)
{
    long val;
    if (check_UpdateCollectionPackageStatus(self))
        return -1;
    if (PyLong_Check(value)) {
        val = PyLong_AsLong(value);
    } else if (PyFloat_Check(value)) {
        val = (long long) PyFloat_AS_DOUBLE(value);
    } else {
        PyErr_SetString(PyExc_TypeError, "Number expected!");
        return -1;
    }
    cr_UpdateCollectionPackage *pkg = self->pkg;
    *((int *) ((size_t) pkg + (size_t) member_offset)) = (int) val;
    return 0;
}

static int
set_str(_UpdateCollectionPackageObject *self, PyObject *value, void *member_offset)
{
    if (check_UpdateCollectionPackageStatus(self))
        return -1;
    if (!PyUnicode_Check(value) && !PyBytes_Check(value) && value != Py_None) {
        PyErr_SetString(PyExc_TypeError, "Unicode, bytes, or None expected!");
        return -1;
    }
    cr_UpdateCollectionPackage *pkg = self->pkg;
    char *str = PyObject_ToChunkedString(value, pkg->chunk);
    *((char **) ((size_t) pkg + (size_t) member_offset)) = str;
    return 0;
}

static PyGetSetDef updatecollectionpackage_getsetters[] = {
    {"name",                (getter)get_str, (setter)set_str,
        "Name",             OFFSET(name)},
    {"version",             (getter)get_str, (setter)set_str,
        "Version",          OFFSET(version)},
    {"release",             (getter)get_str, (setter)set_str,
        "Release",          OFFSET(release)},
    {"epoch",               (getter)get_str, (setter)set_str,
        "Epoch",            OFFSET(epoch)},
    {"arch",                (getter)get_str, (setter)set_str,
        "Architecture",     OFFSET(arch)},
    {"src",                 (getter)get_str, (setter)set_str,
        "Source filename",  OFFSET(src)},
    {"filename",            (getter)get_str, (setter)set_str,
        "Filename",         OFFSET(filename)},
    {"sum",                 (getter)get_str, (setter)set_str,
        "Checksum",         OFFSET(sum)},
    {"sum_type",            (getter)get_int, (setter)set_int,
        "Type of checksum", OFFSET(sum_type)},
    {"reboot_suggested",    (getter)get_int, (setter)set_int,
        "Suggested reboot", OFFSET(reboot_suggested)},
    {"restart_suggested",   (getter)get_int, (setter)set_int,
        "Suggested restart",OFFSET(restart_suggested)},
    {"relogin_suggested",   (getter)get_int, (setter)set_int,
        "Suggested relogin",OFFSET(relogin_suggested)},
    {NULL, NULL, NULL, NULL, NULL} /* sentinel */
};

/* Object */

PyTypeObject UpdateCollectionPackage_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "createrepo_c.UpdateCollectionPackage",
    .tp_basicsize = sizeof(_UpdateCollectionPackageObject),
    .tp_dealloc = (destructor) updatecollectionpackage_dealloc,
    .tp_repr = (reprfunc) updatecollectionpackage_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = updatecollectionpackage_init__doc__,
    .tp_iter = PyObject_SelfIter,
    .tp_methods = updatecollectionpackage_methods,
    .tp_getset = updatecollectionpackage_getsetters,
    .tp_init = (initproc) updatecollectionpackage_init,
    .tp_new = updatecollectionpackage_new,
};
