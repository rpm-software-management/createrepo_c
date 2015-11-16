/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012-2013  Tomas Mlcoch
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
#include "typeconversion.h"
#include "src/createrepo_c.h"
#include "src/repomd_internal.h"

#define ERR_DOMAIN              CREATEREPO_C_ERROR


void
PyErr_ToGError(GError **err)
{
    PyObject *type, *val, *traceback, *pystr;

    if (!err)
        return;

    assert(*err == NULL);

    PyErr_Fetch(&type, &val, &traceback);

    pystr = PyObject_Str(val);

    Py_XDECREF(type);
    Py_XDECREF(val);
    Py_XDECREF(traceback);

    if (!pystr) {
        PyErr_Clear();
        g_set_error(err, ERR_DOMAIN, CRE_XMLPARSER,
                    "Error while error handling");
    } else {
        if (PyUnicode_Check(pystr)) {
            pystr = PyUnicode_AsUTF8String(pystr);
        }
        g_set_error(err, ERR_DOMAIN, CRE_XMLPARSER,
                    "%s", PyBytes_AsString(pystr));
    }

    Py_XDECREF(pystr);
}

PyObject *
PyUnicodeOrNone_FromString(const char *str)
{
    if (str == NULL)
        Py_RETURN_NONE;
    return PyUnicode_FromString(str);
}

char *
PyObject_ToStrOrNull(PyObject *pyobj)
{
    // String returned by this function shoud not be freed or modified
    if (PyUnicode_Check(pyobj)) {
        pyobj = PyUnicode_AsUTF8String(pyobj);
    }

    if (PyBytes_Check(pyobj)) {
        return PyBytes_AsString(pyobj);
    }

    // TODO: ? Add support for pyobj like: ("xxx",) and ["xxx"]
    return NULL;
}

char *
PyObject_ToChunkedString(PyObject *pyobj, GStringChunk *chunk)
{
    return cr_safe_string_chunk_insert(chunk, PyObject_ToStrOrNull(pyobj));
}

long long
PyObject_ToLongLongOrZero(PyObject *pyobj)
{
    long long num = 0;
    if (PyLong_Check(pyobj)) {
        num = (long long) PyLong_AsLongLong(pyobj);
    } else if (PyFloat_Check(pyobj)) {
        num = (long long) PyFloat_AS_DOUBLE(pyobj);
#if PY_MAJOR_VERSION < 3
    } else if (PyInt_Check(pyobj)) {
        num = (long long) PyInt_AS_LONG(pyobj);
#endif
    }
    return num;
}

PyObject *
PyObject_FromDependency(cr_Dependency *dep)
{
    PyObject *tuple;

    if ((tuple = PyTuple_New(6)) == NULL)
        return NULL;

    PyTuple_SetItem(tuple, 0, PyUnicodeOrNone_FromString(dep->name));
    PyTuple_SetItem(tuple, 1, PyUnicodeOrNone_FromString(dep->flags));
    PyTuple_SetItem(tuple, 2, PyUnicodeOrNone_FromString(dep->epoch));
    PyTuple_SetItem(tuple, 3, PyUnicodeOrNone_FromString(dep->version));
    PyTuple_SetItem(tuple, 4, PyUnicodeOrNone_FromString(dep->release));
    PyTuple_SetItem(tuple, 5, PyBool_FromLong((long) dep->pre));

    return tuple;
}

cr_Dependency *
PyObject_ToDependency(PyObject *tuple, GStringChunk *chunk)
{
    PyObject *pyobj;
    cr_Dependency *dep = cr_dependency_new();

    pyobj = PyTuple_GetItem(tuple, 0);
    dep->name = cr_safe_string_chunk_insert(chunk, PyObject_ToStrOrNull(pyobj));

    pyobj = PyTuple_GetItem(tuple, 1);
    dep->flags = cr_safe_string_chunk_insert(chunk, PyObject_ToStrOrNull(pyobj));

    pyobj = PyTuple_GetItem(tuple, 2);
    dep->epoch = cr_safe_string_chunk_insert(chunk, PyObject_ToStrOrNull(pyobj));

    pyobj = PyTuple_GetItem(tuple, 3);
    dep->version = cr_safe_string_chunk_insert(chunk, PyObject_ToStrOrNull(pyobj));

    pyobj = PyTuple_GetItem(tuple, 4);
    dep->release = cr_safe_string_chunk_insert(chunk, PyObject_ToStrOrNull(pyobj));

    pyobj = PyTuple_GetItem(tuple, 5);
    dep->pre = (PyObject_IsTrue(pyobj)) ? TRUE : FALSE;

    return dep;
}

PyObject *
PyObject_FromPackageFile(cr_PackageFile *file)
{
    PyObject *tuple;

    if ((tuple = PyTuple_New(3)) == NULL)
        return NULL;

    PyTuple_SetItem(tuple, 0, PyUnicodeOrNone_FromString(file->type));
    PyTuple_SetItem(tuple, 1, PyUnicodeOrNone_FromString(file->path));
    PyTuple_SetItem(tuple, 2, PyUnicodeOrNone_FromString(file->name));

    return tuple;
}

cr_PackageFile *
PyObject_ToPackageFile(PyObject *tuple, GStringChunk *chunk)
{
    PyObject *pyobj;
    cr_PackageFile *file = cr_package_file_new();

    pyobj = PyTuple_GetItem(tuple, 0);
    file->type = cr_safe_string_chunk_insert(chunk, PyObject_ToStrOrNull(pyobj));

    pyobj = PyTuple_GetItem(tuple, 1);
    file->path = cr_safe_string_chunk_insert(chunk, PyObject_ToStrOrNull(pyobj));

    pyobj = PyTuple_GetItem(tuple, 2);
    file->name = cr_safe_string_chunk_insert(chunk, PyObject_ToStrOrNull(pyobj));

    return file;
}

PyObject *
PyObject_FromChangelogEntry(cr_ChangelogEntry *log)
{
    PyObject *tuple;

    if ((tuple = PyTuple_New(3)) == NULL)
        return NULL;

    PyTuple_SetItem(tuple, 0, PyUnicodeOrNone_FromString(log->author));
    PyTuple_SetItem(tuple, 1, PyLong_FromLong((long) log->date));
    PyTuple_SetItem(tuple, 2, PyUnicodeOrNone_FromString(log->changelog));

    return tuple;
}

cr_ChangelogEntry *
PyObject_ToChangelogEntry(PyObject *tuple, GStringChunk *chunk)
{
    PyObject *pyobj;
    cr_ChangelogEntry *log = cr_changelog_entry_new();

    pyobj = PyTuple_GetItem(tuple, 0);
    log->author = cr_safe_string_chunk_insert(chunk, PyObject_ToStrOrNull(pyobj));

    pyobj = PyTuple_GetItem(tuple, 1);
    log->date = PyObject_ToLongLongOrZero(pyobj);

    pyobj = PyTuple_GetItem(tuple, 2);
    log->changelog = cr_safe_string_chunk_insert(chunk, PyObject_ToStrOrNull(pyobj));

    return log;
}

PyObject *
PyObject_FromDistroTag(cr_DistroTag *tag)
{
    PyObject *tuple;

    if ((tuple = PyTuple_New(2)) == NULL)
        return NULL;

    PyTuple_SetItem(tuple, 0, PyUnicodeOrNone_FromString(tag->cpeid));
    PyTuple_SetItem(tuple, 1, PyUnicodeOrNone_FromString(tag->val));

    return tuple;
}

cr_DistroTag *
PyObject_ToDistroTag(PyObject *tuple, GStringChunk *chunk)
{
    PyObject *pyobj;
    cr_DistroTag *tag = cr_distrotag_new();

    pyobj = PyTuple_GetItem(tuple, 0);
    tag->cpeid = cr_safe_string_chunk_insert(chunk, PyObject_ToStrOrNull(pyobj));

    pyobj = PyTuple_GetItem(tuple, 2);
    tag->val = cr_safe_string_chunk_insert(chunk, PyObject_ToStrOrNull(pyobj));

    return tag;
}

GSList *
GSList_FromPyList_Str(PyObject *py_list)
{
    GSList *list = NULL;

    if (!py_list)
        return NULL;

    if (!PyList_Check(py_list))
        return NULL;

    Py_ssize_t size = PyList_Size(py_list);
    for (Py_ssize_t x=0; x < size; x++) {
        PyObject *py_str = PyList_GetItem(py_list, x);
        assert(py_str != NULL);
        if (!PyUnicode_Check(py_str) && !PyBytes_Check(py_str))
            // Hmm, element is not a string, just skip it
            continue;

        if (PyUnicode_Check(py_str))
            py_str = PyUnicode_AsUTF8String(py_str);

        list = g_slist_prepend(list, PyBytes_AsString(py_str));
    }

    return list;
}
