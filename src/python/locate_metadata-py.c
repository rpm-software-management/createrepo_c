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

#include "locate_metadata-py.h"
#include "exception-py.h"
#include "typeconversion.h"

typedef struct {
    PyObject_HEAD
    struct cr_MetadataLocation *ml;
} _MetadataLocationObject;

struct cr_MetadataLocation *
MetadataLocation_FromPyObject(PyObject *o)
{
    if (!MetadataLocationObject_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "Expected a createrepo_c.MetadataLocation object.");
        return NULL;
    }
    return ((_MetadataLocationObject *) o)->ml;
}

static int
check_MetadataLocationStatus(const _MetadataLocationObject *self)
{
    assert(self != NULL);
    assert(MetadataLocationObject_Check(self));
    if (self->ml == NULL) {
        PyErr_SetString(CrErr_Exception, "Improper createrepo_c MetadataLocation object.");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
metadatalocation_new(PyTypeObject *type,
                     G_GNUC_UNUSED PyObject *args,
                     G_GNUC_UNUSED PyObject *kwds)
{
    _MetadataLocationObject *self = (_MetadataLocationObject *)type->tp_alloc(type, 0);
    if (self)
        self->ml = NULL;
    return (PyObject *)self;
}

PyDoc_STRVAR(metadatalocation_init__doc__,
"Class representing location of metadata\n\n"
".. method:: __init__(path, ignore_db)\n\n"
"    :arg path: String with url/path to the repository\n"
"    :arg ignore_db: Boolean. If False then in case of remote repository\n"
"                    databases will not be downloaded)\n");

static int
metadatalocation_init(_MetadataLocationObject *self,
                      PyObject *args,
                      G_GNUC_UNUSED PyObject *kwds)
{
    char *repopath;
    PyObject *py_ignore_db = NULL;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sO|:metadatalocation_init", &repopath, &py_ignore_db))
        return -1;

    /* Free all previous resources when reinitialization */
    if (self->ml) {
        cr_metadatalocation_free(self->ml);
    }

    /* Init */
    self->ml = cr_locate_metadata(repopath, PyObject_IsTrue(py_ignore_db), &tmp_err);
    if (tmp_err) {
        g_clear_pointer(&(self->ml), cr_metadatalocation_free);
        nice_exception(&tmp_err, NULL);
        return -1;
    }
    return 0;
}

static void
metadatalocation_dealloc(_MetadataLocationObject *self)
{
    if (self->ml)
        cr_metadatalocation_free(self->ml);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
metadatalocation_repr(G_GNUC_UNUSED _MetadataLocationObject *self)
{
    return PyUnicode_FromFormat("<createrepo_c.MetadataLocation object>");
}

/* MetadataLocation methods */

static struct PyMethodDef metadatalocation_methods[] = {
    {NULL, NULL, 0, NULL} /* sentinel */
};

/* Mapping interface */

Py_ssize_t
length(_MetadataLocationObject *self)
{
    if (self->ml)
        return 9;
    return 0;
}

PyObject *
getitem(_MetadataLocationObject *self, PyObject *pykey)
{
    char *key, *value;

    if (check_MetadataLocationStatus(self))
        return NULL;

    if (!PyUnicode_Check(pykey) && !PyBytes_Check(pykey)) {
        PyErr_SetString(PyExc_TypeError, "Unicode or bytes expected!");
        return NULL;
    }

    pykey = PyObject_ToPyBytesOrNull(pykey);
    if (!pykey) {
        return NULL;
    }
    key = PyBytes_AsString(pykey);
    value = NULL;

    if (!strcmp(key, "primary")) {
        value = self->ml->pri_xml_href;
    } else if (!strcmp(key, "filelists")) {
        value = self->ml->fil_xml_href;
    } else if (!strcmp(key, "other")) {
        value = self->ml->oth_xml_href;
    } else if (!strcmp(key, "primary_db")) {
        value = self->ml->pri_sqlite_href;
    } else if (!strcmp(key, "filelists_db")) {
        value = self->ml->fil_sqlite_href;
    } else if (!strcmp(key, "other_db")) {
        value = self->ml->oth_sqlite_href;
    } else if (!strcmp(key, "group")) {   //NOTE(amatej): Preserve old API for these specific files (group, group_gz, updateinfo)
        if (self->ml->additional_metadata){
            GSList *m = g_slist_find_custom(self->ml->additional_metadata, "group", cr_cmp_metadatum_type);
            if (m)
                value = ((cr_Metadatum *)(m->data))->name;
        }
    } else if (!strcmp(key, "group_gz")) {
        if (self->ml->additional_metadata){
            GSList *m = g_slist_find_custom(self->ml->additional_metadata, "group_gz", cr_cmp_metadatum_type);
            if (m)
                value = ((cr_Metadatum *)(m->data))->name;
        }
    } else if (!strcmp(key, "updateinfo")) {
        if (self->ml->additional_metadata){
            GSList *m = g_slist_find_custom(self->ml->additional_metadata, "updateinfo", cr_cmp_metadatum_type);
            if (m)
                value = ((cr_Metadatum *)(m->data))->name;
        }
    } else if (!strcmp(key, "additional_metadata")){
        if (self->ml->additional_metadata){
            PyObject *list = PyList_New(0);
            if (!list) {
                Py_XDECREF(pykey);
                return NULL;
            }
            for (GSList *elem = self->ml->additional_metadata; elem; elem=g_slist_next(elem)){
                PyObject *namestr = PyUnicode_FromString(((cr_Metadatum *)(elem->data))->name);
                if (!namestr || PyList_Append(list, namestr)) {
                    Py_DECREF(list);
                    Py_XDECREF(pykey);
                    return NULL;
                } else {
                    Py_DECREF(namestr);
                }
            }
            Py_XDECREF(pykey);
            return list;
        }
    }

    Py_XDECREF(pykey);

    if (value) {
        return PyUnicode_FromString(value);
    } else {
        Py_RETURN_NONE;
    }
}

static PyMappingMethods mapping_methods = {
    .mp_length = (lenfunc) length,
    .mp_subscript = (binaryfunc) getitem,
    .mp_ass_subscript = NULL,
};

/* Object */

PyTypeObject MetadataLocation_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "createrepo_c.MetadataLocation",
    .tp_basicsize = sizeof(_MetadataLocationObject),
    .tp_dealloc = (destructor)metadatalocation_dealloc,
    .tp_repr = (reprfunc)metadatalocation_repr,
    .tp_as_mapping = &mapping_methods,
    .tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc = metadatalocation_init__doc__,
    .tp_iter = PyObject_SelfIter,
    .tp_methods = metadatalocation_methods,
    .tp_init = (initproc)metadatalocation_init,
    .tp_new = metadatalocation_new,
};
