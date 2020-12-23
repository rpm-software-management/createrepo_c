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
#include <glib.h>
#include <glib/gprintf.h>
#include "exception-py.h"

PyObject *CrErr_Exception = NULL;

int
init_exceptions()
{
    CrErr_Exception = PyErr_NewExceptionWithDoc("createrepo_c.CreaterepoCError",
                            "Createrepo_c library exception", NULL, NULL);

    if (!CrErr_Exception)
        return 0;
    Py_INCREF(CrErr_Exception);

    return 1;
}

void
nice_exception(GError **err, const char *format, ...)
{
    int ret;
    va_list vl;
    gchar *message, *usr_message = NULL;
    PyObject *exception;

    if (format) {
        // Prepare user message
        va_start(vl, format);
        ret = g_vasprintf(&usr_message, format, vl);
        va_end(vl);

        if (ret < 0) {
            // vasprintf failed - silently ignore this error
            g_free(usr_message);
            usr_message = NULL;
        }
    }

    // Prepare whole error message
    if (usr_message)
        message = g_strdup_printf("%s%s", usr_message, (*err)->message);
    else
        message = g_strdup((*err)->message);

    g_free(usr_message);

    // Select appropriate exception
    switch ((*err)->code) {
        case CRE_IO:
        case CRE_STAT:
        case CRE_NOFILE:
        case CRE_NODIR:
        case CRE_EXISTS:
            exception = PyExc_IOError;
            break;
        case CRE_MEMORY:
            exception = PyExc_MemoryError;
            break;
        case CRE_BADARG:
            exception = PyExc_ValueError;
            break;
        default:
            exception = CrErr_Exception;
    }

    g_clear_error(err);

    // Set exception
    PyErr_SetString(exception, message);

    g_free(message);
}
