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
repomd_new(PyTypeObject *type,
           G_GNUC_UNUSED PyObject *args,
           G_GNUC_UNUSED PyObject *kwds)
{
    _RepomdObject *self = (_RepomdObject *)type->tp_alloc(type, 0);
    if (self) {
        self->repomd = NULL;
    }
    return (PyObject *)self;
}

PyDoc_STRVAR(repomd_init__doc__,
"Repomd object");

static int
repomd_init(_RepomdObject *self,
            G_GNUC_UNUSED PyObject *args,
            G_GNUC_UNUSED PyObject *kwds)
{
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
repomd_repr(G_GNUC_UNUSED _RepomdObject *self)
{
    return PyUnicode_FromString("<createrepo_c.Repomd object>");
}

/* Repomd methods */

PyDoc_STRVAR(set_record__doc__,
"set_record(repomdrecord) -> None\n\n"
"Add RepomdRecord");

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

PyDoc_STRVAR(set_revision__doc__,
"set_revision(revision) -> None\n\n"
"Set revision string");

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

PyDoc_STRVAR(set_repoid__doc__,
"set_repoid(repoid, repoid_type) -> None\n\n"
"Set repoid value and repoid_type");

static PyObject *
set_repoid(_RepomdObject *self, PyObject *args)
{
    char *repoid, *repoid_type;
    if (!PyArg_ParseTuple(args, "zz:set_repoid", &repoid, &repoid_type))
        return NULL;
    if (check_RepomdStatus(self))
        return NULL;
    cr_repomd_set_repoid(self->repomd, repoid, repoid_type);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(set_contenthash__doc__,
"set_contenthash(contenthash, contenthash_type) -> None\n\n"
"Set contenthash value and contenthash_type");

static PyObject *
set_contenthash(_RepomdObject *self, PyObject *args)
{
    char *contenthash, *contenthash_type;
    if (!PyArg_ParseTuple(args, "zz:set_contenthash", &contenthash, &contenthash_type))
        return NULL;
    if (check_RepomdStatus(self))
        return NULL;
    cr_repomd_set_contenthash(self->repomd, contenthash, contenthash_type);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(add_distro_tag__doc__,
"add_distro_tag(tag[, cpeid=None]) -> None\n\n"
"Add distro tag");

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

PyDoc_STRVAR(add_repo_tag__doc__,
"add_repo_tag(tag) -> None\n\n"
"Add repo tag");

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

PyDoc_STRVAR(add_content_tag__doc__,
"add_content_tag(tag) -> None\n\n"
"Add content tag");

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

PyDoc_STRVAR(sort_records__doc__,
"sort_records() -> str\n\n"
"Sort repomd records to the createrepo_c preferred order");

static PyObject *
sort_records(_RepomdObject *self, G_GNUC_UNUSED void *nothing)
{
    cr_repomd_sort_records(self->repomd);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(xml_dump__doc__,
"xml_dump() -> str\n\n"
"Generate xml representation of the repomd");

static PyObject *
xml_dump(_RepomdObject *self, G_GNUC_UNUSED void *nothing)
{
    PyObject *py_str;
    GError *tmp_err = NULL;
    char *xml = cr_xml_dump_repomd(self->repomd, &tmp_err);
    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }
    py_str = PyUnicodeOrNone_FromString(xml);
    free(xml);
    return py_str;
}

static struct PyMethodDef repomd_methods[] = {
    {"set_record", (PyCFunction)set_record, METH_VARARGS,
        set_record__doc__},
    {"set_revision", (PyCFunction)set_revision, METH_VARARGS,
        set_revision__doc__},
    {"set_repoid", (PyCFunction)set_repoid, METH_VARARGS,
        set_repoid__doc__},
    {"set_contenthash", (PyCFunction)set_contenthash, METH_VARARGS,
        set_contenthash__doc__},
    {"add_distro_tag", (PyCFunction)add_distro_tag, METH_VARARGS|METH_KEYWORDS,
        add_distro_tag__doc__},
    {"add_repo_tag", (PyCFunction)add_repo_tag, METH_VARARGS,
        add_repo_tag__doc__},
    {"add_content_tag", (PyCFunction)add_content_tag, METH_VARARGS,
        add_content_tag__doc__},
    {"sort_records", (PyCFunction)sort_records, METH_NOARGS,
        sort_records__doc__},
    {"xml_dump", (PyCFunction)xml_dump, METH_NOARGS,
        xml_dump__doc__},
    {NULL, NULL, 0, NULL} /* sentinel */
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
CheckPyUnicode(PyObject *dep)
{
    if (!PyUnicode_Check(dep)) {
        PyErr_SetString(PyExc_TypeError, "Element of list has to be a string");
        return 1;
    }
    return 0;
}

static int
CheckPyDistroTag(PyObject *dep)
{
    if (!PyTuple_Check(dep) || PyTuple_Size(dep) != 2) {
        PyErr_SetString(PyExc_TypeError, "Element of list has to be a tuple with 2 items.");
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
    { offsetof(cr_Repomd, repo_tags),
      (ConversionFromFunc) PyUnicodeOrNone_FromString,
      (ConversionToCheckFunc) CheckPyUnicode,
      (ConversionToFunc) PyObject_ToChunkedString },
    { offsetof(cr_Repomd, distro_tags),
      (ConversionFromFunc) PyObject_FromDistroTag,
      (ConversionToCheckFunc) CheckPyDistroTag,
      (ConversionToFunc) PyObject_ToDistroTag },
    { offsetof(cr_Repomd, content_tags),
      (ConversionFromFunc) PyUnicodeOrNone_FromString,
      (ConversionToCheckFunc) CheckPyUnicode,
      (ConversionToFunc) PyObject_ToChunkedString },
    { offsetof(cr_Repomd, records),
      (ConversionFromFunc) PyObject_FromRepomdRecord,
      (ConversionToCheckFunc) NULL,
      (ConversionToFunc) NULL },
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
    return PyUnicode_FromString(str);
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
set_str(_RepomdObject *self, PyObject *value, void *member_offset)
{
    if (check_RepomdStatus(self))
        return -1;
    if (!PyUnicode_Check(value) && !PyBytes_Check(value) && value != Py_None) {
        PyErr_SetString(PyExc_TypeError, "Unicode, bytes, or None expected!");
        return -1;
    }
    cr_Repomd *repomd = self->repomd;

    char *str = PyObject_ToChunkedString(value, repomd->chunk);
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
        PyErr_SetString(PyExc_TypeError, "List expected!");
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
    {"revision",         (getter)get_str,  (setter)set_str,
        "Revision value", OFFSET(revision)},
    {"repoid",           (getter)get_str,  (setter)set_str,
        "Repoid value", OFFSET(repoid)},
    {"repoid_type",      (getter)get_str,  (setter)set_str,
        "Repoid type value", OFFSET(repoid_type)},
     {"contenthash",     (getter)get_str,  (setter)set_str,
        "Contenthash value", OFFSET(contenthash)},
    {"contenthash_type", (getter)get_str,  (setter)set_str,
        "contenthash type value", OFFSET(contenthash_type)},
    {"repo_tags",        (getter)get_list, (setter)set_list,
        "List of repo tags", &(list_convertors[0])},
    {"distro_tags",      (getter)get_list, (setter)set_list,
        "List of distro tags", &(list_convertors[1])},
    {"content_tags",     (getter)get_list, (setter)set_list,
        "List of content tags", &(list_convertors[2])},
    {"records",          (getter)get_list, (setter)NULL,
        "List of RepomdRecords", &(list_convertors[3])},
    {NULL, NULL, NULL, NULL, NULL} /* sentinel */
};

/* Object */


PyTypeObject Repomd_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name =  "createrepo_c.Repomd",
    .tp_basicsize =  sizeof(_RepomdObject),
    .tp_dealloc =  (destructor) repomd_dealloc,
    .tp_repr =  (reprfunc) repomd_repr,
    .tp_flags =  Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
    .tp_doc =  repomd_init__doc__,
    .tp_iter =  PyObject_SelfIter,
    .tp_methods =  repomd_methods,
    .tp_getset =  repomd_getsetters,
    .tp_init =  (initproc) repomd_init,
    .tp_new =  repomd_new,
};
