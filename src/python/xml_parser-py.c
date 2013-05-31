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
#include "exception-py.h"

static int
c_newpkgcb(cr_Package **pkg,
           const char *pkgId,
           const char *name,
           const char *arch,
           void *cbdata,
           GError **err)
{
    PyObject *arglist, *result;

    arglist = Py_BuildValue("(sss)", pkgId, name, arch);
    result = PyObject_CallObject(cbdata, arglist);
    Py_DECREF(arglist);

    if (result == NULL) {
        // Exception raised
        PyErr_ToGError(err);
        return CR_CB_RET_ERR;
    }

    if (!PackageObject_Check(result) && result != Py_None) {
        PyErr_SetString(PyExc_TypeError,
            "Expected a cr_Package or None as a callback return value");
        return CR_CB_RET_ERR;
    }

    *pkg = Package_FromPyObject(result);

    Py_DECREF(result);
    return CR_CB_RET_OK;
}

static int
c_pkgcb(cr_Package *pkg,
        void *cbdata,
        GError **err)
{
    PyObject *result;

    CR_UNUSED(pkg);

    result = PyObject_CallObject(cbdata, NULL);

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

    arglist = Py_BuildValue("(is)", type, msg);
    result = PyObject_CallObject(cbdata, arglist);
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
py_xml_parse_primary(PyObject *self, PyObject *args)
{
    CR_UNUSED(self);

    char *filename;
    int do_files;
    PyObject *newpkgcb, *pkgcb, *warningcb;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sOOOi:py_xml_parse_primary",
                                         &filename,
                                         &newpkgcb,
                                         &pkgcb,
                                         &warningcb,
                                         &do_files)) {
        return NULL;
    }

    if (!PyCallable_Check(newpkgcb)) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable");
        return NULL;
    }

    if (!PyCallable_Check(pkgcb) && pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(warningcb) && warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    Py_XINCREF(newpkgcb);
    Py_XINCREF(pkgcb);
    Py_XINCREF(warningcb);

    cr_XmlParserPkgCb       ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cr_xml_parse_primary(filename,
                         c_newpkgcb,
                         newpkgcb,
                         ptr_c_pkgcb,
                         pkgcb,
                         ptr_c_warningcb,
                         warningcb,
                         do_files,
                         &tmp_err);

    Py_XDECREF(newpkgcb);
    Py_XDECREF(pkgcb);
    Py_XDECREF(warningcb);

    if (tmp_err) {
        PyErr_Format(CrErr_Exception, "%s", tmp_err->message);
        g_clear_error(&tmp_err);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
py_xml_parse_filelists(PyObject *self, PyObject *args)
{
    CR_UNUSED(self);

    char *filename;
    PyObject *newpkgcb, *pkgcb, *warningcb;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sOOO:py_xml_parse_filelists",
                                         &filename,
                                         &newpkgcb,
                                         &pkgcb,
                                         &warningcb)) {
        return NULL;
    }

    if (!PyCallable_Check(newpkgcb)) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable");
        return NULL;
    }

    if (!PyCallable_Check(pkgcb) && pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(warningcb) && warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    Py_XINCREF(newpkgcb);
    Py_XINCREF(pkgcb);
    Py_XINCREF(warningcb);

    cr_XmlParserPkgCb       ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cr_xml_parse_filelists(filename,
                           c_newpkgcb,
                           newpkgcb,
                           ptr_c_pkgcb,
                           pkgcb,
                           ptr_c_warningcb,
                           warningcb,
                           &tmp_err);

    Py_XDECREF(newpkgcb);
    Py_XDECREF(pkgcb);
    Py_XDECREF(warningcb);

    if (tmp_err) {
        PyErr_Format(CrErr_Exception, "%s", tmp_err->message);
        g_clear_error(&tmp_err);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
py_xml_parse_other(PyObject *self, PyObject *args)
{
    CR_UNUSED(self);

    char *filename;
    PyObject *newpkgcb, *pkgcb, *warningcb;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sOOO:py_xml_parse_other",
                                         &filename,
                                         &newpkgcb,
                                         &pkgcb,
                                         &warningcb)) {
        return NULL;
    }

    if (!PyCallable_Check(newpkgcb)) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable");
        return NULL;
    }

    if (!PyCallable_Check(pkgcb) && pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(warningcb) && warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    Py_XINCREF(newpkgcb);
    Py_XINCREF(pkgcb);
    Py_XINCREF(warningcb);

    cr_XmlParserPkgCb       ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cr_xml_parse_other(filename,
                       c_newpkgcb,
                       newpkgcb,
                       ptr_c_pkgcb,
                       pkgcb,
                       ptr_c_warningcb,
                       warningcb,
                       &tmp_err);

    Py_XDECREF(newpkgcb);
    Py_XDECREF(pkgcb);
    Py_XDECREF(warningcb);

    if (tmp_err) {
        PyErr_Format(CrErr_Exception, "%s", tmp_err->message);
        g_clear_error(&tmp_err);
        return NULL;
    }

    Py_RETURN_NONE;
}
