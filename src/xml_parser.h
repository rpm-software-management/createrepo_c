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

#ifndef __C_CREATEREPOLIB_XML_PARSER_H__
#define __C_CREATEREPOLIB_XML_PARSER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "package.h"

/** \defgroup   xml_parser        XML parser API.
 *  \addtogroup xml_parser
 *  @{
 */

/** Callback for XML parser wich is called when a package element is parsed.
 * @param pkg       Currently parsed package.
 * @param cbdata    User data.
 * @err             GError **
 * @return          0 - OK, 1 - ERROR (stops the parsing)
 */
typedef int (*cr_XmlParserPkgCb)(cr_Package *pkg,
                                 void *cbdata,
                                 GError **err);

/** Callback for XML parser wich is called when a new package object parsing
 * is started. This function has to set *pkg to package object which will
 * be populated by parser. The object could be empty, or already partially
 * filled (by other XML parsers) package object.
 * If the pointer is set to NULL, current package will be skiped.
 * Note: For the primary.xml file pkgId, name and arch are NULL!
 * @param pkg           Package that will be populated.
 * @param pkgId         pkgId (hash) of the new package.
 * @param name          Name of the new package.
 * @param arch          Arch of the new package.
 * @param cbdata        User data.
 * @param err           GError **
 * @return              0 - OK, 1 - ERR (stops the parsing)
 */
typedef int (*cr_XmlParserNewPkgCb)(cr_Package **pkg,
                                    const char *pkgId,
                                    const char *name,
                                    const char *arch,
                                    void *cbdata,
                                    GError **err);

/** Parse filelists.xml. File could be compressed.
 * @param path          Path to filelists.xml (plain or compressed)
 * @param newpkgcb      Callback for new package (Called when new package
 *                      xml chunk is found and package object to store
 *                      the data is needed). If NULL cr_newpkgcb is used.
 * @param newpkgcb_data User data for the newpkgcb.
 * @param pkgcb         Package callback. (Called when complete package
 *                      xml chunk is parsed.). Could be NULL if newpkgcb is
 *                      not NULL.
 * @param pkgcb_data    User data for the pkgcb.
 * @param messages      Pointer to char* where messages (warnings)
 *                      from parsing will be stored.
 * @param err           GError **
 * @return              cr_Error code.
 */
int cr_xml_parse_filelists(const char *path,
                           cr_XmlParserNewPkgCb newpkgcb,
                           void *newpkgcb_data,
                           cr_XmlParserPkgCb pkgcb,
                           void *pkgcb_data,
                           char **messages,
                           GError **err);
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_XML_PARSER_H__ */
