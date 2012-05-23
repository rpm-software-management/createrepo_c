/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __C_CREATEREPOLIB_XML_DUMP_H__
#define __C_CREATEREPOLIB_XML_DUMP_H__

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "package.h"

// XML namespaces

#define XML_COMMON_NS           "http://linux.duke.edu/metadata/common"
#define XML_FILELISTS_NS        "http://linux.duke.edu/metadata/filelists"
#define XML_OTHER_NS            "http://linux.duke.edu/metadata/other"
#define XML_RPM_NS              "http://linux.duke.edu/metadata/rpm"


struct XmlStruct{
    char *primary;
    char *filelists;
    char *other;
};


void dump_files(xmlNodePtr, Package *, int);

char *xml_dump_primary(Package *);
char *xml_dump_filelists(Package *);
char *xml_dump_other(Package *);
struct XmlStruct xml_dump(Package *);

#endif /* __C_CREATEREPOLIB_XML_DUMP_H__ */
