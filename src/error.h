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
    CRE_BADARG, /*!<
        (5) At least one argument of function is bad or non complete */
    CRE_NOFILE, /*!<
        (6) File doesn't exist */
    CRE_NODIR, /*!<
        (7) Directory doesn't exist (not a dir or path doesn't exist) */
    CRE_EXISTS, /*!<
        (8) File/Directory already exists */
    CRE_UNKNOWNCHECKSUMTYPE, /*!<
        (9) Unknown/Unsupported checksum type */
    CRE_UNKNOWNCOMPRESSION, /*!<
        (10) Unknown/Unsupported compression type */
    CRE_XMLPARSER, /*!<
        (11) XML parser error (non valid xml, corrupted xml,  ..) */
    CRE_XMLDATA, /*!<
        (12) Loaded xml metadata are bad */
    CRE_CBINTERRUPTED, /*!<
        (13) Interrupted by callback. */
    CRE_BADXMLPRIMARY, /*!<
        (14) Bad filelists.xml file */
    CRE_BADXMLFILELISTS, /*!<
        (15) Bad filelists.xml file */
    CRE_BADXMLOTHER, /*!<
        (16) Bad filelists.xml file */
    CRE_BADXMLREPOMD, /*!<
        (17) Bad repomd.xml file */
    CRE_MAGIC, /*!<
        (18) Magic Number Recognition error */
    CRE_GZ, /*!<
        (19) Gzip library related error */
    CRE_BZ2, /*!<
        (20) Bzip2 library related error */
    CRE_XZ, /*!<
        (21) Xz (lzma) library related error */
    CRE_OPENSSL, /*!<
        (22) OpenSSL library related error */
    CRE_CURL, /*!<
        (23) Curl library related error */
    CRE_ASSERT, /*!<
        (24) Ideally this error should never happened. Nevertheless if
        it happend, probable reason is that some values of createrepo_c
        object was changed (by you - a programmer) in a bad way */
    CRE_BADCMDARG, /*!<
        (25) Bad command line argument(s) */
    CRE_SPAWNERRCODE, /*!<
        (26) Child process exited with error code != 0 */
    CRE_SPAWNKILLED, /*!<
        (27) Child process killed by signal */
    CRE_SPAWNSTOPED, /*!<
        (28) Child process stopped by signal */
    CRE_SPAWNABNORMAL, /*!<
        (29) Child process exited abnormally */
    CRE_DELTARPM, /*!<
        (30) Deltarpm related error */
    CRE_BADXMLUPDATEINFO, /*!<
        (31) Bad updateinfo.xml file */
    CRE_SIGPROCMASK, /*!<
        (32) Cannot change blocked signals */
    CRE_ZCK, /*!<
        (33) ZCK library related error */
    CRE_MODULEMD, /*!<
        (34) modulemd related error */
    CRE_ZSTD, /*!<
        (35) Zstd library related error */
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
