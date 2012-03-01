#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <zlib.h>
#include <libxml/xmlreader.h>
#include "logging.h"
#include "load_metadata.h"
#include "compression_wrapper.h"

#undef MODULE
#define MODULE "load_metadata: "


void free_values(gpointer data)
{
    struct PackageMetadata *md = (struct PackageMetadata *) data;
    xmlFree(md->location_href);
    if (md->location_base) {
        xmlFree(md->location_base);
    }
    xmlFree(md->checksum_type);
    g_free(md->primary_xml);
    g_free(md->filelists_xml);
    g_free(md->other_xml);
    g_free(md);
}


GHashTable *new_old_metadata_hashtable()
{
    GHashTable *hashtable = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, free_values);
    return hashtable;
}


void destroy_old_metadata_hashtable(GHashTable *hashtable)
{
    if (hashtable) {
        g_hash_table_destroy (hashtable);
    }
}


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
    g_free(ml->repomd);
    g_free(ml);
}


int xmlInputReadCallback_plaintext (void * context, char * buffer, int len)
{
    int readed = fread(buffer, 1, len, (FILE *) context);
    if (readed != len && !feof((FILE *) context)) {
        return -1;
    }
    return readed;
}


int xmlInputReadCallback_compressed (void * context, char * buffer, int len)
{
    int readed = cw_read( ((CW_FILE *) context), buffer, (unsigned int) len);
    if (readed == CW_ERR) {
        return -1;
    }
    return readed;
}


int xmlInputCloseCallback_plaintext (void * context) {
    return fclose((FILE *) context) ? -1 : 0;
}


int xmlInputCloseCallback_compressed (void * context) {
    return (cw_close((CW_FILE *) context) == CW_OK) ? 0 : -1;
}



void process_node(GHashTable *metadata, xmlTextReaderPtr pri_reader,
                 xmlTextReaderPtr fil_reader, xmlTextReaderPtr oth_reader) {

/*
 * TODO: Maybe some repodata sanity checks
 */

    if (xmlTextReaderNodeType(pri_reader) != 1 ||
        xmlTextReaderNodeType(fil_reader) != 1 ||
        xmlTextReaderNodeType(oth_reader) != 1)
    {
        // If element is not a start element -> SKIP
        return;
    }


    // Expand current node
    xmlNodePtr pri_pkg_node = xmlTextReaderExpand(pri_reader);
    xmlNodePtr fil_pkg_node = xmlTextReaderExpand(fil_reader);
    xmlNodePtr oth_pkg_node = xmlTextReaderExpand(oth_reader);

    // Get xml dumps
    int len;
    char *pri_pkg_xml;
    char *fil_pkg_xml;
    char *oth_pkg_xml;
    xmlBufferPtr buf = xmlBufferCreate();

    len = xmlNodeDump(buf, NULL, pri_pkg_node, 0, 0);  // TODO: formatting
    pri_pkg_xml = malloc(sizeof(char) * (len+1));
    strcpy(pri_pkg_xml, (char *) xmlBufferContent(buf));
    ((char *) pri_pkg_xml)[len] = '\0';

    xmlBufferEmpty(buf);

    len = xmlNodeDump(buf, NULL, fil_pkg_node, 0, 0);  // TODO: formatting
    fil_pkg_xml = malloc(sizeof(char) * (len+1));
    strcpy(fil_pkg_xml, (char *) xmlBufferContent(buf));
    ((char *) fil_pkg_xml)[len] = '\0';

    xmlBufferEmpty(buf);

    len = xmlNodeDump(buf, NULL, oth_pkg_node, 0, 0);  // TODO: formatting
    oth_pkg_xml = malloc(sizeof(char) * (len+1));
    strcpy(oth_pkg_xml, (char *) xmlBufferContent(buf));
    ((char *) oth_pkg_xml)[len] = '\0';

    xmlBufferFree(buf);

    // Get some info about package
    char *location_href = NULL;
    char *location_base = NULL;
    char *checksum_type = NULL;
    long time_file = -1;
    long size = -1;

    xmlNodePtr node = pri_pkg_node->children;
    int counter = 0;
    while (node) {
        if (node->type != XML_ELEMENT_NODE) {
            node = xmlNextElementSibling(node);
            continue;
        }

        if (!g_strcmp0((char *) node->name, "location")) {
            location_href = (char *) xmlGetProp(node, (xmlChar *) "href");
            location_base = (char *) xmlGetProp(node, (xmlChar *) "base");
            counter++;
        } else if (!g_strcmp0((char *) node->name, "checksum")) {
            checksum_type = (char *) xmlGetProp(node, (xmlChar *) "type");
            counter++;
        } else if (!g_strcmp0((char *) node->name, "size")) {
            char *size_str = (char *) xmlGetProp(node, (xmlChar *) "package");
            size = g_ascii_strtoll(size_str, NULL, 10);
            xmlFree(size_str);
            counter++;
        } else if (!g_strcmp0((char *) node->name, "time")) {
            char *file_time_str = (char *) xmlGetProp(node, (xmlChar *) "file");
            time_file = g_ascii_strtoll(file_time_str, NULL, 10);
            xmlFree(file_time_str);
            counter++;
        }

        if (counter == 4) {
            // We got everything we needed
            break;
        }

        node = xmlNextElementSibling(node);
    }

    if ( counter != 4 || !location_href || !checksum_type) {
        g_warning(MODULE"process_node: Bad xml data! Some information are missing (for package: %s)!", location_href);
        g_free(pri_pkg_xml);
        g_free(fil_pkg_xml);
        g_free(oth_pkg_xml);
        xmlFree(location_href);
        if (location_base) {
            xmlFree(location_base);
        }
        xmlFree(checksum_type);
        return;
    }

    // Key is filename only and it is only pointer into location_href
    gchar *key;
    key = g_strrstr(location_href, "/");
    if (!key) {
        key = location_href;
    } else {
        key++;
    }

    // Check if key already exists
    if (g_hash_table_lookup(metadata, key)) {
        g_warning(MODULE"process_node: Warning: Key \"%s\" already exists in old metadata\n", key);
        g_free(pri_pkg_xml);
        g_free(fil_pkg_xml);
        g_free(oth_pkg_xml);
        xmlFree(location_href);
        if (location_base) {
            xmlFree(location_base);
        }
        xmlFree(checksum_type);
        return;
    }

    // Insert record into hashtable
    struct PackageMetadata *pkg_md = g_malloc(sizeof(struct PackageMetadata));
    pkg_md->time_file = time_file;
    pkg_md->size_package = size;
    pkg_md->location_href = location_href;
    pkg_md->location_base = location_base;
    pkg_md->checksum_type = checksum_type;
    pkg_md->primary_xml = pri_pkg_xml;
    pkg_md->filelists_xml = fil_pkg_xml;
    pkg_md->other_xml = oth_pkg_xml;
    g_hash_table_insert(metadata, key, pkg_md);
}



int parse_xml_metadata(GHashTable *hashtable, xmlTextReaderPtr pri_reader, xmlTextReaderPtr fil_reader, xmlTextReaderPtr oth_reader)
{
    if (!hashtable || !pri_reader || !fil_reader || !oth_reader) {
        return 0;
    }


    // Go to a package level in xmls

    int pri_ret;
    int fil_ret;
    int oth_ret;
    xmlChar *name;


    // Get root element

    pri_ret = xmlTextReaderRead(pri_reader);
    name = xmlTextReaderName(pri_reader);
    if (g_strcmp0((char *) name, "metadata")) {
        xmlFree(name);
        return 0;
    }
    xmlFree(name);

    fil_ret = xmlTextReaderRead(fil_reader);
    name = xmlTextReaderName(fil_reader);
    if (g_strcmp0((char *) name, "filelists")) {
        xmlFree(name);
        return 0;
    }
    xmlFree(name);

    oth_ret = xmlTextReaderRead(oth_reader);
    name = xmlTextReaderName(oth_reader);
    if (g_strcmp0((char *) name, "otherdata")) {
        xmlFree(name);
        return 0;
    }
    xmlFree(name);


    // Get first package element

    pri_ret = xmlTextReaderRead(pri_reader);
    name = xmlTextReaderName(pri_reader);
    if (g_strcmp0((char *) name, "package")) {
        g_warning(MODULE"parse_xml_metadata: Probably bad xml");
        xmlFree(name);
        return 0;
    }
    xmlFree(name);

    fil_ret = xmlTextReaderRead(fil_reader);
    name = xmlTextReaderName(fil_reader);
    if (g_strcmp0((char *) name, "package")) {
        g_warning(MODULE"parse_xml_metadata: Probably bad xml");
        xmlFree(name);
        return 0;
    }
    xmlFree(name);

    oth_ret = xmlTextReaderRead(oth_reader);
    name = xmlTextReaderName(oth_reader);
    if (g_strcmp0((char *) name, "package")) {
        g_warning(MODULE"parse_xml_metadata: Probably bad xml");
        xmlFree(name);
        return 0;
    }
    xmlFree(name);

    while (pri_ret && fil_ret && oth_ret) {
        process_node(hashtable, pri_reader, fil_reader, oth_reader);
        pri_ret = xmlTextReaderNext(pri_reader);
        fil_ret = xmlTextReaderNext(fil_reader);
        oth_ret = xmlTextReaderNext(oth_reader);
    }

    return 1;
}



int load_compressed_xml_metadata(GHashTable *hashtable, const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path)
{
    if (!hashtable) {
        g_debug(MODULE"load_compressed_xml_metadata: No hash table passed");
        return 0;
    }

    GFileTest flags = G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR;
    if (!g_file_test(primary_xml_path, flags) ||
        !g_file_test(filelists_xml_path, flags) ||
        !g_file_test(other_xml_path, flags))
    {
        g_debug(MODULE"load_compressed_xml_metadata: One or more files don't exist");
        return 0;
    }

    // Detect compression type
    CompressionType c_type;
    c_type = detect_compression(primary_xml_path);
    if (c_type == UNKNOWN_COMPRESSION || c_type == NO_COMPRESSION) {
        g_debug(MODULE"load_compressed_xml_metadata: Unknown compression");
        return 0;
    }

    // Open compressed files
    CW_FILE *pri_xml_cwfile;
    CW_FILE *fil_xml_cwfile;
    CW_FILE *oth_xml_cwfile;

    if (!(pri_xml_cwfile = cw_open(primary_xml_path, CW_MODE_READ, c_type))) {
        return 0;
    }

    if (!(fil_xml_cwfile = cw_open(filelists_xml_path, CW_MODE_READ, c_type))) {
        cw_close(pri_xml_cwfile);
        return 0;
    }

    if (!(oth_xml_cwfile = cw_open(other_xml_path, CW_MODE_READ, c_type))) {
        cw_close(pri_xml_cwfile);
        cw_close(fil_xml_cwfile);
        return 0;
    }

    // Setup xml readers
    xmlTextReaderPtr pri_reader;
    xmlTextReaderPtr fil_reader;
    xmlTextReaderPtr oth_reader;

    pri_reader = xmlReaderForIO(xmlInputReadCallback_compressed,
                                xmlInputCloseCallback_compressed,
                                pri_xml_cwfile,
                                NULL,
                                NULL,
                                XML_PARSE_NOBLANKS);
    if (!pri_reader) {
        g_critical(MODULE"load_compressed_xml_metadata: Reader for primary.xml.* file failed");
        cw_close(pri_xml_cwfile);
        cw_close(fil_xml_cwfile);
        cw_close(oth_xml_cwfile);
        return 0;
    }

    fil_reader = xmlReaderForIO(xmlInputReadCallback_compressed,
                                xmlInputCloseCallback_compressed,
                                fil_xml_cwfile,
                                NULL,
                                NULL,
                                XML_PARSE_NOBLANKS);
    if (!fil_reader) {
        g_critical(MODULE"load_compressed_xml_metadata: Reader for filelists.xml.* file failed");
        xmlFreeTextReader(pri_reader);
        cw_close(fil_xml_cwfile);
        cw_close(oth_xml_cwfile);
        return 0;
    }

    oth_reader = xmlReaderForIO(xmlInputReadCallback_compressed,
                                xmlInputCloseCallback_compressed,
                                oth_xml_cwfile,
                                NULL,
                                NULL,
                                XML_PARSE_NOBLANKS);
    if (!oth_reader) {
        g_critical(MODULE"load_compressed_xml_metadata: Reader for other.xml.* file failed");
        xmlFreeTextReader(pri_reader);
        xmlFreeTextReader(fil_reader);
        cw_close(oth_xml_cwfile);
        return 0;
    }

    int result = parse_xml_metadata(hashtable, pri_reader, fil_reader, oth_reader);

    xmlFreeTextReader(pri_reader);
    xmlFreeTextReader(fil_reader);
    xmlFreeTextReader(oth_reader);

    return result;
}


int load_xml_metadata(GHashTable *hashtable, const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path)
{
    if (!hashtable) {
        g_debug(MODULE"load_xml_metadata: No hash table passed");
        return 0;
    }

    GFileTest flags = G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR;
    if (!g_file_test(primary_xml_path, flags) ||
        !g_file_test(filelists_xml_path, flags) ||
        !g_file_test(other_xml_path, flags))
    {
        g_debug(MODULE"load_xml_metadata: One or more files don't exist");
        return 0;
    }

    FILE *pri_xml_file = fopen(primary_xml_path, "rb");
    FILE *fil_xml_file = fopen(filelists_xml_path, "rb");
    FILE *oth_xml_file = fopen(other_xml_path, "rb");

    // Setup xml readers
    xmlTextReaderPtr pri_reader;
    xmlTextReaderPtr fil_reader;
    xmlTextReaderPtr oth_reader;

    pri_reader = xmlReaderForIO(xmlInputReadCallback_plaintext,
                                xmlInputCloseCallback_plaintext,
                                pri_xml_file,
                                NULL,
                                NULL,
                                XML_PARSE_NOBLANKS);
    if (!pri_reader) {
        g_critical(MODULE"load_xml_metadata: Reader for primary.xml file failed");
        fclose(pri_xml_file);
        fclose(fil_xml_file);
        fclose(oth_xml_file);
        return 0;
    }

    fil_reader = xmlReaderForIO(xmlInputReadCallback_plaintext,
                                xmlInputCloseCallback_plaintext,
                                fil_xml_file,
                                NULL,
                                NULL,
                                XML_PARSE_NOBLANKS);
    if (!fil_reader) {
        g_critical(MODULE"load_xml_metadata: Reader for filelists.xml file failed");
        xmlFreeTextReader(pri_reader);
        fclose(fil_xml_file);
        fclose(oth_xml_file);
        return 0;
    }

    oth_reader = xmlReaderForIO(xmlInputReadCallback_plaintext,
                                xmlInputCloseCallback_plaintext,
                                oth_xml_file,
                                NULL,
                                NULL,
                                XML_PARSE_NOBLANKS);
    if (!oth_reader) {
        g_critical("load_xml_metadata: Reader for other.xml file failed");
        xmlFreeTextReader(pri_reader);
        xmlFreeTextReader(fil_reader);
        fclose(oth_xml_file);
        return 0;
    }

    int result = parse_xml_metadata(hashtable, pri_reader, fil_reader, oth_reader);

    xmlFreeTextReader(pri_reader);
    xmlFreeTextReader(fil_reader);
    xmlFreeTextReader(oth_reader);

    return result;
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
        g_debug(MODULE"locate_metadata_via_repomd: %s doesn't exists", repomd);
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
        g_warning(MODULE"locate_metadata_via_repomd: Bad xml - missing repomd element? (%s)", name);
        xmlFree(name);
        xmlFreeTextReader(reader);
        g_free(repomd);
        return NULL;
    }
    xmlFree(name);

    ret = xmlTextReaderRead(reader);
    name = xmlTextReaderName(reader);
    if (g_strcmp0((char *) name, "revision")) {
        g_warning(MODULE"locate_metadata_via_repomd: Bad xml - missing revision element? (%s)", name);
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
        g_warning(MODULE"locate_metadata_vie_repomd: Bad xml - missing data elements?");
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


int locate_and_load_xml_metadata(GHashTable *hashtable, const char *repopath)
{
    if (!hashtable || !repopath || !g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        return 0;
    }


    // Get paths of old metadata files from repomd

    struct MetadataLocation *ml;
    ml = locate_metadata_via_repomd(repopath);
    if (!ml) {
        return 0;
    }


    if (!ml->pri_xml_href || !ml->fil_xml_href || !ml->oth_xml_href) {
        // Some file(s) is/are missing
        free_metadata_location(ml);
        return 0;
    }


    // Load metadata

    int result;

    if(g_str_has_suffix(ml->pri_xml_href, ".xml") &&
              g_str_has_suffix(ml->fil_xml_href, ".xml") &&
              g_str_has_suffix(ml->oth_xml_href, ".xml"))
    {
        result = load_xml_metadata(hashtable, ml->pri_xml_href, ml->fil_xml_href, ml->oth_xml_href);
    } else {
        result = load_compressed_xml_metadata(hashtable, ml->pri_xml_href, ml->fil_xml_href, ml->oth_xml_href);
    }

    free_metadata_location(ml);

    return result;
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
        g_debug(MODULE"remove_old_metadata: Path %s doesn't exists", repopath);
        return -1;
    }


    // List dir and remove all files which could be related to metadata

    int removed_files = 0;
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

            g_debug(MODULE"Removing: %s", path);
            if (g_remove(path) == -1) {
                g_warning(MODULE"remove_old_metadata: Cannot remove %s", path);
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
