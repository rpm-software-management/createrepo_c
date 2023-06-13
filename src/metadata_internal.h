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

#ifndef __C_CREATEREPOLIB_METADATA_INTERNAL_H__
#define __C_CREATEREPOLIB_METADATA_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WITH_LIBMODULEMD
#include <modulemd.h>
#include "load_metadata.h"

/** Return module metadata from a cr_Metadata
 * @param md        cr_Metadata object.
 * @return          Pointer to internal ModulemdModuleIndex.
 */
ModulemdModuleIndex *cr_metadata_modulemd(cr_Metadata *md);

/** Load (compressed) module metadata file into moduleindex,
 * compression is autodetected.
 * @param moduleindex   memory adress where to store the
 *                      created pointer to ModulemdModuleIndex
 * @param path_to_md    path to module metadata
 * @return              cr_Error code
 */
int
cr_metadata_load_modulemd(ModulemdModuleIndex **moduleindex,
                          gchar *path_to_md,
                          GError **err);

/** Compress groupfile into dest_dir with specified compression.
 * @param groupfile     Path to local groupfile, it can be already compressed by
 *                      some compression.
 * @param dest_dir      Path to directory where the compressed groupfile should be stored.
 * @return              Path to the new compressed groupfile. Has to be freed by the caller.
 */
gchar *
cr_compress_groupfile(const char *groupfile,
                      const char *dest_dir,
                      cr_CompressionType compression);


#endif /* WITH_LIBMODULEMD */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_METADATA_INTERNAL_H__ */
