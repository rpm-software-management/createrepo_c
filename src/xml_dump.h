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

#ifndef __C_CREATEREPOLIB_XML_DUMP_H__
#define __C_CREATEREPOLIB_XML_DUMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "package.h"
#include "repomd.h"

/** \defgroup   xml_dump        XML dump API.
 *
 * Example:
 * \code
 * cr_Package *pkg;
 * struct cr_XmlStruct xml;
 *
 * cr_xml_dump_init();
 * cr_package_parser_init();
 *
 * pkg = cr_package_from_rpm("path/to/rpm.rpm", CR_CHECKSUM_SHA256,
 *                           "repodata/rpm.rpm", NULL, 10, NULL);
 *
 * xml = cr_xml_dump(pkg, NULL);
 *
 * cr_package_free(pkg);
 *
 * printf("Primary XML chunk:\n%s\n", xml.primary);
 * printf("Filelists XML chunk:\n%s\n", xml.filelists);
 * printf("Other XML chunk:\n%s\n", xml.other);
 *
 * free(xml.primary);
 * free(xml.filelists);
 * free(xml.other);
 *
 * cr_package_parser_cleanup();
 * cr_xml_dump_cleanup();
 * \endcode
 *
 *  \addtogroup xml_dump
 *  @{
 */

/** Default namespace for primary.xml */
#define CR_XML_COMMON_NS            "http://linux.duke.edu/metadata/common"
/** Default namespace for filelists.xml */
#define CR_XML_FILELISTS_NS         "http://linux.duke.edu/metadata/filelists"
/** Default namespace for other.xml */
#define CR_XML_OTHER_NS             "http://linux.duke.edu/metadata/other"
/** Default namespace for repomd.xml */
#define CR_XML_REPOMD_NS            "http://linux.duke.edu/metadata/repo"
/** Namespace for rpm (used in primary.xml and repomd.xml) */
#define CR_XML_RPM_NS               "http://linux.duke.edu/metadata/rpm"


/** Xml chunks for primary.xml, filelists.xml and other.xml.
 */
struct cr_XmlStruct {
    char *primary;      /*!< XML chunk for primary.xml */
    char *filelists;    /*!< XML chunk for filelists.xml */
    char *other;        /*!< XML chunk for other.xml */
};

/** Initialize dumping part of library (Initialize libxml2).
 */
void cr_xml_dump_init();

/** Cleanup initialized dumping part of library
 */
void cr_xml_dump_cleanup();

/** Generate primary xml chunk from cr_Package.
 * @param package       cr_Package
 * @param err           **GError
 * @return              xml chunk string or NULL on error
 */
char *cr_xml_dump_primary(cr_Package *package, GError **err);

/** Generate filelists xml chunk from cr_Package.
 * @param package       cr_Package
 * @param err           **GError
 * @return              xml chunk string or NULL on error
 */
char *cr_xml_dump_filelists(cr_Package *package, GError **err);

/** Generate other xml chunk from cr_Package.
 * @param package       cr_Package
 * @param err           **GError
 * @return              xml chunk string or NULL on error
 */
char *cr_xml_dump_other(cr_Package *package, GError **err);

/** Generate all three xml chunks (primary, filelists, other) from cr_Package.
 * @param package       cr_Package
 * @param err           **GError
 * @return              cr_XmlStruct
 */
struct cr_XmlStruct cr_xml_dump(cr_Package *package, GError **err);

/** Generate xml representation of cr_Repomd.
 * @param repomd        cr_Repomd
 * @param err           **GError
 * @return              repomd.xml content
 */
char *cr_xml_dump_repomd(cr_Repomd *repomd, GError **err);

/** Prepare string to xml dump.
 * If string is not utf8 it is converted (source encoding is supposed to be
 * iso-8859-1).
 * Control chars (chars with value <32 except 9, 10 and 13) are excluded.
 *
 * @param in            input string.
 * @param out           output string. space of output string must be
 *                      at least (strlen(in) * 2 + 1) * sizeof(char)
 */
void cr_latin1_to_utf8(const unsigned char *in,
                       unsigned char *out)  __attribute__ ((hot));

/**
 * Check if string contains chars with value <32 (except 9, 10 and 13).
 *
 * @param str           String (NOT NULL!!!!)
 * @return              TRUE if at leas one char with value <32 (except the
 *                      9, 10, 13) is present in the string.
 */
gboolean hascontrollchars(const unsigned char *str);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_XML_DUMP_H__ */
