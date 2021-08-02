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

#include "src/createrepo_c.h"

#include "xml_parser-py.h"
#include "typeconversion.h"
#include "package-py.h"
#include "repomd-py.h"
#include "updateinfo-py.h"
#include "exception-py.h"

typedef struct {
    PyObject *py_newpkgcb;
    PyObject *py_pkgcb;
    PyObject *py_warningcb;
    PyObject *py_pkgs;       /*!< Current processed package */
} CbData;

static int
c_newpkgcb(cr_Package **pkg,
           const char *pkgId,
           const char *name,
           const char *arch,
           void *cbdata,
           GError **err)
{
    PyObject *arglist, *result;
    CbData *data = cbdata;

    arglist = Py_BuildValue("(sss)", pkgId, name, arch);
    result = PyObject_CallObject(data->py_newpkgcb, arglist);
    Py_DECREF(arglist);

    if (result == NULL) {
        // Exception raised
        PyErr_ToGError(err);
        return CR_CB_RET_ERR;
    }

    if (!PackageObject_Check(result) && result != Py_None) {
        PyErr_SetString(PyExc_TypeError,
            "Expected a cr_Package or None as a callback return value");
        Py_DECREF(result);
        return CR_CB_RET_ERR;
    }

    if (result == Py_None) {
        *pkg = NULL;
    } else {
        *pkg = Package_FromPyObject(result);
        if (data->py_pkgcb != Py_None) {
            // Store reference to the python pkg (result) only if we will need it later
            PyObject *keyFromPtr = PyLong_FromVoidPtr(*pkg);
            PyDict_SetItem(data->py_pkgs, keyFromPtr, result);
            Py_DECREF(keyFromPtr);
        }
    }

    Py_DECREF(result);

    return CR_CB_RET_OK;
}

static int
c_pkgcb(cr_Package *pkg,
        void *cbdata,
        GError **err)
{
    PyObject *arglist, *result, *py_pkg;
    CbData *data = cbdata;

    PyObject *keyFromPtr = PyLong_FromVoidPtr(pkg);
    py_pkg = PyDict_GetItem(data->py_pkgs, keyFromPtr);
    if (py_pkg) {
        arglist = Py_BuildValue("(O)", py_pkg);
        result = PyObject_CallObject(data->py_pkgcb, arglist);
        PyDict_DelItem(data->py_pkgs, keyFromPtr);
    } else {
        // The package was not provided by user in c_newpkgcb,
        // create new python package object
        PyObject *new_py_pkg = Object_FromPackage(pkg, 1);
        arglist = Py_BuildValue("(O)", new_py_pkg);
        result = PyObject_CallObject(data->py_pkgcb, arglist);
        Py_DECREF(new_py_pkg);
    }

    Py_DECREF(arglist);
    Py_DECREF(keyFromPtr);

    if (result == NULL) {
        // Exception raised
        PyErr_ToGError(err);
        return CR_CB_RET_ERR;
    }

    Py_DECREF(result);
    return CR_CB_RET_OK;
}

static int
c_warningcb(cr_XmlParserWarningType type,
            char *msg,
            void *cbdata,
            GError **err)
{
    PyObject *arglist, *result;
    CbData *data = cbdata;

    arglist = Py_BuildValue("(is)", type, msg);
    result = PyObject_CallObject(data->py_warningcb, arglist);
    Py_DECREF(arglist);

    if (result == NULL) {
        // Exception raised
        PyErr_ToGError(err);
        return CR_CB_RET_ERR;
    }

    Py_DECREF(result);
    return CR_CB_RET_OK;
}

PyObject *
py_xml_parse_primary(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    char *filename;
    int do_files;
    PyObject *py_newpkgcb, *py_pkgcb, *py_warningcb;
    CbData cbdata;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sOOOi:py_xml_parse_primary",
                                         &filename,
                                         &py_newpkgcb,
                                         &py_pkgcb,
                                         &py_warningcb,
                                         &do_files)) {
        return NULL;
    }

    if (!PyCallable_Check(py_newpkgcb) && py_newpkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_pkgcb) && py_pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_warningcb) && py_warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    if (py_newpkgcb == Py_None && py_pkgcb == Py_None) {
        PyErr_SetString(PyExc_ValueError, "both pkgcb and newpkgcb cannot be None");
        return NULL;
    }

    Py_XINCREF(py_newpkgcb);
    Py_XINCREF(py_pkgcb);
    Py_XINCREF(py_warningcb);

    cr_XmlParserNewPkgCb    ptr_c_newpkgcb  = NULL;
    cr_XmlParserPkgCb       ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (py_newpkgcb != Py_None)
        ptr_c_newpkgcb = c_newpkgcb;
    if (py_pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (py_warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cbdata.py_newpkgcb  = py_newpkgcb;
    cbdata.py_pkgcb     = py_pkgcb;
    cbdata.py_warningcb = py_warningcb;
    cbdata.py_pkgs      = PyDict_New();

    cr_xml_parse_primary(filename,
                         ptr_c_newpkgcb,
                         &cbdata,
                         ptr_c_pkgcb,
                         &cbdata,
                         ptr_c_warningcb,
                         &cbdata,
                         do_files,
                         &tmp_err);

    Py_XDECREF(py_newpkgcb);
    Py_XDECREF(py_pkgcb);
    Py_XDECREF(py_warningcb);

    Py_XDECREF(cbdata.py_pkgs);

    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
py_xml_parse_primary_snippet(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    char *target;
    int do_files;
    PyObject *py_newpkgcb, *py_pkgcb, *py_warningcb;
    CbData cbdata;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sOOOi:py_xml_parse_primary_snippet",
                                          &target,
                                          &py_newpkgcb,
                                          &py_pkgcb,
                                          &py_warningcb,
                                          &do_files)) {
        return NULL;
    }

    if (!PyCallable_Check(py_newpkgcb) && py_newpkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_pkgcb) && py_pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_warningcb) && py_warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    if (py_newpkgcb == Py_None && py_pkgcb == Py_None) {
        PyErr_SetString(PyExc_ValueError, "both pkgcb and newpkgcb cannot be None");
        return NULL;
    }

    Py_XINCREF(py_newpkgcb);
    Py_XINCREF(py_pkgcb);
    Py_XINCREF(py_warningcb);

    cr_XmlParserNewPkgCb    ptr_c_newpkgcb  = NULL;
    cr_XmlParserPkgCb       ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (py_newpkgcb != Py_None)
        ptr_c_newpkgcb = c_newpkgcb;
    if (py_pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (py_warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cbdata.py_newpkgcb  = py_newpkgcb;
    cbdata.py_pkgcb     = py_pkgcb;
    cbdata.py_warningcb = py_warningcb;
    cbdata.py_pkgs      = PyDict_New();

    cr_xml_parse_primary_snippet(target, ptr_c_newpkgcb, &cbdata, ptr_c_pkgcb, &cbdata,
                                 ptr_c_warningcb, &cbdata, do_files, &tmp_err);

    Py_XDECREF(py_newpkgcb);
    Py_XDECREF(py_pkgcb);
    Py_XDECREF(py_warningcb);
    Py_XDECREF(cbdata.py_pkgs);

    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
py_xml_parse_filelists(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    char *filename;
    PyObject *py_newpkgcb, *py_pkgcb, *py_warningcb;
    CbData cbdata;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sOOO:py_xml_parse_filelists",
                                         &filename,
                                         &py_newpkgcb,
                                         &py_pkgcb,
                                         &py_warningcb)) {
        return NULL;
    }

    if (!PyCallable_Check(py_newpkgcb) && py_newpkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_pkgcb) && py_pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_warningcb) && py_warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    if (py_newpkgcb == Py_None && py_pkgcb == Py_None) {
        PyErr_SetString(PyExc_ValueError, "both pkgcb and newpkgcb cannot be None");
        return NULL;
    }

    Py_XINCREF(py_newpkgcb);
    Py_XINCREF(py_pkgcb);
    Py_XINCREF(py_warningcb);

    cr_XmlParserNewPkgCb    ptr_c_newpkgcb  = NULL;
    cr_XmlParserPkgCb       ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (py_newpkgcb != Py_None)
        ptr_c_newpkgcb = c_newpkgcb;
    if (py_pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (py_warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cbdata.py_newpkgcb  = py_newpkgcb;
    cbdata.py_pkgcb     = py_pkgcb;
    cbdata.py_warningcb = py_warningcb;
    cbdata.py_pkgs      = PyDict_New();

    cr_xml_parse_filelists(filename,
                           ptr_c_newpkgcb,
                           &cbdata,
                           ptr_c_pkgcb,
                           &cbdata,
                           ptr_c_warningcb,
                           &cbdata,
                           &tmp_err);

    Py_XDECREF(py_newpkgcb);
    Py_XDECREF(py_pkgcb);
    Py_XDECREF(py_warningcb);
    Py_XDECREF(cbdata.py_pkgs);

    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
py_xml_parse_filelists_snippet(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    char *target;
    PyObject *py_newpkgcb, *py_pkgcb, *py_warningcb;
    CbData cbdata;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sOOO:py_xml_parse_filelists_snippet",
                                         &target,
                                         &py_newpkgcb,
                                         &py_pkgcb,
                                         &py_warningcb)) {
        return NULL;
    }

    if (!PyCallable_Check(py_newpkgcb) && py_newpkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_pkgcb) && py_pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_warningcb) && py_warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    if (py_newpkgcb == Py_None && py_pkgcb == Py_None) {
        PyErr_SetString(PyExc_ValueError, "both pkgcb and newpkgcb cannot be None");
        return NULL;
    }

    Py_XINCREF(py_newpkgcb);
    Py_XINCREF(py_pkgcb);
    Py_XINCREF(py_warningcb);

    cr_XmlParserNewPkgCb    ptr_c_newpkgcb  = NULL;
    cr_XmlParserPkgCb       ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (py_newpkgcb != Py_None)
        ptr_c_newpkgcb = c_newpkgcb;
    if (py_pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (py_warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cbdata.py_newpkgcb  = py_newpkgcb;
    cbdata.py_pkgcb     = py_pkgcb;
    cbdata.py_warningcb = py_warningcb;
    cbdata.py_pkgs      = PyDict_New();

    cr_xml_parse_filelists_snippet(target, ptr_c_newpkgcb, &cbdata, ptr_c_pkgcb,
                                   &cbdata, ptr_c_warningcb, &cbdata, &tmp_err);

    Py_XDECREF(py_newpkgcb);
    Py_XDECREF(py_pkgcb);
    Py_XDECREF(py_warningcb);
    Py_XDECREF(cbdata.py_pkgs);

    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
py_xml_parse_other(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    char *filename;
    PyObject *py_newpkgcb, *py_pkgcb, *py_warningcb;
    CbData cbdata;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sOOO:py_xml_parse_other",
                                         &filename,
                                         &py_newpkgcb,
                                         &py_pkgcb,
                                         &py_warningcb)) {
        return NULL;
    }

    if (!PyCallable_Check(py_newpkgcb) && py_newpkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_pkgcb) && py_pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_warningcb) && py_warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    if (py_newpkgcb == Py_None && py_pkgcb == Py_None) {
        PyErr_SetString(PyExc_ValueError, "both pkgcb and newpkgcb cannot be None");
        return NULL;
    }

    Py_XINCREF(py_newpkgcb);
    Py_XINCREF(py_pkgcb);
    Py_XINCREF(py_warningcb);

    cr_XmlParserNewPkgCb    ptr_c_newpkgcb  = NULL;
    cr_XmlParserPkgCb       ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (py_newpkgcb != Py_None)
        ptr_c_newpkgcb = c_newpkgcb;
    if (py_pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (py_warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cbdata.py_newpkgcb  = py_newpkgcb;
    cbdata.py_pkgcb     = py_pkgcb;
    cbdata.py_warningcb = py_warningcb;
    cbdata.py_pkgs      = PyDict_New();

    cr_xml_parse_other(filename,
                       ptr_c_newpkgcb,
                       &cbdata,
                       ptr_c_pkgcb,
                       &cbdata,
                       ptr_c_warningcb,
                       &cbdata,
                       &tmp_err);

    Py_XDECREF(py_newpkgcb);
    Py_XDECREF(py_pkgcb);
    Py_XDECREF(py_warningcb);
    Py_XDECREF(cbdata.py_pkgs);

    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
py_xml_parse_other_snippet(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    char *target;
    PyObject *py_newpkgcb, *py_pkgcb, *py_warningcb;
    CbData cbdata;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sOOO:py_xml_parse_other_snippet",
                                         &target,
                                         &py_newpkgcb,
                                         &py_pkgcb,
                                         &py_warningcb)) {
        return NULL;
    }

    if (!PyCallable_Check(py_newpkgcb) && py_newpkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_pkgcb) && py_pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_warningcb) && py_warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    if (py_newpkgcb == Py_None && py_pkgcb == Py_None) {
        PyErr_SetString(PyExc_ValueError, "both pkgcb and newpkgcb cannot be None");
        return NULL;
    }

    Py_XINCREF(py_newpkgcb);
    Py_XINCREF(py_pkgcb);
    Py_XINCREF(py_warningcb);

    cr_XmlParserNewPkgCb    ptr_c_newpkgcb  = NULL;
    cr_XmlParserPkgCb       ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (py_newpkgcb != Py_None)
        ptr_c_newpkgcb = c_newpkgcb;
    if (py_pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (py_warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cbdata.py_newpkgcb  = py_newpkgcb;
    cbdata.py_pkgcb     = py_pkgcb;
    cbdata.py_warningcb = py_warningcb;
    cbdata.py_pkgs      = PyDict_New();

    cr_xml_parse_other_snippet(target, ptr_c_newpkgcb, &cbdata, ptr_c_pkgcb, &cbdata,
                               ptr_c_warningcb, &cbdata, &tmp_err);

    Py_XDECREF(py_newpkgcb);
    Py_XDECREF(py_pkgcb);
    Py_XDECREF(py_warningcb);
    Py_XDECREF(cbdata.py_pkgs);

    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
py_xml_parse_repomd(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    char *filename;
    PyObject *py_repomd, *py_warningcb;
    CbData cbdata;
    cr_Repomd *repomd;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sO!O:py_xml_parse_repomd",
                                         &filename,
                                         &Repomd_Type,
                                         &py_repomd,
                                         &py_warningcb)) {
        return NULL;
    }

    if (!PyCallable_Check(py_warningcb) && py_warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    Py_XINCREF(py_repomd);
    Py_XINCREF(py_warningcb);

    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (py_warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cbdata.py_newpkgcb  = NULL;
    cbdata.py_pkgcb     = NULL;
    cbdata.py_warningcb = py_warningcb;
    cbdata.py_pkgs      = NULL;

    repomd = Repomd_FromPyObject(py_repomd);

    cr_xml_parse_repomd(filename,
                       repomd,
                       ptr_c_warningcb,
                       &cbdata,
                       &tmp_err);

    Py_XDECREF(py_repomd);
    Py_XDECREF(py_warningcb);

    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
py_xml_parse_updateinfo(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    char *filename;
    PyObject *py_updateinfo, *py_warningcb;
    CbData cbdata;
    cr_UpdateInfo *updateinfo;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sO!O:py_xml_parse_updateinfo",
                                         &filename,
                                         &UpdateInfo_Type,
                                         &py_updateinfo,
                                         &py_warningcb)) {
        return NULL;
    }

    if (!PyCallable_Check(py_warningcb) && py_warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    Py_XINCREF(py_updateinfo);
    Py_XINCREF(py_warningcb);

    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (py_warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cbdata.py_newpkgcb  = NULL;
    cbdata.py_pkgcb     = NULL;
    cbdata.py_warningcb = py_warningcb;
    cbdata.py_pkgs      = NULL;

    updateinfo = UpdateInfo_FromPyObject(py_updateinfo);

    cr_xml_parse_updateinfo(filename,
                            updateinfo,
                            ptr_c_warningcb,
                            &cbdata,
                            &tmp_err);

    Py_XDECREF(py_updateinfo);
    Py_XDECREF(py_warningcb);

    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}


PyObject *
py_xml_parse_main_metadata_together(G_GNUC_UNUSED PyObject *self, PyObject *args, PyObject *kwargs)
{
    char *primary_filename;
    char *filelists_filename;
    char *other_filename;
    int allow_out_of_order = 1;
    PyObject *py_newpkgcb, *py_pkgcb, *py_warningcb;
    CbData cbdata;
    GError *tmp_err = NULL;
    static char *kwlist[] = { "primary", "filelists", "other", "newpkgcb", "pkgcb",
                              "warningcb", "allow_out_of_order", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sssOOO|p:py_xml_parse_main_metadata_together", kwlist,
                                     &primary_filename, &filelists_filename, &other_filename, &py_newpkgcb,
                                     &py_pkgcb, &py_warningcb, &allow_out_of_order)) {
        return NULL;
    }

    if (!PyCallable_Check(py_newpkgcb) && py_newpkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_pkgcb) && py_pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_warningcb) && py_warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    if (py_newpkgcb == Py_None && py_pkgcb == Py_None) {
        PyErr_SetString(PyExc_ValueError, "both pkgcb and newpkgcb cannot be None");
        return NULL;
    }

    Py_XINCREF(py_newpkgcb);
    Py_XINCREF(py_pkgcb);
    Py_XINCREF(py_warningcb);

    cr_XmlParserNewPkgCb    ptr_c_newpkgcb  = NULL;
    cr_XmlParserPkgCb       ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (py_newpkgcb != Py_None)
        ptr_c_newpkgcb = c_newpkgcb;
    if (py_pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (py_warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cbdata.py_newpkgcb  = py_newpkgcb;
    cbdata.py_pkgcb     = py_pkgcb;
    cbdata.py_warningcb = py_warningcb;
    cbdata.py_pkgs      = PyDict_New();

    cr_xml_parse_main_metadata_together(primary_filename,
                                        filelists_filename,
                                        other_filename,
                                        ptr_c_newpkgcb,
                                        &cbdata,
                                        ptr_c_pkgcb,
                                        &cbdata,
                                        ptr_c_warningcb,
                                        &cbdata,
                                        allow_out_of_order,
                                        &tmp_err);

    Py_XDECREF(py_newpkgcb);
    Py_XDECREF(py_pkgcb);
    Py_XDECREF(py_warningcb);
    Py_XDECREF(cbdata.py_pkgs);

    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}
