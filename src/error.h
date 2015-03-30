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

/* Error codes */
typedef enum {
    CRE_OK,     /*!<
        (0) No error */
    CRE_ERROR, /*!<
        (1) No specified error */
    CRE_IO,     /*!<
        (2) Input/Output error (cannot open file, etc.) */
    CRE_MEMORY, /*!<
        (3) Cannot allocate memory */
    CRE_STAT, /*!<
        (4) Stat() call failed */
    CRE_DB,     /*!<
        (5) A database error */
    CRE_BADARG, /*!<
        (6) At least one argument of function is bad or non complete */
    CRE_NOFILE, /*!<
        (7) File doesn't exist */
    CRE_NODIR, /*!<
        (8) Directory doesn't exist (not a dir or path doesn't exists) */
    CRE_EXISTS, /*!<
        (9) File/Directory already exists */
    CRE_UNKNOWNCHECKSUMTYPE, /*!<
        (10) Unknown/Unsupported checksum type */
    CRE_UNKNOWNCOMPRESSION, /*!<
        (11) Unknown/Unsupported compression type */
    CRE_XMLPARSER, /*!<
        (12) XML parser error (non valid xml, corrupted xml,  ..) */
    CRE_XMLDATA, /*!<
        (13) Loaded xml metadata are bad */
    CRE_CBINTERRUPTED, /*!<
        (14) Interrupted by callback. */
    CRE_BADXMLPRIMARY, /*!<
        (15) Bad filelists.xml file */
    CRE_BADXMLFILELISTS, /*!<
        (16) Bad filelists.xml file */
    CRE_BADXMLOTHER, /*!<
        (17) Bad filelists.xml file */
    CRE_BADXMLREPOMD, /*!<
        (18) Bad repomd.xml file */
    CRE_MAGIC, /*!<
        (19) Magic Number Recognition Library (libmagic) error */
    CRE_GZ, /*!<
        (20) Gzip library related error */
    CRE_BZ2, /*!<
        (21) Bzip2 library related error */
    CRE_XZ, /*!<
        (22) Xz (lzma) library related error */
    CRE_OPENSSL, /*!<
        (23) OpenSSL library related error */
    CRE_CURL, /*!<
        (24) Curl library related error */
    CRE_ASSERT, /*!<
        (25) Ideally this error should never happend. Nevertheless if
        it happend, probable reason is that some values of createrepo_c
        object was changed (by you - a programmer) in a bad way */
    CRE_BADCMDARG, /*!<
        (26) Bad command line argument(s) */
    CRE_SPAWNERRCODE, /*!<
        (27) Child process exited with error code != 0 */
    CRE_SPAWNKILLED, /*!<
        (28) Child process killed by signal */
    CRE_SPAWNSTOPED, /*!<
        (29) Child process stopped by signal */
    CRE_SPAWNABNORMAL, /*!<
        (30) Child process exited abnormally */
    CRE_DELTARPM, /*!<
        (31) Deltarpm related error */
    CRE_BADXMLUPDATEINFO, /*!<
        (32) Bad updateinfo.xml file */
    CRE_SIGPROCMASK, /*!<
        (33) Cannot change blocked signals */
    CRE_SENTINEL, /*!<
        (XX) Sentinel */
} cr_Error;

/** Converts cr_Error return code to error string.
 * @param rc        cr_Error return code
 * @return          Error string
 */
const char *cr_strerror(cr_Error rc);

/* Error domains */
#define CR_CREATEREPO_C_ERROR           cr_createrepo_c_error_quark()
#define CR_CHECKSUM_ERROR               cr_checksum_error_quark()
#define CR_CMD_ERROR                    cr_cmd_error_quark()
#define CR_COMPRESSION_WRAPPER_ERROR    cr_compression_wrapper_error_quark()
#define CR_DB_ERROR                     cr_db_error_quark()
#define CR_DELTARPMS_ERROR              cr_deltarpms_error_quark()
#define CR_HELPER_ERROR                 cr_helper_error_quark()
#define CR_LOAD_METADATA_ERROR          cr_load_metadata_error_quark()
#define CR_LOCATE_METADATA_ERROR        cr_locate_metadata_error_quark()
#define CR_MISC_ERROR                   cr_misc_error_quark()
#define CR_MODIFYREPO_ERROR             cr_modifyrepo_error_quark()
#define CR_PARSEPKG_ERROR               cr_parsepkg_error_quark()
#define CR_REPOMD_ERROR                 cr_repomd_error_quark()
#define CR_REPOMD_RECORD_ERROR          cr_repomd_record_error_quark()
#define CR_SQLITEREPO_ERROR             cr_sqliterepo_error_quark()
#define CR_THREADS_ERROR                cr_threads_error_quark()
#define CR_XML_DUMP_FILELISTS_ERROR     cr_xml_dump_filelists_error_quark()
#define CR_XML_DUMP_OTHER_ERROR         cr_xml_dump_other_error_quark()
#define CR_XML_DUMP_PRIMARY_ERROR       cr_xml_dump_primary_error_quark()
#define CR_XML_DUMP_REPOMD_ERROR        cr_xml_dump_repomd_error_quark()
#define CR_XML_FILE_ERROR               cr_xml_file_error_quark()
#define CR_XML_PARSER_ERROR             cr_xml_parser_error_quark()
#define CR_XML_PARSER_FIL_ERROR         cr_xml_parser_fil_error_quark()
#define CR_XML_PARSER_OTH_ERROR         cr_xml_parser_oth_error_quark()
#define CR_XML_PARSER_PRI_ERROR         cr_xml_parser_pri_error_quark()
#define CR_XML_PARSER_REPOMD_ERROR      cr_xml_parser_repomd_error_quark()
#define CR_XML_PARSER_UPDATEINFO_ERROR  cr_xml_parser_updateinfo_error_quark()

GQuark cr_createrepo_c_error_quark(void);
GQuark cr_checksum_error_quark(void);
GQuark cr_cmd_error_quark(void);
GQuark cr_compression_wrapper_error_quark(void);
GQuark cr_db_error_quark(void);
GQuark cr_deltarpms_error_quark(void);
GQuark cr_helper_error_quark(void);
GQuark cr_load_metadata_error_quark(void);
GQuark cr_locate_metadata_error_quark(void);
GQuark cr_misc_error_quark(void);
GQuark cr_modifyrepo_error_quark(void);
GQuark cr_parsepkg_error_quark(void);
GQuark cr_repomd_error_quark(void);
GQuark cr_repomd_record_error_quark(void);
GQuark cr_sqliterepo_error_quark(void);
GQuark cr_threads_error_quark(void);
GQuark cr_xml_dump_filelists_error_quark(void);
GQuark cr_xml_dump_other_error_quark(void);
GQuark cr_xml_dump_primary_error_quark(void);
GQuark cr_xml_dump_repomd_error_quark(void);
GQuark cr_xml_file_error_quark(void);
GQuark cr_xml_parser_error_quark(void);
GQuark cr_xml_parser_fil_error_quark(void);
GQuark cr_xml_parser_oth_error_quark(void);
GQuark cr_xml_parser_pri_error_quark(void);
GQuark cr_xml_parser_repomd_error_quark(void);
GQuark cr_xml_parser_updateinfo_error_quark(void);

#endif
