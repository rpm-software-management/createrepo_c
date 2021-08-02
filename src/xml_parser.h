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

#include <glib.h>
#include "package.h"
#include "repomd.h"
#include "updateinfo.h"

/** \defgroup   xml_parser        XML parser API.
 *  \addtogroup xml_parser
 *  @{
 */

#define CR_CB_RET_OK    0 /*!< Return value for callbacks signalizing success */
#define CR_CB_RET_ERR   1 /*!< Return value for callbacks signalizing error */

/** Type of warnings reported by parsers by the warning callback.
 */
typedef enum {
    CR_XML_WARNING_UNKNOWNTAG,  /*!< Unknown tag */
    CR_XML_WARNING_MISSINGATTR, /*!< Missing attribute */
    CR_XML_WARNING_UNKNOWNVAL,  /*!< Unknown tag or attribute value */
    CR_XML_WARNING_BADATTRVAL,  /*!< Bad attribute value */
    CR_XML_WARNING_MISSINGVAL,  /*!< Missing tag value */
    CR_XML_WARNING_BADMDTYPE,   /*!< Bad metadata type (expected mandatory tag was not found) */
    CR_XML_WARNING_SENTINEL,
} cr_XmlParserWarningType;

/** Callback for XML parser which is called when a new package object parsing
 * is started. This function has to set *pkg to package object which will
 * be populated by parser. The object could be empty, or already partially
 * filled (by other XML parsers) package object.
 * If the pointer is set to NULL, current package will be skiped.
 * Note: For the primary.xml file pkgId, name and arch are NULL!
 * @param pkg       Package that will be populated.
 * @param pkgId     pkgId (hash) of the new package (in case of filelists.xml
 *                  or other.xml) or package type ("rpm" in case
 *                  of primary.xml).
 * @param name      Name of the new package.
 * @param arch      Arch of the new package.
 * @param cbdata    User data.
 * @param err       GError **
 * @return          CR_CB_RET_OK (0) or CR_CB_RET_ERR (1) - stops the parsing
 */
typedef int (*cr_XmlParserNewPkgCb)(cr_Package **pkg,
                                    const char *pkgId,
                                    const char *name,
                                    const char *arch,
                                    void *cbdata,
                                    GError **err);

/** Callback for XML parser which is called when a package element is parsed.
 * @param pkg       Currently parsed package.
 * @param cbdata    User data.
 * @param err       GError **
 * @return          CR_CB_RET_OK (0) or CR_CB_RET_ERR (1) - stops the parsing
 */
typedef int (*cr_XmlParserPkgCb)(cr_Package *pkg,
                                 void *cbdata,
                                 GError **err);

/** Callback for XML parser warnings. All reported warnings are non-fatal,
 * and ignored by default. But if callback return CR_CB_RET_ERR instead of
 * CR_CB_RET_OK then parsing is immediately interrupted.
 * @param type      Type of warning
 * @param msg       Warning msg. The message is destroyed after the call.
 *                  If you want to use the message later, you have to copy it.
 * @param cbdata    User data.
 * @param err       GError **
 * @return          CR_CB_RET_OK (0) or CR_CB_RET_ERR (1) - stops the parsing
 */
typedef int (*cr_XmlParserWarningCb)(cr_XmlParserWarningType type,
                                     char *msg,
                                     void *cbdata,
                                     GError **err);

/** Parse primary.xml. File could be compressed.
 * @param path           Path to filelists.xml
 * @param newpkgcb       Callback for new package (Called when new package
 *                       xml chunk is found and package object to store
 *                       the data is needed). If NULL cr_newpkgcb is used.
 * @param newpkgcb_data  User data for the newpkgcb.
 * @param pkgcb          Package callback. (Called when complete package
 *                       xml chunk is parsed.). Could be NULL if newpkgcb is
 *                       not NULL.
 * @param pkgcb_data     User data for the pkgcb.
 * @param warningcb      Callback for warning messages.
 * @param warningcb_data User data for the warningcb.
 * @param do_files       0 - Ignore file tags in primary.xml.
 * @param err            GError **
 * @return               cr_Error code.
 */
int cr_xml_parse_primary(const char *path,
                         cr_XmlParserNewPkgCb newpkgcb,
                         void *newpkgcb_data,
                         cr_XmlParserPkgCb pkgcb,
                         void *pkgcb_data,
                         cr_XmlParserWarningCb warningcb,
                         void *warningcb_data,
                         int do_files,
                         GError **err);

/** Parse string snippet of primary xml repodata. Snippet cannot contain
 * root xml element <metadata>. It contains only <package> elemetns.
 * @param xml_string     String containg primary xml data
 * @param newpkgcb       Callback for new package (Called when new package
 *                       xml chunk is found and package object to store
 *                       the data is needed). If NULL cr_newpkgcb is used.
 * @param newpkgcb_data  User data for the newpkgcb.
 * @param pkgcb          Package callback. (Called when complete package
 *                       xml chunk is parsed.). Could be NULL if newpkgcb is
 *                       not NULL.
 * @param pkgcb_data     User data for the pkgcb.
 * @param warningcb      Callback for warning messages.
 * @param warningcb_data User data for the warningcb.
 * @param do_files       0 - Ignore file tags in primary.xml.
 * @param err            GError **
 * @return               cr_Error code.
 */
int cr_xml_parse_primary_snippet(const char *xml_string,
                                 cr_XmlParserNewPkgCb newpkgcb,
                                 void *newpkgcb_data,
                                 cr_XmlParserPkgCb pkgcb,
                                 void *pkgcb_data,
                                 cr_XmlParserWarningCb warningcb,
                                 void *warningcb_data,
                                 int do_files,
                                 GError **err);

/** Parse filelists.xml. File could be compressed.
 * @param path           Path to filelists.xml
 * @param newpkgcb       Callback for new package (Called when new package
 *                       xml chunk is found and package object to store
 *                       the data is needed). If NULL cr_newpkgcb is used.
 * @param newpkgcb_data  User data for the newpkgcb.
 * @param pkgcb          Package callback. (Called when complete package
 *                       xml chunk is parsed.). Could be NULL if newpkgcb is
 *                       not NULL.
 * @param pkgcb_data     User data for the pkgcb.
 * @param warningcb      Callback for warning messages.
 * @param warningcb_data User data for the warningcb.
 * @param err            GError **
 * @return               cr_Error code.
 */
int cr_xml_parse_filelists(const char *path,
                           cr_XmlParserNewPkgCb newpkgcb,
                           void *newpkgcb_data,
                           cr_XmlParserPkgCb pkgcb,
                           void *pkgcb_data,
                           cr_XmlParserWarningCb warningcb,
                           void *warningcb_data,
                           GError **err);

/** Parse string snippet of filelists xml repodata. Snippet cannot contain
 * root xml element <filelists>. It contains only <package> elemetns.
 * @param xml_string     String containg filelists xml data
 * @param newpkgcb       Callback for new package (Called when new package
 *                       xml chunk is found and package object to store
 *                       the data is needed). If NULL cr_newpkgcb is used.
 * @param newpkgcb_data  User data for the newpkgcb.
 * @param pkgcb          Package callback. (Called when complete package
 *                       xml chunk is parsed.). Could be NULL if newpkgcb is
 *                       not NULL.
 * @param pkgcb_data     User data for the pkgcb.
 * @param warningcb      Callback for warning messages.
 * @param warningcb_data User data for the warningcb.
 * @param err            GError **
 * @return               cr_Error code.
 */
int cr_xml_parse_filelists_snippet(const char *xml_string,
                                   cr_XmlParserNewPkgCb newpkgcb,
                                   void *newpkgcb_data,
                                   cr_XmlParserPkgCb pkgcb,
                                   void *pkgcb_data,
                                   cr_XmlParserWarningCb warningcb,
                                   void *warningcb_data,
                                   GError **err);

/** Parse other.xml. File could be compressed.
 * @param path           Path to other.xml
 * @param newpkgcb       Callback for new package (Called when new package
 *                       xml chunk is found and package object to store
 *                       the data is needed). If NULL cr_newpkgcb is used.
 * @param newpkgcb_data  User data for the newpkgcb.
 * @param pkgcb          Package callback. (Called when complete package
 *                       xml chunk is parsed.). Could be NULL if newpkgcb is
 *                       not NULL.
 * @param pkgcb_data     User data for the pkgcb.
 * @param warningcb      Callback for warning messages.
 * @param warningcb_data User data for the warningcb.
 * @param err            GError **
 * @return               cr_Error code.
 */
int cr_xml_parse_other(const char *path,
                       cr_XmlParserNewPkgCb newpkgcb,
                       void *newpkgcb_data,
                       cr_XmlParserPkgCb pkgcb,
                       void *pkgcb_data,
                       cr_XmlParserWarningCb warningcb,
                       void *warningcb_data,
                       GError **err);

/** Parse string snippet of other xml repodata. Snippet cannot contain
 * root xml element <otherdata>. It contains only <package> elemetns.
 * @param xml_string     String containg other xml data
 * @param newpkgcb       Callback for new package (Called when new package
 *                       xml chunk is found and package object to store
 *                       the data is needed). If NULL cr_newpkgcb is used.
 * @param newpkgcb_data  User data for the newpkgcb.
 * @param pkgcb          Package callback. (Called when complete package
 *                       xml chunk is parsed.). Could be NULL if newpkgcb is
 *                       not NULL.
 * @param pkgcb_data     User data for the pkgcb.
 * @param warningcb      Callback for warning messages.
 * @param warningcb_data User data for the warningcb.
 * @param err            GError **
 * @return               cr_Error code.
 */
int cr_xml_parse_other_snippet(const char *xml_string,
                               cr_XmlParserNewPkgCb newpkgcb,
                               void *newpkgcb_data,
                               cr_XmlParserPkgCb pkgcb,
                               void *pkgcb_data,
                               cr_XmlParserWarningCb warningcb,
                               void *warningcb_data,
                               GError **err);

/** Parse repomd.xml. File could be compressed.
 * @param path           Path to repomd.xml
 * @param repomd         cr_Repomd object.
 * @param warningcb      Callback for warning messages.
 * @param warningcb_data User data for the warningcb.
 * @param err            GError **
 * @return               cr_Error code.
 */
int
cr_xml_parse_repomd(const char *path,
                    cr_Repomd *repomd,
                    cr_XmlParserWarningCb warningcb,
                    void *warningcb_data,
                    GError **err);

/** Parse updateinfo.xml. File could be compressed.
 * @param path           Path to updateinfo.xml
 * @param updateinfo     cr_UpdateInfo object.
 * @param warningcb      Callback for warning messages.
 * @param warningcb_data User data for the warningcb.
 * @param err            GError **
 * @return               cr_Error code.
 */
int
cr_xml_parse_updateinfo(const char *path,
                        cr_UpdateInfo *updateinfo,
                        cr_XmlParserWarningCb warningcb,
                        void *warningcb_data,
                        GError **err);

/** Parse all 3 main metadata types (primary, filelists and other) at the same time.
 * Once a package is fully parsed pkgcb is called which transfers ownership of the package
 * to the user, cr_xml_parse_main_metadata_together no longer needs it and it can be freed.
 * This means we don't have store all the packages in memory at the same time, which
 * significantly reduces the memory footprint.
 * Input metadata files can be compressed.
 * @param primary_path       Path to a primary xml file.
 * @param filelists_path     Path to a filelists xml file.
 * @param other_path         Path to an other xml file.
 * @param newpkgcb           Callback for a new package. Called when the new package
 *                           xml chunk is found and a package object to store
 *                           the data is needed.
 * @param newpkgcb_data      User data for the newpkgcb.
 * @param pkgcb              Package callback. Called when a package is completely
 *                           parsed containing information from all 3 main metadata
 *                           files. Could be NULL if newpkgcb is not NULL.
 * @param pkgcb_data         User data for the pkgcb.
 * @param warningcb          Callback for warning messages.
 * @param warningcb_data     User data for the warningcb.
 * @param allow_out_of_order Whether we should allow different order of packages
 *                           among the main metadata files. If allowed, the more
 *                           the order varies the more memory we will need to
 *                           store all the started but unfinished packages.
 * @param err                GError **
 * @return                   cr_Error code.
 */
int
cr_xml_parse_main_metadata_together(const char *primary_path,
                                    const char *filelists_path,
                                    const char *other_path,
                                    cr_XmlParserNewPkgCb newpkgcb,
                                    void *newpkgcb_data,
                                    cr_XmlParserPkgCb pkgcb,
                                    void *pkgcb_data,
                                    cr_XmlParserWarningCb warningcb,
                                    void *warningcb_data,
                                    gboolean allow_out_of_order,
                                    GError **err);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_XML_PARSER_H__ */
