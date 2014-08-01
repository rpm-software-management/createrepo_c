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

#ifndef __C_CREATEREPOLIB_XML_DUMP_PRIVATE_H__
#define __C_CREATEREPOLIB_XML_DUMP_PRIVATE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "package.h"
#include <libxml/tree.h>

#define XML_DOC_VERSION "1.0"
#define XML_ENCODING    "UTF-8"

#define FORMAT_XML      1
#define FORMAT_LEVEL    0

#define DATE_STR_MAX_LEN        32
#define SIZE_STR_MAX_LEN        32

#if DATE_STR_MAX_LEN > SIZE_STR_MAX_LEN
#define DATESIZE_STR_MAX_LEN    DATE_STR_MAX_LEN
#else
#define DATESIZE_STR_MAX_LEN    SIZE_STR_MAX_LEN
#endif

/** Dump files from the package and append them to the node as childrens.
 * @param node          parent xml node
 * @param package       cr_Package
 * @param primary       process only primary files (see cr_is_primary() function
 *                      in the misc module)
 */
void cr_xml_dump_files(xmlNodePtr node, cr_Package *package, int primary);

/** Createrepo_c wrapper over libxml xmlNewTextChild.
 * It allows content to be NULL and non UTF-8 (if content is no UTF8
 * then iso-8859-1 is assumed).
 */
xmlNodePtr cr_xmlNewTextChild(xmlNodePtr parent,
                              xmlNsPtr ns,
                              const xmlChar *name,
                              const xmlChar *content);

/** Inserts new node only if its value is not NULL
 */
static inline xmlNodePtr
cr_xmlNewTextChild_c(xmlNodePtr parent,
                     xmlNsPtr ns,
                     const xmlChar *name,
                     const xmlChar *content)
{
    if (!content)
        return NULL;
    return cr_xmlNewTextChild(parent, ns, name, content);
}

/** Createrepo_c wrapper over the libxml xmlNewProp.
 * It allows content to be NULL and non UTF-8 (if content is no UTF8
 * then iso-8859-1 is assumed)
 */
xmlAttrPtr cr_xmlNewProp(xmlNodePtr node,
                         const xmlChar *name,
                         const xmlChar *value);

/** Inserts new proprety (attribute) only if its value is not NULL
 */
static inline xmlAttrPtr
cr_xmlNewProp_c(xmlNodePtr node,
                const xmlChar *name,
                const xmlChar *orig_content)
{
    if (!orig_content)
        return NULL;
    return cr_xmlNewProp(node, name, orig_content);
}


#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_XML_DUMP_PRIVATE_H__ */
