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
    CRE_ZCK, /*!<
        (34) ZCK library related error */
    CRE_MODULEMD, /*!<
        (35) modulemd related error */
    CRE_SENTINEL, /*!<
        (XX) Sentinel */
} cr_Error;

/** Converts cr_Error return code to error string.
 * @param rc        cr_Error return code
 * @return          Error string
 */
const char *cr_strerror(cr_Error rc);

/* Error domains */
#define CREATEREPO_C_ERROR              createrepo_c_error_quark()

GQuark createrepo_c_error_quark(void);

#endif
