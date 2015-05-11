/* createrepo_c - Library of routines for manipulation with repodata
 *
 * Copyright (C) 2012 Colin Walters <walters@verbum.org>.
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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

/* This file was taken from libhif (https://github.com/hughsie/libhif) */

#ifndef __CR_CLEANUP_H__
#define __CR_CLEANUP_H__

#include <stdio.h>
#include <glib.h>
#include <unistd.h>

G_BEGIN_DECLS

static void
my_close(int fildes)
{
    if (fildes < 0)
        return;
    close(fildes);
}

#define CR_DEFINE_CLEANUP_FUNCTION(Type, name, func) \
  static inline void name (void *v) \
  { \
    func (*(Type*)v); \
  }

#define CR_DEFINE_CLEANUP_FUNCTION0(Type, name, func) \
  static inline void name (void *v) \
  { \
    if (*(Type*)v) \
      func (*(Type*)v); \
  }

#define CR_DEFINE_CLEANUP_FUNCTIONt(Type, name, func) \
  static inline void name (void *v) \
  { \
    if (*(Type*)v) \
      func (*(Type*)v, TRUE); \
  }

CR_DEFINE_CLEANUP_FUNCTION0(FILE*, cr_local_file_fclose, fclose)
CR_DEFINE_CLEANUP_FUNCTION0(GArray*, cr_local_array_unref, g_array_unref)
CR_DEFINE_CLEANUP_FUNCTION0(GChecksum*, cr_local_checksum_free, g_checksum_free)
CR_DEFINE_CLEANUP_FUNCTION0(GDir*, cr_local_dir_close, g_dir_close)
CR_DEFINE_CLEANUP_FUNCTION0(GError*, cr_local_free_error, g_error_free)
CR_DEFINE_CLEANUP_FUNCTION0(GHashTable*, cr_local_hashtable_unref, g_hash_table_unref)
CR_DEFINE_CLEANUP_FUNCTION0(GKeyFile*, cr_local_keyfile_free, g_key_file_free)
#if GLIB_CHECK_VERSION(2, 32, 0)
CR_DEFINE_CLEANUP_FUNCTION0(GKeyFile*, cr_local_keyfile_unref, g_key_file_unref)
#endif
CR_DEFINE_CLEANUP_FUNCTION0(GPtrArray*, cr_local_ptrarray_unref, g_ptr_array_unref)
CR_DEFINE_CLEANUP_FUNCTION0(GTimer*, cr_local_destroy_timer, g_timer_destroy)

CR_DEFINE_CLEANUP_FUNCTIONt(GString*, cr_local_free_string, g_string_free)

CR_DEFINE_CLEANUP_FUNCTION(char**, cr_local_strfreev, g_strfreev)
CR_DEFINE_CLEANUP_FUNCTION(GList*, cr_local_free_list, g_list_free)
CR_DEFINE_CLEANUP_FUNCTION(void*, cr_local_free, g_free)
CR_DEFINE_CLEANUP_FUNCTION(int, cr_local_file_close, my_close)

#define _cleanup_array_unref_ __attribute__ ((cleanup(cr_local_array_unref)))
#define _cleanup_checksum_free_ __attribute__ ((cleanup(cr_local_checksum_free)))
#define _cleanup_dir_close_ __attribute__ ((cleanup(cr_local_dir_close)))
#define _cleanup_error_free_ __attribute__ ((cleanup(cr_local_free_error)))
#define _cleanup_file_close_ __attribute__ ((cleanup(cr_local_file_close)))
#define _cleanup_file_fclose_ __attribute__ ((cleanup(cr_local_file_fclose)))
#define _cleanup_free_ __attribute__ ((cleanup(cr_local_free)))
#define _cleanup_hashtable_unref_ __attribute__ ((cleanup(cr_local_hashtable_unref)))
#define _cleanup_keyfile_free_ __attribute__ ((cleanup(cr_local_keyfile_free)))
#define _cleanup_keyfile_unref_ __attribute__ ((cleanup(cr_local_keyfile_unref)))
#define _cleanup_list_free_ __attribute__ ((cleanup(cr_local_free_list)))
#define _cleanup_ptrarray_unref_ __attribute__ ((cleanup(cr_local_ptrarray_unref)))
#define _cleanup_string_free_ __attribute__ ((cleanup(cr_local_free_string)))
#define _cleanup_strv_free_ __attribute__ ((cleanup(cr_local_strfreev)))
#define _cleanup_timer_destroy_ __attribute__ ((cleanup(cr_local_destroy_timer)))

G_END_DECLS

#endif
