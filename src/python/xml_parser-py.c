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

    if (result->ob_refcnt == 1) {
        *pkg = NULL;
    }
    Py_DECREF(result);
    return CR_CB_RET_OK;
}

static int
c_pkgcb(cr_Package *pkg,
        void *cbdata,
        GError **err)
{
    // destroys "pkg"
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

typedef struct {
    PyObject_HEAD
    cr_PkgIterator *pkg_iterator;
    CbData *cbdata;
} _PkgIteratorObject;

cr_PkgIterator *
PkgIterator_FromPyObject(PyObject *o)
{
    if (!PkgIteratorObject_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "Expected a createrepo_c.PkgIterator object.");
        return NULL;
    }
    return ((_PkgIteratorObject *)o)->pkg_iterator;
}

PyObject *
Object_FromPkgIterator(cr_PkgIterator *pkg_iterator)
{
    if (!pkg_iterator) {
        PyErr_SetString(PyExc_ValueError, "Expected a cr_PkgIterator pointer not NULL.");
        return NULL;
    }

    PyObject *py_pkg_iterator = PyObject_CallObject((PyObject *)&PkgIterator_Type, NULL);
    ((_PkgIteratorObject *)py_pkg_iterator)->pkg_iterator = pkg_iterator;
    return py_pkg_iterator;
}

static int
check_PkgIteratorStatus(const _PkgIteratorObject *self)
{
    assert(self != NULL);
    assert(PkgIteratorObject_Check(self));
    if (self->pkg_iterator == NULL) {
        PyErr_SetString(CrErr_Exception, "Improper createrepo_c PkgIterator object.");
        return -1;
    }
    return 0;
}

/* Function on the type */

static PyObject *
pkg_iterator_new(PyTypeObject *type,
            G_GNUC_UNUSED PyObject *args,
            G_GNUC_UNUSED PyObject *kwds)
{
    _PkgIteratorObject *self = (_PkgIteratorObject *)type->tp_alloc(type, 0);
    if (self) {
        self->pkg_iterator = NULL;
        self->cbdata = g_malloc0(sizeof(CbData));
    }
    return (PyObject *)self;
}

PyDoc_STRVAR(pkg_iterator_init__doc__,
             "PkgIterator object\n\n"
             ".. method:: __init__()\n\n"
             "    Default constructor\n");

static int
pkg_iterator_init(_PkgIteratorObject *self, PyObject *args, PyObject *kwargs)
{
    char *primary_path;
    char *filelists_path;
    char *other_path;
    PyObject *py_newpkgcb, *py_warningcb;
    GError *tmp_err = NULL;
    static char *kwlist[] = {"primary", "filelists", "other", "newpkgcb",
                             "warningcb", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "szzOO:pkg_iterator_init", kwlist,
                                     &primary_path, &filelists_path, &other_path, &py_newpkgcb,
                                     &py_warningcb)) {
        return -1;
    }

    if (!primary_path) {
        PyErr_SetString(PyExc_TypeError, "primary file path must be provided");
        return -1;
    }

    if (!PyCallable_Check(py_newpkgcb) && py_newpkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable or None");
        return -1;
    }

    if (!PyCallable_Check(py_warningcb) && py_warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return -1;
    }

    if (self->pkg_iterator) // reinitialization by __init__()
        cr_PkgIterator_free(self->pkg_iterator, &tmp_err);
    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return -1;
    }

    Py_XINCREF(py_newpkgcb);
    Py_XINCREF(py_warningcb);

    cr_XmlParserNewPkgCb ptr_c_newpkgcb = NULL;
    cr_XmlParserWarningCb ptr_c_warningcb = NULL;

    if (py_newpkgcb != Py_None)
        ptr_c_newpkgcb = c_newpkgcb;
    if (py_warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    self->cbdata->py_newpkgcb = py_newpkgcb;
    self->cbdata->py_pkgcb = NULL;
    self->cbdata->py_warningcb = py_warningcb;
    self->cbdata->py_pkgs = PyDict_New();

    self->pkg_iterator = cr_PkgIterator_new(
        primary_path, filelists_path, other_path, ptr_c_newpkgcb, self->cbdata, ptr_c_warningcb, self->cbdata, &tmp_err);

    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return -1;
    }

    if (self->pkg_iterator == NULL) {
        PyErr_SetString(CrErr_Exception, "PkgIterator initialization failed");
        return -1;
    }

    return 0;
}

static void
pkg_iterator_dealloc(_PkgIteratorObject *self)
{
    GError *tmp_err;
    if (self->pkg_iterator) {
        cr_PkgIterator_free(self->pkg_iterator, &tmp_err);
    }
    if (self->cbdata) {
        Py_XDECREF(self->cbdata->py_newpkgcb);
        Py_XDECREF(self->cbdata->py_warningcb);
        Py_XDECREF(self->cbdata->py_pkgs);

        free(self->cbdata);
    }

    Py_TYPE(self)->tp_free(self);
}

static PyObject *
pkg_iterator_next_package(PyObject *PyObject_self)
{
    _PkgIteratorObject *self = (_PkgIteratorObject *) PyObject_self;
    cr_Package *pkg;
    GError *tmp_err = NULL;

    if (check_PkgIteratorStatus(self)) {
        return NULL;
    }
    pkg = cr_PkgIterator_parse_next(self->pkg_iterator, &tmp_err);
    if (tmp_err) {
        cr_package_free(pkg);
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    if (!pkg) {
        assert(cr_PkgIterator_is_finished(self->pkg_iterator));
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    PyObject *keyFromPtr = PyLong_FromVoidPtr(pkg);
    PyObject *py_pkg = PyDict_GetItem(self->cbdata->py_pkgs, keyFromPtr);
    if (py_pkg) {
        // Remove pkg from PyDict but keep one reference so its not freed if the
        // user doesn't have any references to the package
        Py_XINCREF(py_pkg);
        PyDict_DelItem(self->cbdata->py_pkgs, keyFromPtr);
    } else {
        // The package was not provided by user in c_newpkgcb,
        // create new python package object
        py_pkg = Object_FromPackage(pkg, 1);
    }
    Py_DECREF(keyFromPtr);
    return py_pkg;

}

static PyObject *
pkg_iterator_is_finished(_PkgIteratorObject *self, G_GNUC_UNUSED void *nothing) {
    if (check_PkgIteratorStatus(self))
        return NULL;

    if (cr_PkgIterator_is_finished (self->pkg_iterator)) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

PyDoc_STRVAR(pkg_iterator_is_finished__doc__,
             "Whether the package iterator has been consumed. \n\n"
             ".. method:: is_finished()\n\n"
             "    Whether the iterator is consumed.\n");

static struct PyMethodDef pkg_iterator_methods[] = {
    {"is_finished", (PyCFunction) pkg_iterator_is_finished, METH_NOARGS, pkg_iterator_is_finished__doc__},
    {NULL, NULL, 0, NULL} /* sentinel */
};

/* Object */

PyTypeObject PkgIterator_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = "createrepo_c.PkgIterator",
    .tp_basicsize = sizeof(_PkgIteratorObject),
    .tp_dealloc = (destructor) pkg_iterator_dealloc,
    .tp_repr = 0, // (reprfunc) pkg_iterator_repr,
    .tp_str = 0, //(reprfunc) pkg_iterator_str,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = pkg_iterator_init__doc__,
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = pkg_iterator_next_package,
    .tp_methods = pkg_iterator_methods,
    .tp_init = (initproc) pkg_iterator_init,
    .tp_new = pkg_iterator_new,
};
