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

#include "typeconversion.h"
#include "exception-py.h"
#include "misc-py.h"
#include "contentstat-py.h"

PyObject *
py_compress_file_with_stat(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    int type;
    char *src, *dst;
    PyObject *py_contentstat = NULL;
    cr_ContentStat *contentstat;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sziO:py_compress_file", &src, &dst, &type,
                          &py_contentstat))
        return NULL;

    if (!py_contentstat || py_contentstat == Py_None) {
        contentstat = NULL;
    } else {
        contentstat = ContentStat_FromPyObject(py_contentstat);
        if (!contentstat)
            return NULL;
    }

    cr_compress_file_with_stat(src, dst, type, contentstat, NULL, FALSE, &tmp_err);
    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
py_decompress_file_with_stat(G_GNUC_UNUSED PyObject *self, PyObject *args)
{
    int type;
    char *src, *dst;
    PyObject *py_contentstat = NULL;
    cr_ContentStat *contentstat;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sziO:py_decompress_file", &src, &dst, &type,
                          &py_contentstat))
        return NULL;

    if (!py_contentstat || py_contentstat == Py_None) {
        contentstat = NULL;
    } else {
        contentstat = ContentStat_FromPyObject(py_contentstat);
        if (!contentstat)
            return NULL;
    }

    cr_decompress_file_with_stat(src, dst, type, contentstat, &tmp_err);
    if (tmp_err) {
        nice_exception(&tmp_err, NULL);
        return NULL;
    }

    Py_RETURN_NONE;
}
