#include <glib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "logging.h"
#include "package.h"
#include "xml_dump.h"

#undef MODULE
#define MODULE "xml_dump_other: "

#define FORMAT_XML  1

#define DATE_MAX_LEN    32


void dump_changelog(xmlNodePtr root, Package *package)
{
    if (!package->changelogs) {
        return;
    }

    GSList *element = NULL;
    for(element = package->changelogs; element; element=element->next) {

        ChangelogEntry *entry = (ChangelogEntry*) element->data;

        assert(entry);


        // ***********************************
        // Element: Changelog
        // ************************************

        xmlNodePtr changelog;

        // Add changelog element
        changelog = xmlNewTextChild(root, NULL, BAD_CAST "changelog", BAD_CAST ((entry->changelog) ? entry->changelog : ""));

        // Write param author
        xmlNewProp(changelog, BAD_CAST "author", BAD_CAST ((entry->author) ? entry->author : ""));

        // Write param date
        char date_str[DATE_MAX_LEN];
        g_snprintf(date_str, DATE_MAX_LEN, "%lld", (long long int) entry->date);
        xmlNewProp(changelog, BAD_CAST "date", BAD_CAST date_str);
    }
}


void dump_other_items(xmlNodePtr root, Package *package)
{
    /***********************************
     Element: package
    ************************************/

    // Add pkgid attribute
    xmlNewProp(root, BAD_CAST "pkgid", BAD_CAST ((package->pkgId) ? package->pkgId : ""));

    // Add name attribute
    xmlNewProp(root, BAD_CAST "name", BAD_CAST ((package->name) ? package->name : ""));

    // Add arch attribute
    xmlNewProp(root, BAD_CAST "arch", BAD_CAST ((package->arch) ? package->arch : ""));


    /***********************************
     Element: version
    ************************************/

    xmlNodePtr version;

    // Add version element
    version = xmlNewChild(root, NULL, BAD_CAST "version", NULL);

    // Write version attribute epoch
    xmlNewProp(version, BAD_CAST "epoch", BAD_CAST ((package->epoch) ? package->epoch : ""));

    // Write version attribute ver
    xmlNewProp(version, BAD_CAST "ver", BAD_CAST ((package->version) ? package->version : ""));

    // Write version attribute rel
    xmlNewProp(version, BAD_CAST "rel", BAD_CAST ((package->release) ? package->release : ""));


    // Changelog dump

    dump_changelog(root, package);
}


char *xml_dump_other(Package *package)
{
    xmlNodePtr root = NULL;
    root = xmlNewNode(NULL, BAD_CAST "package");


    // Dump IT!

    dump_other_items(root, package);

    char *result;
    xmlBufferPtr buf = xmlBufferCreate();
    if (buf == NULL) {
        g_critical(MODULE"%s: Error creating the xml buffer", __func__);
        return NULL;
    }
    // Seems to be little bit faster than xmlDocDumpFormatMemory
    xmlNodeDump(buf, NULL, root, 1, FORMAT_XML);
    assert(buf->content);
    result = g_strdup((char *) buf->content);
    xmlBufferFree(buf);


    // Cleanup

    xmlFreeNode(root);

    return result;
}
