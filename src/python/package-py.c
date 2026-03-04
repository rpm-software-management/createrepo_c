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
        PyErr_SetString(PyExc_ValueError, "Expected a cr_Package pointer not NULL.");
        return NULL;
    }

    pypkg = PyObject_CallObject((PyObject*)&Package_Type, NULL);
    if (!pypkg)
        return NULL;
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
package_new(PyTypeObject *type,
            G_GNUC_UNUSED PyObject *args,
            G_GNUC_UNUSED PyObject *kwds)
{
    _PackageObject *self = (_PackageObject *)type->tp_alloc(type, 0);
    if (self) {
        self->package = NULL;
        self->free_on_destroy = 1;
        self->parent = NULL;
    }
    return (PyObject *)self;
}

PyDoc_STRVAR(package_init__doc__,
"Package object\n\n"
".. method:: __init__()\n\n"
"    Default constructor\n");

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
    freefunc free_func = PyType_GetSlot(Py_TYPE(self), Py_tp_free);
    free_func(self);
}

static PyObject *
package_repr(_PackageObject *self)
{
    cr_Package *pkg = self->package;
    PyObject *repr;
    if (pkg) {
        const char *pkg_id = cr_package_get_pkg_id(pkg);
        const char *name = cr_package_get_name(pkg);
        repr = PyUnicode_FromFormat("<createrepo_c.Package object id %s, %s>",
                                   (pkg_id ? pkg_id : "-"),
                                   (name ? name : "-"));
    } else {
       repr = PyUnicode_FromFormat("<createrepo_c.Package object id -, ->");
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
        gchar *nevra = cr_package_nvra(self->package);
        ret = PyUnicode_FromString(nevra);
        g_free(nevra);
    } else {
        ret = PyUnicode_FromString("-");
    }
    return ret;
}

/* Package methods */

PyDoc_STRVAR(nvra__doc__,
"nvra() -> str\n\n"
"Package NVRA string (Name-Version-Release-Architecture)");

static PyObject *
nvra(_PackageObject *self, G_GNUC_UNUSED void *nothing)
{
    PyObject *pystr;
    if (check_PackageStatus(self))
        return NULL;
    gchar *nvra = cr_package_nvra(self->package);
    pystr = PyUnicodeOrNone_FromString(nvra);
    g_free(nvra);
    return pystr;
}

PyDoc_STRVAR(nevra__doc__,
"nevra() -> str\n\n"
"Package NEVRA string (Name-Epoch-Version-Release-Architecture)");

static PyObject *
nevra(_PackageObject *self, G_GNUC_UNUSED void *nothing)
{
    PyObject *pystr;
    if (check_PackageStatus(self))
        return NULL;
    gchar *nevra = cr_package_nevra(self->package);
    pystr = PyUnicodeOrNone_FromString(nevra);
    g_free(nevra);
    return pystr;
}

PyDoc_STRVAR(copy__doc__,
"copy() -> Package\n\n"
"Copy of the package object");

static PyObject *
copy_pkg(_PackageObject *self, G_GNUC_UNUSED void *nothing)
{
    if (check_PackageStatus(self))
        return NULL;
    return Object_FromPackage(cr_package_copy(self->package), 1);
}

static PyObject *
deepcopy_pkg(_PackageObject *self, PyObject *args)
{
    PyObject *obj;

    if (!PyArg_ParseTuple(args, "O:deepcopy_pkg", &obj))
        return NULL;
    if (check_PackageStatus(self))
        return NULL;
    return Object_FromPackage(cr_package_copy(self->package), 1);
}

static struct PyMethodDef package_methods[] = {
    {"nvra", (PyCFunction)nvra, METH_NOARGS, nvra__doc__},
    {"nevra", (PyCFunction)nevra, METH_NOARGS, nevra__doc__},
    {"copy", (PyCFunction)copy_pkg, METH_NOARGS, copy__doc__},
    {"__copy__", (PyCFunction)copy_pkg, METH_NOARGS, copy__doc__},
    {"__deepcopy__", (PyCFunction)deepcopy_pkg, METH_VARARGS, copy__doc__},
    {NULL, NULL, 0, NULL} /* sentinel */
};

/* Property accessors */

typedef const char *(*PkgStrGetter)(cr_Package *);
typedef void (*PkgStrSetter)(cr_Package *, const char *);
typedef gint64 (*PkgNumGetter)(cr_Package *);
typedef void (*PkgNumSetter)(cr_Package *, gint64);
typedef GSList *(*PkgListGetter)(cr_Package *);
typedef void (*PkgListSetter)(cr_Package *, GSList *);

typedef struct {
    PkgStrGetter get;
    PkgStrSetter set;
} StrField;

typedef struct {
    PkgNumGetter get;
    PkgNumSetter set;
} NumField;

/* Getters */

static PyObject *
get_num(_PackageObject *self, void *closure)
{
    if (check_PackageStatus(self))
        return NULL;
    cr_Package *pkg = self->package;
    return PyLong_FromLongLong((long long) ((NumField *) closure)->get(pkg));
}

static PyObject *
get_str(_PackageObject *self, void *closure)
{
    if (check_PackageStatus(self))
        return NULL;
    cr_Package *pkg = self->package;
    const char *str = ((StrField *) closure)->get(pkg);
    if (str == NULL)
        Py_RETURN_NONE;
    return PyUnicode_FromString(str);
}

static PyObject *
get_signatures(_PackageObject *self, G_GNUC_UNUSED void *unused)
{
    if (check_PackageStatus(self))
        return NULL;

    PyObject *list = PyList_New(0);
    if (!list)
        return NULL;

    for (GSList *elem = cr_package_get_signatures(self->package); elem; elem = g_slist_next(elem))
    {
        PyObject *str = PyUnicode_FromString((char *)elem->data);
        if (!str)
        {
            Py_DECREF(list);
            return NULL;
        }
        PyList_Append(list, str);
        Py_DECREF(str);
    }

    return list;
}

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
    PkgListGetter get;      /*!< Package list getter */
    PkgListSetter set;      /*!< Package list setter */
    ConversionFromFunc f;   /*!< Conversion func to PyObject from a C object */
    ConversionToCheckFunc t_check; /*!< Check func for a single element of list */
    ConversionToFunc t;     /*!< Conversion func to C object from PyObject */
} ListConvertor;

static PyObject *
get_list(_PackageObject *self, void *conv)
{
    if (check_PackageStatus(self))
        return NULL;

    ListConvertor *convertor = conv;
    PyObject *list;
    cr_Package *pkg = self->package;
    GSList *glist = convertor->get(pkg);

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
set_num(_PackageObject *self, PyObject *value, void *closure)
{
    gint64 val;
    if (check_PackageStatus(self))
        return -1;
    if (PyLong_Check(value)) {
        val = (gint64) PyLong_AsLong(value);
    } else if (PyFloat_Check(value)) {
        val = (gint64) PyFloat_AsDouble(value);
    } else {
        PyErr_SetString(PyExc_TypeError, "Number expected!");
        return -1;
    }
    cr_Package *pkg = self->package;
    ((NumField *) closure)->set(pkg, val);
    return 0;
}

static int
set_str(_PackageObject *self, PyObject *value, void *closure)
{
    if (check_PackageStatus(self))
        return -1;
    if (!PyUnicode_Check(value) && !PyBytes_Check(value) && value != Py_None) {
        PyErr_SetString(PyExc_TypeError, "Unicode, bytes, or None expected!");
        return -1;
    }
    cr_Package *pkg = self->package;
    StrField *field = closure;

    if (value == Py_None) {
        // If value is None exist right now (avoid possibly
        // creation of a string chunk)
        field->set(pkg, NULL);
        return 0;
    }

    // Check if chunk exits
    // If it doesn't - this is package from loaded metadata and all its
    // strings are in a metadata common chunk (cr_Metadata->chunk).
    // In this case, we have to create a chunk for this package before
    // inserting a new string.
    if (!cr_package_get_chunk(pkg))
        cr_package_set_chunk(pkg, g_string_chunk_new(0));

    PyObject *pybytes = PyObject_ToPyBytesOrNull(value);
    if (!pybytes) {
        return -1;
    }
    const char *str = PyBytes_AsString(pybytes);
    field->set(pkg, str);
    Py_DECREF(pybytes);
    return 0;
}


static int
CheckPyDependency(PyObject *dep)
{
    if (!PyTuple_Check(dep) || PyTuple_Size(dep) != 6) {
        PyErr_SetString(PyExc_TypeError, "Element of list has to be a tuple with 6 items.");
        return 1;
    }
    return 0;
}

static int
CheckPyPackageFile(PyObject *dep)
{
    // The fourth element (file checksum) is optional since it is present only in filelists-ext
    if (!PyTuple_Check(dep) || (PyTuple_Size(dep) != 4 && PyTuple_Size(dep) != 3)) {
        PyErr_SetString(PyExc_TypeError, "Element of list has to be a tuple with 3 or 4 items.");
        return 1;
    }
    return 0;
}

static int
CheckPyChangelogEntry(PyObject *dep)
{
    if (!PyTuple_Check(dep) || PyTuple_Size(dep) != 3) {
        PyErr_SetString(PyExc_TypeError, "Element of list has to be a tuple with 3 items.");
        return 1;
    }
    return 0;
}

static int
set_list(_PackageObject *self, PyObject *list, void *conv)
{
    ListConvertor *convertor = conv;
    GSList *glist = NULL;

    if (check_PackageStatus(self))
        return -1;
    if (!PyList_Check(list)) {
        PyErr_SetString(PyExc_TypeError, "List expected!");
        return -1;
    }

    // Check if chunk exits
    // If it doesn't - this is package from loaded metadata and all its
    // strings are in a metadata common chunk (cr_Metadata->chunk).
    // In this case, we have to create a chunk for this package before
    // inserting a new string.
    GStringChunk *chunk = cr_package_get_chunk(self->package);
    if (!chunk) {
        chunk = g_string_chunk_new(0);
        cr_package_set_chunk(self->package, chunk);
    }

    Py_ssize_t len = PyList_Size(list);

    // Check all elements
    for (Py_ssize_t x = 0; x < len; x++) {
        PyObject *elem = PyList_GetItem(list, x);
        if (convertor->t_check && convertor->t_check(elem))
            return -1;
    }

    for (Py_ssize_t x = 0; x < len; x++) {
        glist = g_slist_prepend(glist, convertor->t(PyList_GetItem(list, x), chunk));
    }

    convertor->set(self->package, glist);
    return 0;
}

static PyGetSetDef package_getsetters[] = {
    {"pkgId",            (getter)get_str, (setter)set_str,
      "Checksum of the package file",
      &(StrField){ cr_package_get_pkg_id, cr_package_set_pkg_id }},
    {"name",             (getter)get_str, (setter)set_str,
        "Name of the package",
        &(StrField){ cr_package_get_name, cr_package_set_name }},
    {"arch",             (getter)get_str, (setter)set_str,
        "Architecture for which the package was built",
        &(StrField){ cr_package_get_arch, cr_package_set_arch }},
    {"version",          (getter)get_str, (setter)set_str,
        "Version of the packaged software",
        &(StrField){ cr_package_get_version, cr_package_set_version }},
    {"epoch",            (getter)get_str, (setter)set_str,
        "Epoch", &(StrField){ cr_package_get_epoch, cr_package_set_epoch }},
    {"release",          (getter)get_str, (setter)set_str,
        "Release number of the package",
        &(StrField){ cr_package_get_release, cr_package_set_release }},
    {"summary",          (getter)get_str, (setter)set_str,
        "Short description of the packaged software",
        &(StrField){ cr_package_get_summary, cr_package_set_summary }},
    {"description",      (getter)get_str, (setter)set_str,
        "In-depth description of the packaged software",
        &(StrField){ cr_package_get_description, cr_package_set_description }},
    {"url",              (getter)get_str, (setter)set_str,
        "URL with more information about packaged software",
        &(StrField){ cr_package_get_url, cr_package_set_url }},
    {"time_file",        (getter)get_num, (setter)set_num,
        "mtime of the package file",
        &(NumField){ cr_package_get_time_file, cr_package_set_time_file }},
    {"time_build",       (getter)get_num, (setter)set_num,
        "Time when package was builded",
        &(NumField){ cr_package_get_time_build, cr_package_set_time_build }},
    {"rpm_license",      (getter)get_str, (setter)set_str,
        "License term applicable to the package software (GPLv2, etc.)",
        &(StrField){ cr_package_get_rpm_license, cr_package_set_rpm_license }},
    {"rpm_vendor",       (getter)get_str, (setter)set_str,
        "Name of the organization producing the package",
        &(StrField){ cr_package_get_rpm_vendor, cr_package_set_rpm_vendor }},
    {"rpm_group",        (getter)get_str, (setter)set_str,
        "RPM group (See: http://fedoraproject.org/wiki/RPMGroups)",
        &(StrField){ cr_package_get_rpm_group, cr_package_set_rpm_group }},
    {"rpm_buildhost",    (getter)get_str, (setter)set_str,
        "Hostname of the system that built the package",
        &(StrField){ cr_package_get_rpm_buildhost, cr_package_set_rpm_buildhost }},
    {"rpm_sourcerpm",    (getter)get_str, (setter)set_str,
        "Name of the source package from which this binary package was built",
        &(StrField){ cr_package_get_rpm_sourcerpm, cr_package_set_rpm_sourcerpm }},
    {"rpm_header_start", (getter)get_num, (setter)set_num,
        "First byte of the header",
        &(NumField){ cr_package_get_rpm_header_start, cr_package_set_rpm_header_start }},
    {"rpm_header_end",   (getter)get_num, (setter)set_num,
        "Last byte of the header",
        &(NumField){ cr_package_get_rpm_header_end, cr_package_set_rpm_header_end }},
    {"rpm_packager",     (getter)get_str, (setter)set_str,
        "Person or persons responsible for creating the package",
        &(StrField){ cr_package_get_rpm_packager, cr_package_set_rpm_packager }},
    {"size_package",     (getter)get_num, (setter)set_num,
        "Size, in bytes, of the package",
        &(NumField){ cr_package_get_size_package, cr_package_set_size_package }},
    {"size_installed",   (getter)get_num, (setter)set_num,
        "Total size, in bytes, of every file installed by this package",
        &(NumField){ cr_package_get_size_installed, cr_package_set_size_installed }},
    {"size_archive",     (getter)get_num, (setter)set_num,
        "Size, in bytes, of the archive portion of the original package file",
        &(NumField){ cr_package_get_size_archive, cr_package_set_size_archive }},
    {"location_href",    (getter)get_str, (setter)set_str,
        "Relative location of package to the repodata",
        &(StrField){ cr_package_get_location_href, cr_package_set_location_href }},
    {"location_base",    (getter)get_str, (setter)set_str,
        "Base location of this package",
        &(StrField){ cr_package_get_location_base, cr_package_set_location_base }},
    {"checksum_type",    (getter)get_str, (setter)set_str,
        "Type of checksum",
        &(StrField){ cr_package_get_checksum_type, cr_package_set_checksum_type }},
    {"files_checksum_type",    (getter)get_str, (setter)set_str,
        "Type of checksum for files",
        &(StrField){ cr_package_get_files_checksum_type,
                     cr_package_set_files_checksum_type }},
    {"requires",         (getter)get_list, (setter)set_list,
        "Capabilities the package requires",
        &(ListConvertor){ cr_package_get_requires, cr_package_set_requires,
            (ConversionFromFunc) PyObject_FromDependency, CheckPyDependency,
            (ConversionToFunc) PyObject_ToDependency }},
    {"provides",         (getter)get_list, (setter)set_list,
        "Capabilities the package provides",
        &(ListConvertor){ cr_package_get_provides, cr_package_set_provides,
            (ConversionFromFunc) PyObject_FromDependency, CheckPyDependency,
            (ConversionToFunc) PyObject_ToDependency }},
    {"conflicts",        (getter)get_list, (setter)set_list,
        "Capabilities the package conflicts with",
        &(ListConvertor){ cr_package_get_conflicts, cr_package_set_conflicts,
            (ConversionFromFunc) PyObject_FromDependency, CheckPyDependency,
            (ConversionToFunc) PyObject_ToDependency }},
    {"obsoletes",        (getter)get_list, (setter)set_list,
        "Capabilities the package obsoletes",
        &(ListConvertor){ cr_package_get_obsoletes, cr_package_set_obsoletes,
            (ConversionFromFunc) PyObject_FromDependency, CheckPyDependency,
            (ConversionToFunc) PyObject_ToDependency }},
    {"suggests",         (getter)get_list, (setter)set_list,
        "Capabilities the package suggests",
        &(ListConvertor){ cr_package_get_suggests, cr_package_set_suggests,
            (ConversionFromFunc) PyObject_FromDependency, CheckPyDependency,
            (ConversionToFunc) PyObject_ToDependency }},
    {"enhances",         (getter)get_list, (setter)set_list,
        "Capabilities the package enhances",
        &(ListConvertor){ cr_package_get_enhances, cr_package_set_enhances,
            (ConversionFromFunc) PyObject_FromDependency, CheckPyDependency,
            (ConversionToFunc) PyObject_ToDependency }},
    {"recommends",       (getter)get_list, (setter)set_list,
        "Capabilities the package recommends",
        &(ListConvertor){ cr_package_get_recommends, cr_package_set_recommends,
            (ConversionFromFunc) PyObject_FromDependency, CheckPyDependency,
            (ConversionToFunc) PyObject_ToDependency }},
    {"supplements",      (getter)get_list, (setter)set_list,
        "Capabilities the package supplements",
        &(ListConvertor){ cr_package_get_supplements, cr_package_set_supplements,
            (ConversionFromFunc) PyObject_FromDependency, CheckPyDependency,
            (ConversionToFunc) PyObject_ToDependency }},
    {"files",            (getter)get_list, (setter)set_list,
        "Files that package contains",
        &(ListConvertor){ cr_package_get_files, cr_package_set_files,
            (ConversionFromFunc) PyObject_FromPackageFile, CheckPyPackageFile,
            (ConversionToFunc) PyObject_ToPackageFile }},
    {"changelogs",       (getter)get_list, (setter)set_list,
        "Changelogs that package contains",
        &(ListConvertor){ cr_package_get_changelogs, cr_package_set_changelogs,
            (ConversionFromFunc) PyObject_FromChangelogEntry, CheckPyChangelogEntry,
            (ConversionToFunc) PyObject_ToChangelogEntry }},
    {"signatures",       (getter)get_signatures, NULL,
        "OpenPGP signatures of the package (read-only)", NULL},
    {NULL, NULL, NULL, NULL, NULL} /* sentinel */
};

/* Object */

PyTypeObject Package_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "createrepo_c.Package",
    .tp_basicsize = sizeof(_PackageObject),
    .tp_dealloc = (destructor) package_dealloc,
    .tp_repr = (reprfunc) package_repr,
    .tp_str = (reprfunc) package_str,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = package_init__doc__,
    .tp_iter = PyObject_SelfIter,
    .tp_methods = package_methods,
    .tp_getset = package_getsetters,
    .tp_init = (initproc) package_init,
    .tp_new = package_new,
};
