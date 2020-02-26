/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2014  Tomas Mlcoch
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

#include <glib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlsave.h>
#include "error.h"
#include "updateinfo.h"
#include "xml_dump.h"
#include "xml_dump_internal.h"

#define ERR_DOMAIN      CREATEREPO_C_ERROR
#define INDENT          2

void
cr_xml_dump_updatecollectionpackages(xmlNodePtr collection, GSList *packages)
{
    for (GSList *elem = packages; elem; elem = g_slist_next(elem)) {
        cr_UpdateCollectionPackage *pkg = elem->data;
        xmlNodePtr package;

        package = xmlNewChild(collection, NULL, BAD_CAST "package", NULL);
        cr_xmlNewProp_c(package, BAD_CAST "name", BAD_CAST pkg->name);
        cr_xmlNewProp_c(package, BAD_CAST "version", BAD_CAST pkg->version);
        cr_xmlNewProp_c(package, BAD_CAST "release", BAD_CAST pkg->release);
        cr_xmlNewProp_c(package, BAD_CAST "epoch", BAD_CAST pkg->epoch);
        cr_xmlNewProp_c(package, BAD_CAST "arch", BAD_CAST pkg->arch);
        cr_xmlNewProp_c(package, BAD_CAST "src", BAD_CAST pkg->src);
        cr_xmlNewTextChild_c(package, NULL, BAD_CAST "filename",
                             BAD_CAST pkg->filename);

        if (pkg->sum) {
            xmlNodePtr sum;
            sum = cr_xmlNewTextChild_c(package,
                                       NULL,
                                       BAD_CAST "sum",
                                       BAD_CAST pkg->sum);
            cr_xmlNewProp_c(sum,
                            BAD_CAST "type",
                            BAD_CAST cr_checksum_name_str(pkg->sum_type));
        }

        if (pkg->reboot_suggested)
            xmlNewChild(package, NULL, BAD_CAST "reboot_suggested", BAD_CAST "True");
        if (pkg->restart_suggested)
            xmlNewChild(package, NULL, BAD_CAST "restart_suggested", BAD_CAST "True");
        if (pkg->relogin_suggested)
            xmlNewChild(package, NULL, BAD_CAST "relogin_suggested", BAD_CAST "True");
    }
}

void
cr_xml_dump_updatecollectionmodule(xmlNodePtr collection, cr_UpdateCollectionModule *module)
{
    if (!module)
        return;

    xmlNodePtr xml_module;
    xml_module = xmlNewChild(collection, NULL, BAD_CAST "module", NULL);

    cr_xmlNewProp_c(xml_module, BAD_CAST "name", BAD_CAST module->name);
    cr_xmlNewProp_c(xml_module, BAD_CAST "stream", BAD_CAST module->stream);
    gchar buf[21]; //20 + '\0' is max number of chars of guint64: G_MAXUINT64 (= 18,446,744,073,709,551,615)
    snprintf(buf, 21, "%" G_GUINT64_FORMAT, module->version);
    cr_xmlNewProp_c(xml_module, BAD_CAST "version", BAD_CAST buf);
    cr_xmlNewProp_c(xml_module, BAD_CAST "context", BAD_CAST module->context);
    cr_xmlNewProp_c(xml_module, BAD_CAST "arch", BAD_CAST module->arch);
}

void
cr_xml_dump_updateinforecord_pkglist(xmlNodePtr update, GSList *collections)
{
    xmlNodePtr pkglist;

    pkglist = xmlNewChild(update, NULL, BAD_CAST "pkglist", NULL);

    for (GSList *elem = collections; elem; elem = g_slist_next(elem)) {
        cr_UpdateCollection *col = elem->data;
        xmlNodePtr collection;

        collection = xmlNewChild(pkglist, NULL, BAD_CAST "collection", NULL);
        cr_xmlNewProp_c(collection, BAD_CAST "short", BAD_CAST col->shortname);
        cr_xmlNewTextChild_c(collection,
                             NULL,
                             BAD_CAST "name",
                             BAD_CAST col->name);

        cr_xml_dump_updatecollectionmodule(collection, col->module);

        cr_xml_dump_updatecollectionpackages(collection, col->packages);
    }
}

void
cr_xml_dump_updateinforecord_references(xmlNodePtr update, GSList *refs)
{
    xmlNodePtr references;
    references = xmlNewChild(update, NULL, BAD_CAST "references", NULL);

    for (GSList *elem = refs; elem; elem = g_slist_next(elem)) {
        cr_UpdateReference *ref = elem->data;
        xmlNodePtr reference;

        reference = xmlNewChild(references, NULL, BAD_CAST "reference", NULL);
        cr_xmlNewProp_c(reference, BAD_CAST "href", BAD_CAST ref->href);
        cr_xmlNewProp_c(reference, BAD_CAST "id", BAD_CAST ref->id);
        cr_xmlNewProp_c(reference, BAD_CAST "type", BAD_CAST ref->type);
        cr_xmlNewProp_c(reference, BAD_CAST "title", BAD_CAST ref->title);
    }
}

xmlNodePtr
cr_xml_dump_updateinforecord_internal(xmlNodePtr root, cr_UpdateRecord *rec)
{
    xmlNodePtr update, node;

    if (!rec)
        return NULL;

    // Update element
    if (!root)
        update = xmlNewNode(NULL, BAD_CAST "update");
    else
        update = xmlNewChild(root, NULL, BAD_CAST "update", NULL);

    cr_xmlNewProp_c(update, BAD_CAST "from", BAD_CAST rec->from);
    cr_xmlNewProp_c(update, BAD_CAST "status", BAD_CAST rec->status);
    cr_xmlNewProp_c(update, BAD_CAST "type", BAD_CAST rec->type);
    cr_xmlNewProp_c(update, BAD_CAST "version", BAD_CAST rec->version);

    cr_xmlNewTextChild_c(update, NULL, BAD_CAST "id", BAD_CAST rec->id);
    cr_xmlNewTextChild_c(update, NULL, BAD_CAST "title", BAD_CAST rec->title);

    if (rec->issued_date) {
        node = xmlNewChild(update, NULL, BAD_CAST "issued", NULL);
        cr_xmlNewProp(node, BAD_CAST "date", BAD_CAST rec->issued_date);
    }

    if (rec->updated_date) {
        node = xmlNewChild(update, NULL, BAD_CAST "updated", NULL);
        cr_xmlNewProp(node, BAD_CAST "date", BAD_CAST rec->updated_date);
    }

    cr_xmlNewTextChild_c(update, NULL, BAD_CAST "rights", BAD_CAST rec->rights);
    cr_xmlNewTextChild_c(update, NULL, BAD_CAST "release", BAD_CAST rec->release);
    cr_xmlNewTextChild_c(update, NULL, BAD_CAST "pushcount", BAD_CAST rec->pushcount);
    cr_xmlNewTextChild_c(update, NULL, BAD_CAST "severity", BAD_CAST rec->severity);
    cr_xmlNewTextChild_c(update, NULL, BAD_CAST "summary", BAD_CAST rec->summary);
    cr_xmlNewTextChild_c(update, NULL, BAD_CAST "description", BAD_CAST rec->description);
    cr_xmlNewTextChild_c(update, NULL, BAD_CAST "solution", BAD_CAST rec->solution);

    if (rec->reboot_suggested)
        xmlNewChild(update, NULL, BAD_CAST "reboot_suggested", BAD_CAST "True");

    // References
    cr_xml_dump_updateinforecord_references(update, rec->references);

    // Pkglist
    cr_xml_dump_updateinforecord_pkglist(update, rec->collections);

    return update;
}


void
cr_xml_dump_updateinfo_body(xmlNodePtr root, cr_UpdateInfo *ui)
{
    GSList *element;

    // Dump updates
    for (element = ui->updates; element; element = g_slist_next(element)) {
        cr_UpdateRecord *rec = element->data;
        cr_xml_dump_updateinforecord_internal(root, rec);
    }
}


char *
cr_xml_dump_updateinfo(cr_UpdateInfo *updateinfo, GError **err)
{
    xmlDocPtr doc;
    xmlNodePtr root;
    char *result;

    assert(!err || *err == NULL);

    if (!updateinfo)
        return NULL;

    // Dump IT!

    doc = xmlNewDoc(BAD_CAST XML_DOC_VERSION);
    root = xmlNewNode(NULL, BAD_CAST "updates");
    cr_xml_dump_updateinfo_body(root, updateinfo);
    xmlDocSetRootElement(doc, root);
    xmlDocDumpFormatMemoryEnc(doc,
                              (xmlChar **) &result,
                              NULL,
                              XML_ENCODING,
                              FORMAT_XML);

    // Clean up

    xmlFreeDoc(doc);

    return result;
}

char *
cr_xml_dump_updaterecord(cr_UpdateRecord *rec, GError **err)
{
    xmlNodePtr root;
    char *result;

    assert(!err || *err == NULL);

    if (!rec) {
        g_set_error(err, CREATEREPO_C_ERROR, CRE_BADARG,
                    "No updateinfo object to dump specified");
        return NULL;
    }

    // Dump IT!

    xmlBufferPtr buf = xmlBufferCreate();
    if (buf == NULL) {
        g_critical("%s: Error creating the xml buffer", __func__);
        g_set_error(err, ERR_DOMAIN, CRE_MEMORY,
                    "Cannot create an xml buffer");
        return NULL;
    }

    root = cr_xml_dump_updateinforecord_internal(NULL, rec);
    // xmlNodeDump seems to be a little bit faster than xmlDocDumpFormatMemory
    xmlNodeDump(buf, NULL, root, 1, FORMAT_XML);
    assert(buf->content);
    // First line in the buf is not indented, we must indent it by ourself
    result = g_malloc(sizeof(char *) * buf->use + INDENT + 1);
    for (int x = 0; x < INDENT; x++) result[x] = ' ';
    memcpy((void *) result+INDENT, buf->content, buf->use);
    result[buf->use + INDENT]   = '\n';
    result[buf->use + INDENT + 1]   = '\0';

    // Cleanup

    xmlBufferFree(buf);
    xmlFreeNode(root);

    return result;
}
