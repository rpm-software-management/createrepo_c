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

#ifndef __C_CREATEREPOLIB_ERROR_H__
#define __C_CREATEREPOLIB_ERROR_H__

#include <glib.h>

typedef enum {
    CRE_OK,     /*!<
        No error */
    CRE_IO,     /*!<
        Input/Output error (cannot open file, etc.) */
    CRE_MEMORY, /*!<
        Cannot allocate memory */
    CRE_STAT, /*!<
        Stat() call failed */
    CRE_DB,     /*!<
        A database error */
    CRE_BADARG, /*!<
        At least one argument of function is bad or non complete */
    CRE_NOFILE, /*!<
        File doesn't exists */
    CRE_UNKNOWNCHECKSUMTYPE, /*!<
        Unknown/Unsupported checksum type */
} cr_Error;

#define CR_DB_ERROR cr_db_error_quark()
#define CR_XML_DUMP_PRIMARY_ERROR cr_xml_dump_primary_error_quark()
#define CR_XML_DUMP_FILELISTS_ERROR cr_xml_dump_filelists_error_quark()
#define CR_XML_DUMP_OTHER_ERROR cr_xml_dump_other_error_quark()
#define CR_REPOMD_ERROR cr_repomd_error_quark()
#define CR_REPOMD_RECORD_ERROR cr_repomd_record_error_quark()
#define CR_CHECKSUM_ERROR cr_checksum_error_quark()
#define CR_PARSEPKG_ERROR cr_parsepkg_error_quark()

GQuark cr_db_error_quark(void);
GQuark cr_xml_dump_primary_error_quark(void);
GQuark cr_xml_dump_filelists_error_quark(void);
GQuark cr_xml_dump_other_error_quark(void);
GQuark cr_repomd_error_quark(void);
GQuark cr_repomd_record_error_quark(void);
GQuark cr_checksum_error_quark(void);
GQuark cr_parsepkg_error_quark(void);

#endif
