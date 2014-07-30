/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
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

#ifndef __C_CREATEREPOLIB_H__
#define __C_CREATEREPOLIB_H__

#ifdef __cplusplus
extern "C" {
#endif

/*! \mainpage createrepo_c library
 *
 * \section intro_sec Introduction
 *
 * Usage:
 * \code
 *   #include <createrepo_c/createrepo_c.h>
 * \endcode
 *
 */

/** \defgroup   main    Complete API of createrepo_c library
 */

#include <glib.h>
#include "checksum.h"
#include "compression_wrapper.h"
#include "deltarpms.h"
#include "error.h"
#include "load_metadata.h"
#include "locate_metadata.h"
#include "misc.h"
#include "package.h"
#include "parsehdr.h"
#include "parsepkg.h"
#include "repomd.h"
#include "sqlite.h"
#include "threads.h"
#include "updateinfo.h"
#include "version.h"
#include "xml_dump.h"
#include "xml_file.h"
#include "xml_parser.h"

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_H__ */
