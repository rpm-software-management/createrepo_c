#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <zlib.h>
#include <libxml/xmlreader.h>
#include "logging.h"
#include "load_metadata.h"

#undef MODULE
#define MODULE "load_metadata: "

/*
TODO:
- Support for loading bz2 compressed metadata
*/

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


int xmlInputReadCallback_gz_compressed (void * context, char * buffer, int len)
{
    return gzread( *( (gzFile *) context), buffer, (unsigned int) len);
}


int xmlInputCloseCallback_plaintext (void * context) {
    return fclose((FILE *) context) ? -1 : 0;
}


int xmlInputCloseCallback_gz_compressed (void * context) {
    return (gzclose( *( (gzFile *) context ) ) == Z_OK) ? 0 : -1;
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
    long time_file;
    long size;

    xmlNodePtr node = pri_pkg_node->children;
    int counter = 0;
    while (node) {
        if (node->type != XML_ELEMENT_NODE) {
            node = xmlNextElementSibling(node);
            continue;
        }

        if (!strcmp(node->name, "location")) {
            location_href = xmlGetProp(node, "href");
            location_base = xmlGetProp(node, "base");
            counter++;
        } else if (!strcmp(node->name, "checksum")) {
            checksum_type = xmlGetProp(node, "type");
            counter++;
        } else if (!strcmp(node->name, "size")) {
            char *size_str = xmlGetProp(node, "package");
            size = g_ascii_strtoll(size_str, NULL, 10);
            xmlFree(size_str);
            counter++;
        } else if (!strcmp(node->name, "time")) {
            char *file_time_str = xmlGetProp(node, "file");
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

    if ( !location_href || !checksum_type) {
        g_warning(MODULE"process_node: Bad xml data! Some information are missing!");
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
    if (strcmp(name, "metadata")) {
        xmlFree(name);
        return 0;
    }
    xmlFree(name);

    fil_ret = xmlTextReaderRead(fil_reader);
    name = xmlTextReaderName(fil_reader);
    if (strcmp(name, "filelists")) {
        xmlFree(name);
        return 0;
    }
    xmlFree(name);

    oth_ret = xmlTextReaderRead(oth_reader);
    name = xmlTextReaderName(oth_reader);
    if (strcmp(name, "otherdata")) {
        xmlFree(name);
        return 0;
    }
    xmlFree(name);


    // Get first package element

    pri_ret = xmlTextReaderRead(pri_reader);
    name = xmlTextReaderName(pri_reader);
    if (strcmp(name, "package")) {
        g_warning(MODULE"parse_xml_metadata: Probably bad xml");
        xmlFree(name);
        return 0;
    }
    xmlFree(name);

    fil_ret = xmlTextReaderRead(fil_reader);
    name = xmlTextReaderName(fil_reader);
    if (strcmp(name, "package")) {
        g_warning(MODULE"parse_xml_metadata: Probably bad xml");
        xmlFree(name);
        return 0;
    }
    xmlFree(name);

    oth_ret = xmlTextReaderRead(oth_reader);
    name = xmlTextReaderName(oth_reader);
    if (strcmp(name, "package")) {
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


#define GZ_BUFFER_SIZE   131072  // 1024*128

int load_gz_compressed_xml_metadata(GHashTable *hashtable, const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path)
{
    if (!hashtable) {
        g_debug(MODULE"load_gz_compressed_xml_metadata: No hash table passed");
        return 0;
    }

    GFileTest flags = G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR;
    if (!g_file_test(primary_xml_path, flags) ||
        !g_file_test(filelists_xml_path, flags) ||
        !g_file_test(other_xml_path, flags))
    {
        g_debug(MODULE"load_gz_compressed_xml_metadata: One or more files don't exist");
        return 0;
    }

    // Open files and get file descriptors
    int pri_xml_fd = open(primary_xml_path, O_RDONLY);
    int fil_xml_fd = open(filelists_xml_path, O_RDONLY);
    int oth_xml_fd = open(other_xml_path, O_RDONLY);

    // Get sizes of files
    int pri_xml_size = lseek(pri_xml_fd, 0, SEEK_END);
    lseek(pri_xml_fd, 0L, SEEK_SET);
    int fil_xml_size = lseek(fil_xml_fd, 0, SEEK_END);
    lseek(fil_xml_fd, 0L, SEEK_SET);
    int oth_xml_size = lseek(oth_xml_fd, 0, SEEK_END);
    lseek(oth_xml_fd, 0L, SEEK_SET);

    if (!pri_xml_size || !fil_xml_size || !oth_xml_size) {
        // One or more archives are empty
        close(pri_xml_fd);
        close(fil_xml_fd);
        close(oth_xml_fd);
        return 0;
    }

    // Open gziped file
    gzFile pri_xml_gzfile;
    gzFile fil_xml_gzfile;
    gzFile oth_xml_gzfile;

    if (!(pri_xml_gzfile = gzdopen(pri_xml_fd, "rb"))) {
//    if (!(pri_xml_gzfile = gzopen(primary_xml_path, "rb"))) {
        return 0;
    }

    if (!(fil_xml_gzfile = gzdopen(fil_xml_fd, "rb"))) {
//    if (!(fil_xml_gzfile = gzopen(filelists_xml_path, "rb"))) {
        gzclose(pri_xml_gzfile);
        return 0;
    }

    if (!(oth_xml_gzfile = gzdopen(oth_xml_fd, "rb"))) {
//    if (!(oth_xml_gzfile = gzopen(other_xml_path, "rb"))) {
        gzclose(pri_xml_gzfile);
        gzclose(fil_xml_gzfile);
        return 0;
    }

    // Set buffers
    gzbuffer(pri_xml_gzfile, GZ_BUFFER_SIZE);
    gzbuffer(fil_xml_gzfile, GZ_BUFFER_SIZE);
    gzbuffer(oth_xml_gzfile, GZ_BUFFER_SIZE);

    // Setup xml readers
    xmlTextReaderPtr pri_reader;
    xmlTextReaderPtr fil_reader;
    xmlTextReaderPtr oth_reader;

    pri_reader = xmlReaderForIO(xmlInputReadCallback_gz_compressed,
                                xmlInputCloseCallback_gz_compressed,
                                &pri_xml_gzfile,
                                NULL,
                                NULL,
                                XML_PARSE_NOBLANKS);
    if (!pri_reader) {
        g_critical(MODULE"load_gz_compressed_xml_metadata: Reader for primary.xml.gz file failed");
        gzclose(pri_xml_gzfile);
        gzclose(fil_xml_gzfile);
        gzclose(oth_xml_gzfile);
        return 0;
    }

    fil_reader = xmlReaderForIO(xmlInputReadCallback_gz_compressed,
                                xmlInputCloseCallback_gz_compressed,
                                &fil_xml_gzfile,
                                NULL,
                                NULL,
                                XML_PARSE_NOBLANKS);
    if (!fil_reader) {
        g_critical(MODULE"load_gz_compressed_xml_metadata: Reader for filelists.xml.gz file failed");
        xmlFreeTextReader(pri_reader);
        gzclose(fil_xml_gzfile);
        gzclose(oth_xml_gzfile);
        return 0;
    }

    oth_reader = xmlReaderForIO(xmlInputReadCallback_gz_compressed,
                                xmlInputCloseCallback_gz_compressed,
                                &oth_xml_gzfile,
                                NULL,
                                NULL,
                                XML_PARSE_NOBLANKS);
    if (!oth_reader) {
        g_critical(MODULE"load_gz_compressed_xml_metadata: Reader for other.xml.gz file failed");
        xmlFreeTextReader(pri_reader);
        xmlFreeTextReader(fil_reader);
        gzclose(oth_xml_gzfile);
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
    if (strcmp(name, "repomd")) {
        g_warning(MODULE"locate_metadata_via_repomd: Bad xml - missing repomd element? (%s)", name);
        xmlFree(name);
        xmlFreeTextReader(reader);
        g_free(repomd);
        return NULL;
    }
    xmlFree(name);

    ret = xmlTextReaderRead(reader);
    name = xmlTextReaderName(reader);
    if (strcmp(name, "revision")) {
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
        if (!strcmp(name, "data")) {
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

    char *data_type = NULL;
    char *location_href = NULL;

    while (ret) {
        if (xmlTextReaderNodeType(reader) != 1) {
            ret = xmlTextReaderNext(reader);
            continue;
        }

        xmlNodePtr data_node = xmlTextReaderExpand(reader);
        data_type = xmlGetProp(data_node, "type");
        xmlNodePtr sub_node = data_node->children;

        while (sub_node) {
            if (sub_node->type != XML_ELEMENT_NODE) {
                sub_node = xmlNextElementSibling(sub_node);
                continue;
            }

            if (!strcmp(sub_node->name, "location")) {
                location_href = xmlGetProp(sub_node, "href");
            }

            // TODO: Check repodata validity checksum? mtime? size?

            sub_node = xmlNextElementSibling(sub_node);
        }


        // Build absolute path

        gchar *full_location_href;
        if (repopath_ends_with_slash) {
            full_location_href = g_strconcat(repopath, location_href, NULL);
        } else {
            full_location_href = g_strconcat(repopath, "/", location_href, NULL);
        }


        // Store the path

        if (!strcmp(data_type, "primary")) {
            mdloc->pri_xml_href = full_location_href;
        } else if (!strcmp(data_type, "filelists")) {
            mdloc->fil_xml_href = full_location_href;
        } else if (!strcmp(data_type, "other")) {
            mdloc->oth_xml_href = full_location_href;
        } else if (!strcmp(data_type, "primary_db")) {
            mdloc->pri_sqlite_href = full_location_href;
        } else if (!strcmp(data_type, "filelists_db")) {
            mdloc->fil_sqlite_href = full_location_href;
        } else if (!strcmp(data_type, "other_db")) {
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

    if(g_str_has_suffix(ml->pri_xml_href, ".gz") &&
       g_str_has_suffix(ml->fil_xml_href, ".gz") &&
       g_str_has_suffix(ml->oth_xml_href, ".gz"))
    {
        result = load_gz_compressed_xml_metadata(hashtable, ml->pri_xml_href, ml->fil_xml_href, ml->oth_xml_href);
    }
    else if(g_str_has_suffix(ml->pri_xml_href, ".xml") &&
              g_str_has_suffix(ml->fil_xml_href, ".xml") &&
              g_str_has_suffix(ml->oth_xml_href, ".xml"))
    {
        result = load_xml_metadata(hashtable, ml->pri_xml_href, ml->fil_xml_href, ml->oth_xml_href);
    }
    else
    {
        g_warning("locate_and_load_xml_metadata: Metadata uses unsupported type of compression");
        free_metadata_location(ml);
        return 0;
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
    while (file = g_dir_read_name (repodir)) {
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
