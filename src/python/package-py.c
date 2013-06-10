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

#include "package-py.h"
#include "exception-py.h"
#include "typeconversion.h"

typedef struct {
    PyObject_HEAD
    cr_Package *package;
    int free_on_destroy;
    PyObject *parent;
} _PackageObject;

cr_Package *
Package_FromPyObject(PyObject *o)
{
    if (!PackageObject_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "Expected a createrepo_c.Package object.");
        return NULL;
    }
    return ((_PackageObject *)o)->package;
}

PyObject *
Object_FromPackage(cr_Package *pkg, int free_on_destroy)
{
    PyObject *pypkg;

    if (!pkg) {
        PyErr_SetString(PyExc_TypeError, "Expected a cr_Package pointer not NULL.");
        return NULL;
    }

    pypkg = PyObject_CallObject((PyObject*)&Package_Type, NULL);
    // XXX: Remove empty package in pypkg and replace it with pkg
    cr_package_free(((_PackageObject *)pypkg)->package);
    ((_PackageObject *)pypkg)->package = pkg;
    ((_PackageObject *)pypkg)->free_on_destroy = free_on_destroy;
    ((_PackageObject *)pypkg)->parent = NULL;

    return pypkg;
}

PyObject *
Object_FromPackage_WithParent(cr_Package *pkg, int free_on_destroy, PyObject *parent)
{
    PyObject *pypkg;
    pypkg = Object_FromPackage(pkg, free_on_destroy);
    if (pypkg) {
        ((_PackageObject *)pypkg)->parent = parent;
        Py_XINCREF(parent);
    }
    return pypkg;
}

static int
check_PackageStatus(const _PackageObject *self)
{
    assert(self != NULL);
    assert(PackageObject_Check(self));
    if (self->package == NULL) {
        PyErr_SetString(CrErr_Exception, "Improper createrepo_c Package object.");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
package_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    CR_UNUSED(args);
    CR_UNUSED(kwds);
    _PackageObject *self = (_PackageObject *)type->tp_alloc(type, 0);
    if (self) {
        self->package = NULL;
        self->free_on_destroy = 1;
        self->parent = NULL;
    }
    return (PyObject *)self;
}

static int
package_init(_PackageObject *self, PyObject *args, PyObject *kwds)
{
    char *kwlist[] = {NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|:package_init", kwlist))
        return -1;

    if (self->package && self->free_on_destroy)  // reinitialization by __init__()
        cr_package_free(self->package);
    if (self->parent) {
        Py_DECREF(self->parent);
        self->parent = NULL;
    }

    self->package = cr_package_new();
    if (self->package == NULL) {
        PyErr_SetString(CrErr_Exception, "Package initialization failed");
        return -1;
    }
    return 0;
}

static void
package_dealloc(_PackageObject *self)
{
    if (self->package && self->free_on_destroy)
        cr_package_free(self->package);
    if (self->parent) {
        Py_DECREF(self->parent);
        self->parent = NULL;
    }
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
package_repr(_PackageObject *self)
{
    cr_Package *pkg = self->package;
    PyObject *repr;
    if (pkg) {
        repr = PyString_FromFormat("<createrepo_c.Package object id %s, %s>",
                                   (pkg->pkgId ? pkg->pkgId : "-"),
                                   (pkg->name  ? pkg->name  : "-"));
    } else {
       repr = PyString_FromFormat("<createrepo_c.Package object id -, ->");
    }
    return repr;
}

static PyObject *
package_str(_PackageObject *self)
{
    PyObject *ret;
    if (check_PackageStatus(self))
        return NULL;
    if (self->package) {
        char *nevra = cr_package_nvra(self->package);
        ret = PyString_FromString(nevra);
        free(nevra);
    } else {
        ret = PyString_FromString("-");
    }
    return ret;
}

/* Package methods */

static PyObject *
nvra(_PackageObject *self, void *nothing)
{
    CR_UNUSED(nothing);
    PyObject *pystr;
    if (check_PackageStatus(self))
        return NULL;
    char *nvra = cr_package_nvra(self->package);
    pystr = PyStringOrNone_FromString(nvra);
    free(nvra);
    return pystr;
}

static PyObject *
nevra(_PackageObject *self, void *nothing)
{
    CR_UNUSED(nothing);
    PyObject *pystr;
    if (check_PackageStatus(self))
        return NULL;
    char *nevra = cr_package_nevra(self->package);
    pystr = PyStringOrNone_FromString(nevra);
    free(nevra);
    return pystr;
}

static PyObject *
copy_pkg(_PackageObject *self, void *nothing)
{
    CR_UNUSED(nothing);
    if (check_PackageStatus(self))
        return NULL;
    return Object_FromPackage(cr_package_copy(self->package), 1);
}

static struct PyMethodDef package_methods[] = {
    {"nvra", (PyCFunction)nvra, METH_NOARGS, NULL},
    {"nevra", (PyCFunction)nevra, METH_NOARGS, NULL},
    {"copy", (PyCFunction)copy_pkg, METH_NOARGS, NULL},
    {NULL} /* sentinel */
};

/* Getters */

static PyObject *
get_num(_PackageObject *self, void *member_offset)
{
    if (check_PackageStatus(self))
        return NULL;
    cr_Package *pkg = self->package;
    gint64 val = *((gint64 *) ((size_t)pkg + (size_t) member_offset));
    return PyLong_FromLongLong((long long) val);
}

static PyObject *
get_str(_PackageObject *self, void *member_offset)
{
    if (check_PackageStatus(self))
        return NULL;
    cr_Package *pkg = self->package;
    char *str = *((char **) ((size_t) pkg + (size_t) member_offset));
    if (str == NULL)
        Py_RETURN_NONE;
    return PyString_FromString(str);
}

/** Return offset of a selected member of cr_Package structure. */
#define OFFSET(member) (void *) offsetof(cr_Package, member)

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

/* Pre-Declaration for check functions */
static int CheckPyDependency(PyObject *dep);
static int CheckPyPackageFile(PyObject *dep);
static int CheckPyChangelogEntry(PyObject *dep);

typedef struct {
    size_t offset;          /*!< Ofset of the list in cr_Package */
    ConversionFromFunc f;   /*!< Conversion func to PyObject from a C object */
    ConversionToCheckFunc t_check; /*!< Check func for a single element of list */
    ConversionToFunc t;     /*!< Conversion func to C object from PyObject */
} ListConvertor;

/** List of convertors for converting a lists in cr_Package. */
static ListConvertor list_convertors[] = {
    { offsetof(cr_Package, requires),   PyObject_FromDependency,
      CheckPyDependency, PyObject_ToDependency },
    { offsetof(cr_Package, provides),   PyObject_FromDependency,
      CheckPyDependency, PyObject_ToDependency },
    { offsetof(cr_Package, conflicts),  PyObject_FromDependency,
      CheckPyDependency, PyObject_ToDependency },
    { offsetof(cr_Package, obsoletes),  PyObject_FromDependency,
      CheckPyDependency, PyObject_ToDependency },
    { offsetof(cr_Package, files),      PyObject_FromPackageFile,
      CheckPyPackageFile, PyObject_ToPackageFile },
    { offsetof(cr_Package, changelogs), PyObject_FromChangelogEntry,
      CheckPyChangelogEntry, PyObject_ToChangelogEntry },
};

static PyObject *
get_list(_PackageObject *self, void *conv)
{
    ListConvertor *convertor = conv;
    PyObject *list;
    cr_Package *pkg = self->package;
    GSList *glist = *((GSList **) ((size_t) pkg + (size_t) convertor->offset));

    if (check_PackageStatus(self))
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

static int
set_num(_PackageObject *self, PyObject *value, void *member_offset)
{
    gint64 val;
    if (check_PackageStatus(self))
        return -1;
    if (PyLong_Check(value)) {
        val = (gint64) PyLong_AsLong(value);
    } else if (PyInt_Check(value)) {
        val = (gint64) PyInt_AS_LONG(value);
    } else {
        PyErr_SetString(PyExc_ValueError, "Number expected!");
        return -1;
    }
    cr_Package *pkg = self->package;
    *((gint64 *) ((size_t) pkg + (size_t) member_offset)) = val;
    return 0;
}

static int
set_str(_PackageObject *self, PyObject *value, void *member_offset)
{
    if (check_PackageStatus(self))
        return -1;
    if (!PyString_Check(value) && value != Py_None) {
        PyErr_SetString(PyExc_ValueError, "String or None expected!");
        return -1;
    }
    cr_Package *pkg = self->package;

    if (value == Py_None) {
        // If value is None exist right now (avoid possibly
        // creation of a string chunk)
        *((char **) ((size_t) pkg + (size_t) member_offset)) = NULL;
        return 0;
    }

    // Check if chunk exits
    // If it doesn't - this is package from loaded metadata and all its
    // strings are in a metadata common chunk (cr_Metadata->chunk).
    // In this case, we have to create a chunk for this package before
    // inserting a new string.
    if (!pkg->chunk)
        pkg->chunk = g_string_chunk_new(0);

    char *str = g_string_chunk_insert(pkg->chunk, PyString_AsString(value));
    *((char **) ((size_t) pkg + (size_t) member_offset)) = str;
    return 0;
}


static int
CheckPyDependency(PyObject *dep)
{
    if (!PyTuple_Check(dep) || PyTuple_Size(dep) != 6) {
        PyErr_SetString(PyExc_ValueError, "Element of list has to be a tuple with 6 items.");
        return 1;
    }
    return 0;
}

static int
CheckPyPackageFile(PyObject *dep)
{
    if (!PyTuple_Check(dep) || PyTuple_Size(dep) != 3) {
        PyErr_SetString(PyExc_ValueError, "Element of list has to be a tuple with 3 items.");
        return 1;
    }
    return 0;
}

static int
CheckPyChangelogEntry(PyObject *dep)
{
    if (!PyTuple_Check(dep) || PyTuple_Size(dep) != 3) {
        PyErr_SetString(PyExc_ValueError, "Element of list has to be a tuple with 3 items.");
        return 1;
    }
    return 0;
}

static int
set_list(_PackageObject *self, PyObject *list, void *conv)
{
    ListConvertor *convertor = conv;
    cr_Package *pkg = self->package;
    GSList *glist = NULL;

    if (check_PackageStatus(self))
        return -1;

    if (!PyList_Check(list)) {
        PyErr_SetString(PyExc_ValueError, "List expected!");
        return -1;
    }

    // Check if chunk exits
    // If it doesn't - this is package from loaded metadata and all its
    // strings are in a metadata common chunk (cr_Metadata->chunk).
    // In this case, we have to create a chunk for this package before
    // inserting a new string.
    if (!pkg->chunk)
        pkg->chunk = g_string_chunk_new(0);

    Py_ssize_t len = PyList_Size(list);

    // Check all elements
    for (Py_ssize_t x = 0; x < len; x++) {
        PyObject *elem = PyList_GetItem(list, x);
        if (convertor->t_check && convertor->t_check(elem))
            return -1;
    }

    for (Py_ssize_t x = 0; x < len; x++) {
        glist = g_slist_prepend(glist, convertor->t(PyList_GetItem(list, x), pkg->chunk));
    }

    *((GSList **) ((size_t) pkg + (size_t) convertor->offset)) = glist;
    return 0;
}

static PyGetSetDef package_getsetters[] = {
    {"pkgId",            (getter)get_str, (setter)set_str, NULL, OFFSET(pkgId)},
    {"name",             (getter)get_str, (setter)set_str, NULL, OFFSET(name)},
    {"arch",             (getter)get_str, (setter)set_str, NULL, OFFSET(arch)},
    {"version",          (getter)get_str, (setter)set_str, NULL, OFFSET(version)},
    {"epoch",            (getter)get_str, (setter)set_str, NULL, OFFSET(epoch)},
    {"release",          (getter)get_str, (setter)set_str, NULL, OFFSET(release)},
    {"summary",          (getter)get_str, (setter)set_str, NULL, OFFSET(summary)},
    {"description",      (getter)get_str, (setter)set_str, NULL, OFFSET(description)},
    {"url",              (getter)get_str, (setter)set_str, NULL, OFFSET(url)},
    {"time_file",        (getter)get_num, (setter)set_num, NULL, OFFSET(time_file)},
    {"time_build",       (getter)get_num, (setter)set_num, NULL, OFFSET(time_build)},
    {"rpm_license",      (getter)get_str, (setter)set_str, NULL, OFFSET(rpm_license)},
    {"rpm_vendor",       (getter)get_str, (setter)set_str, NULL, OFFSET(rpm_vendor)},
    {"rpm_group",        (getter)get_str, (setter)set_str, NULL, OFFSET(rpm_group)},
    {"rpm_buildhost",    (getter)get_str, (setter)set_str, NULL, OFFSET(rpm_buildhost)},
    {"rpm_sourcerpm",    (getter)get_str, (setter)set_str, NULL, OFFSET(rpm_sourcerpm)},
    {"rpm_header_start", (getter)get_num, (setter)set_num, NULL, OFFSET(rpm_header_start)},
    {"rpm_header_end",   (getter)get_num, (setter)set_num, NULL, OFFSET(rpm_header_end)},
    {"rpm_packager",     (getter)get_str, (setter)set_str, NULL, OFFSET(rpm_packager)},
    {"size_package",     (getter)get_num, (setter)set_num, NULL, OFFSET(size_package)},
    {"size_installed",   (getter)get_num, (setter)set_num, NULL, OFFSET(size_installed)},
    {"size_archive",     (getter)get_num, (setter)set_num, NULL, OFFSET(size_archive)},
    {"location_href",    (getter)get_str, (setter)set_str, NULL, OFFSET(location_href)},
    {"location_base",    (getter)get_str, (setter)set_str, NULL, OFFSET(location_base)},
    {"checksum_type",    (getter)get_str, (setter)set_str, NULL, OFFSET(checksum_type)},
    {"requires",         (getter)get_list, (setter)set_list, NULL, &(list_convertors[0])},
    {"provides",         (getter)get_list, (setter)set_list, NULL, &(list_convertors[1])},
    {"conflicts",        (getter)get_list, (setter)set_list, NULL, &(list_convertors[2])},
    {"obsoletes",        (getter)get_list, (setter)set_list, NULL, &(list_convertors[3])},
    {"files",            (getter)get_list, (setter)set_list, NULL, &(list_convertors[4])},
    {"changelogs",       (getter)get_list, (setter)set_list, NULL, &(list_convertors[5])},
    {NULL, NULL, NULL, NULL, NULL} /* sentinel */
};

/* Object */

PyTypeObject Package_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "createrepo_c.Package",         /* tp_name */
    sizeof(_PackageObject),         /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor) package_dealloc,   /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    (reprfunc) package_repr,        /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    (reprfunc)package_str,          /* tp_str */
    0,                              /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /* tp_flags */
    "Package object",               /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    0,                              /* tp_iternext */
    package_methods,                /* tp_methods */
    0,                              /* tp_members */
    package_getsetters,             /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    (initproc) package_init,        /* tp_init */
    0,                              /* tp_alloc */
    package_new,                    /* tp_new */
    0,                              /* tp_free */
    0,                              /* tp_is_gc */
};
