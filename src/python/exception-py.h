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

#ifndef CR_EXCEPTION_PY_H
#define CR_EXCEPTION_PY_H

#include "src/createrepo_c.h"

extern PyObject *CrErr_Exception;

int init_exceptions();

/* Set exception by its return code (e.g., for CRE_IO, CRE_NOFILE, etc. will
 * be used a build-in python IOError exception) and free the GError.
 * @param err           GError **, must be !=  NULL
 * @param format        Prefix for the error message.
 */
void nice_exception(GError **err, const char *format, ...);

#endif
