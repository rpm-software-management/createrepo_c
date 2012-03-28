#include <glib.h>
#include <glib/gstdio.h>
#include <libxml/xmlreader.h>
#include "locate_metadata.h"

#undef MODULE
#define MODULE "locate_metadata: "

#define FORMAT_XML      1
#define FORMAT_LEVEL    0



void free_metadata_location(struct MetadataLocation *ml)
{
    if (!ml) {
        return;
    }

    g_free(ml->pri_xml_href);
    g_free(ml->fil_xml_href);
    g_free(ml->oth_xml_href);
    g_free(ml->pri_sqlite_href);
    g_free(ml->fil_sqlite_href);
    g_free(ml->oth_sqlite_href);
    g_free(ml->groupfile_href);
    g_free(ml->cgroupfile_href);
    g_free(ml->repomd);
    g_free(ml);
}



struct MetadataLocation *locate_metadata_via_repomd(const char *repopath) {

    if (!repopath || !g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        return NULL;
    }


    // Check if repopath ends with slash

    gboolean repopath_ends_with_slash = FALSE;

    if (g_str_has_suffix(repopath, "/")) {
        repopath_ends_with_slash = TRUE;
    }


    // Create path to repomd.xml and check if it exists

    gchar *repomd;

    if (repopath_ends_with_slash) {
        repomd = g_strconcat(repopath, "repodata/repomd.xml", NULL);
    } else {
        repomd = g_strconcat(repopath, "/repodata/repomd.xml", NULL);
    }

    if (!g_file_test(repomd, G_FILE_TEST_EXISTS)) {
        g_debug(MODULE"%s: %s doesn't exists", __func__, repomd);
        g_free(repomd);
        return NULL;
    }


    // Parse repomd.xml

    int ret;
    xmlChar *name;
    xmlTextReaderPtr reader;
    reader = xmlReaderForFile(repomd, NULL, XML_PARSE_NOBLANKS);

    ret = xmlTextReaderRead(reader);
    name = xmlTextReaderName(reader);
    if (g_strcmp0((char *) name, "repomd")) {
        g_warning(MODULE"%s: Bad xml - missing repomd element? (%s)", __func__, name);
        xmlFree(name);
        xmlFreeTextReader(reader);
        g_free(repomd);
        return NULL;
    }
    xmlFree(name);

    ret = xmlTextReaderRead(reader);
    name = xmlTextReaderName(reader);
    if (g_strcmp0((char *) name, "revision")) {
        g_warning(MODULE"%s: Bad xml - missing revision element? (%s)", __func__, name);
        xmlFree(name);
        xmlFreeTextReader(reader);
        g_free(repomd);
        return NULL;
    }
    xmlFree(name);


    // Parse data elements

    while (ret) {
        // Find first data element
        ret = xmlTextReaderRead(reader);
        name = xmlTextReaderName(reader);
        if (!g_strcmp0((char *) name, "data")) {
            xmlFree(name);
            break;
        }
        xmlFree(name);
    }

    if (!ret) {
        // No elements left -> Bad xml
        g_warning(MODULE"%s: Bad xml - missing data elements?", __func__);
        xmlFreeTextReader(reader);
        g_free(repomd);
        return NULL;
    }

    struct MetadataLocation *mdloc;
    mdloc = g_malloc0(sizeof(struct MetadataLocation));
    mdloc->repomd = repomd;

    xmlChar *data_type = NULL;
    xmlChar *location_href = NULL;

    while (ret) {
        if (xmlTextReaderNodeType(reader) != 1) {
            ret = xmlTextReaderNext(reader);
            continue;
        }

        xmlNodePtr data_node = xmlTextReaderExpand(reader);
        data_type = xmlGetProp(data_node, (xmlChar *) "type");
        xmlNodePtr sub_node = data_node->children;

        while (sub_node) {
            if (sub_node->type != XML_ELEMENT_NODE) {
                sub_node = xmlNextElementSibling(sub_node);
                continue;
            }

            if (!g_strcmp0((char *) sub_node->name, "location")) {
                location_href = xmlGetProp(sub_node, (xmlChar *) "href");
            }

            // TODO: Check repodata validity checksum? mtime? size?

            sub_node = xmlNextElementSibling(sub_node);
        }


        // Build absolute path

        gchar *full_location_href;
        if (repopath_ends_with_slash) {
            full_location_href = g_strconcat(repopath, (char *) location_href, NULL);
        } else {
            full_location_href = g_strconcat(repopath, "/", (char *) location_href, NULL);
        }


        // Store the path

        if (!g_strcmp0((char *) data_type, "primary")) {
            mdloc->pri_xml_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "filelists")) {
            mdloc->fil_xml_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "other")) {
            mdloc->oth_xml_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "primary_db")) {
            mdloc->pri_sqlite_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "filelists_db")) {
            mdloc->fil_sqlite_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "other_db")) {
            mdloc->oth_sqlite_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "group")) {
            mdloc->groupfile_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "group_gz")) { // even with a createrepo param --xz this name has a _gz suffix
            mdloc->cgroupfile_href = full_location_href;
        }


        // Memory cleanup

        xmlFree(data_type);
        xmlFree(location_href);
        location_href = NULL;

        ret = xmlTextReaderNext(reader);
    }

    xmlFreeTextReader(reader);

    // Note: Do not free repomd! It is pointed from mdloc structure!

    return mdloc;
}


// Return list of non-null pointers on strings in the passed structure
GSList *get_list_of_md_locations (struct MetadataLocation *ml)
{
    GSList *list = NULL;

    if (!ml) {
        return list;
    }

    if (ml->pri_xml_href)    list = g_slist_prepend(list, (gpointer) ml->pri_xml_href);
    if (ml->fil_xml_href)    list = g_slist_prepend(list, (gpointer) ml->fil_xml_href);
    if (ml->oth_xml_href)    list = g_slist_prepend(list, (gpointer) ml->oth_xml_href);
    if (ml->pri_sqlite_href) list = g_slist_prepend(list, (gpointer) ml->pri_sqlite_href);
    if (ml->fil_sqlite_href) list = g_slist_prepend(list, (gpointer) ml->fil_sqlite_href);
    if (ml->oth_sqlite_href) list = g_slist_prepend(list, (gpointer) ml->oth_sqlite_href);
    if (ml->groupfile_href)  list = g_slist_prepend(list, (gpointer) ml->groupfile_href);
    if (ml->cgroupfile_href) list = g_slist_prepend(list, (gpointer) ml->cgroupfile_href);
    if (ml->repomd)          list = g_slist_prepend(list, (gpointer) ml->repomd);

    return list;
}



void free_list_of_md_locations(GSList *list)
{
    if (list) {
        g_slist_free(list);
    }
}



int remove_old_metadata(const char *repopath)
{
    if (!repopath || !g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        return -1;
    }

    gchar *full_repopath;
    full_repopath = g_strconcat(repopath, "/repodata/", NULL);

    GDir *repodir;
    repodir = g_dir_open(full_repopath, 0, NULL);
    if (!repodir) {
        g_debug(MODULE"%s: Path %s doesn't exists", __func__, repopath);
        return -1;
    }


    // Remove all metadata listed in repomd.xml

    int removed_files = 0;

    struct MetadataLocation *ml;
    ml = locate_metadata_via_repomd(repopath);
    if (ml) {
        GSList *list = get_list_of_md_locations(ml);
        GSList *element;

        for (element=list; element; element=element->next) {
            gchar *path = (char *) element->data;

            g_debug("%s: Removing: %s (path obtained from repomd.xml)", __func__, path);
            if (g_remove(path) == -1) {
                g_warning("%s: remove_old_metadata: Cannot remove %s", __func__, path);
            } else {
                removed_files++;
            }
        }

        free_list_of_md_locations(list);
        free_metadata_location(ml);
    }


    // (Just for sure) List dir and remove all files which could be related to an old metadata

    const gchar *file;
    while ((file = g_dir_read_name (repodir))) {
        if (g_str_has_suffix(file, "primary.xml.gz") ||
            g_str_has_suffix(file, "filelists.xml.gz") ||
            g_str_has_suffix(file, "other.xml.gz") ||
            g_str_has_suffix(file, "primary.xml.bz2") ||
            g_str_has_suffix(file, "filelists.xml.bz2") ||
            g_str_has_suffix(file, "other.xml.bz2") ||
            g_str_has_suffix(file, "primary.xml") ||
            g_str_has_suffix(file, "filelists.xml") ||
            g_str_has_suffix(file, "other.xml") ||
            !g_strcmp0(file, "repomd.xml"))
        {
            gchar *path;
            path = g_strconcat(full_repopath, file, NULL);

            g_debug(MODULE"%s: Removing: %s", __func__, path);
            if (g_remove(path) == -1) {
                g_warning(MODULE"%s: Cannot remove %s", __func__, path);
            } else {
                removed_files++;
            }

            g_free(path);
        }
    }

    g_dir_close(repodir);
    g_free(full_repopath);

    return removed_files;
}
