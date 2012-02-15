#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <zlib.h>
#include <libxml/xmlreader.h>
#include "load_metadata.h"


void free_values(gpointer data)
{
    struct package_metadata *md = (struct package_metadata *) data;
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


void *destroy_old_metadata_hashtable(GHashTable *hashtable)
{
    if (hashtable) {
        g_hash_table_destroy (hashtable);
    }
}


int xmlInputReadCallback_plaintext (void * context, char * buffer, int len)
{
    int readed = fread(buffer, 1, len, (FILE *) context);
    if (readed != len && !feof((FILE *) context)) {
        // This is sad
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



void processNode(GHashTable *metadata, xmlTextReaderPtr pri_reader,
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
//    char *name = NULL;
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

//        if (!strcmp(node->name, "name")) {
//            char *name = xmlNodeGetContent(node);
//            puts(name);
//        }
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
        puts("Warning: Bad xml data");
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
        printf("Warning: Key \"%s\" already exists\n", key);
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
    struct package_metadata *pkg_md = g_malloc(sizeof(struct package_metadata));
    pkg_md->time_file = time_file;
    pkg_md->size_package = size;
    pkg_md->location_href = location_href;
    pkg_md->location_base = location_base;
    pkg_md->checksum_type = checksum_type;
    pkg_md->primary_xml = pri_pkg_xml;
    pkg_md->filelists_xml = fil_pkg_xml;
    pkg_md->other_xml = oth_pkg_xml;
    g_hash_table_insert(metadata, key, pkg_md);

//    printf("%s | %s | %s | %d | %d\n", location_href, location_base, checksum_type, time_file, size);
//    printf("PRIMARY:\n%s\nFILELISTS:\n%s\nOTHER:\n%s\n", pri_pkg_xml, fil_pkg_xml, oth_pkg_xml);
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
        puts("bad xml");
        xmlFree(name);
        return 0;
    }
    xmlFree(name);

    fil_ret = xmlTextReaderRead(fil_reader);
    name = xmlTextReaderName(fil_reader);
    if (strcmp(name, "package")) {
        puts("bad xml");
        xmlFree(name);
        return 0;
    }
    xmlFree(name);

    oth_ret = xmlTextReaderRead(oth_reader);
    name = xmlTextReaderName(oth_reader);
    if (strcmp(name, "package")) {
        puts("bad xml");
        xmlFree(name);
        return 0;
    }
    xmlFree(name);

    while (pri_ret && fil_ret && oth_ret) {
        processNode(hashtable, pri_reader, fil_reader, oth_reader);
        pri_ret = xmlTextReaderNext(pri_reader);
        fil_ret = xmlTextReaderNext(fil_reader);
        oth_ret = xmlTextReaderNext(oth_reader);
    }

    return 1;
}


#define GZ_BUFFER_SIZE   131072  // 1024*128

int load_gz_compressed_xml_metadata_2(GHashTable *hashtable, const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path)
{
    if (!hashtable) {
        return 0;
    }

    GFileTest flags = G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR;
    if (!g_file_test(primary_xml_path, flags) ||
        !g_file_test(filelists_xml_path, flags) ||
        !g_file_test(other_xml_path, flags))
    {
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
        puts("pri Reader fail");
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
        xmlFreeTextReader(pri_reader);
        gzclose(fil_xml_gzfile);
        gzclose(oth_xml_gzfile);
        puts("Fil Reader fail");
        return 0;
    }

    oth_reader = xmlReaderForIO(xmlInputReadCallback_gz_compressed,
                                xmlInputCloseCallback_gz_compressed,
                                &oth_xml_gzfile,
                                NULL,
                                NULL,
                                XML_PARSE_NOBLANKS);
    if (!oth_reader) {
        xmlFreeTextReader(pri_reader);
        xmlFreeTextReader(fil_reader);
        gzclose(oth_xml_gzfile);
        puts("Oth Reader fail");
        return 0;
    }

    int result = parse_xml_metadata(hashtable, pri_reader, fil_reader, oth_reader);

    xmlFreeTextReader(pri_reader);
    xmlFreeTextReader(fil_reader);
    xmlFreeTextReader(oth_reader);

    return result;
}


int load_xml_metadata_2(GHashTable *hashtable, const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path)
{
    if (!hashtable) {
        return 0;
    }

    GFileTest flags = G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR;
    if (!g_file_test(primary_xml_path, flags) ||
        !g_file_test(filelists_xml_path, flags) ||
        !g_file_test(other_xml_path, flags))
    {
        puts("file fail");
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
        fclose(pri_xml_file);
        fclose(fil_xml_file);
        fclose(oth_xml_file);
        puts("pri Reader fail");
        return 0;
    }

    fil_reader = xmlReaderForIO(xmlInputReadCallback_plaintext,
                                xmlInputCloseCallback_plaintext,
                                fil_xml_file,
                                NULL,
                                NULL,
                                XML_PARSE_NOBLANKS);
    if (!fil_reader) {
        xmlFreeTextReader(pri_reader);
        fclose(fil_xml_file);
        fclose(oth_xml_file);
        puts("Fil Reader fail");
        return 0;
    }

    oth_reader = xmlReaderForIO(xmlInputReadCallback_plaintext,
                                xmlInputCloseCallback_plaintext,
                                oth_xml_file,
                                NULL,
                                NULL,
                                XML_PARSE_NOBLANKS);
    if (!oth_reader) {
        xmlFreeTextReader(pri_reader);
        xmlFreeTextReader(fil_reader);
        fclose(oth_xml_file);
        puts("Oth Reader fail");
        return 0;
    }

    int result = parse_xml_metadata(hashtable, pri_reader, fil_reader, oth_reader);

    xmlFreeTextReader(pri_reader);
    xmlFreeTextReader(fil_reader);
    xmlFreeTextReader(oth_reader);

    return result;
}


int locate_and_load_xml_metadata_2(GHashTable *hashtable, const char *repopath)
{
    // TODO: First try get info from repomd.xml

    if (!hashtable || !repopath || !g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        return 0;
    }

    gchar *pri_gz_xml = NULL;
    gchar *fil_gz_xml = NULL;
    gchar *oth_gz_xml = NULL;
    gchar *pri_xml = NULL;
    gchar *fil_xml = NULL;
    gchar *oth_xml = NULL;

    GDir *repodir = g_dir_open(repopath, 0, NULL);
    if (!repodir) {
        return 0;
    }

    gchar *file;
    while (file = g_dir_read_name (repodir)) {
        if (g_str_has_suffix(file, "primary.xml.gz")) {
            pri_gz_xml = g_strconcat (repopath, file, NULL);
        } else if (g_str_has_suffix(file, "filelists.xml.gz")) {
            fil_gz_xml = g_strconcat (repopath, file, NULL);
        } else if (g_str_has_suffix(file, "other.xml.gz")) {
            oth_gz_xml = g_strconcat (repopath, file, NULL);
        } else if (g_str_has_suffix(file, "primary.xml")) {
            pri_xml = g_strconcat (repopath, file, NULL);
        } else if (g_str_has_suffix(file, "filelists.xml")) {
            fil_xml = g_strconcat (repopath, file, NULL);
        } else if (g_str_has_suffix(file, "other.xml")) {
            oth_xml = g_strconcat (repopath, file, NULL);
        }
    }

    g_dir_close(repodir);

    int result = 0;

//    xmlInitParser();

    if (pri_gz_xml && fil_gz_xml && oth_gz_xml) {
        result = load_gz_compressed_xml_metadata_2(hashtable, pri_gz_xml, fil_gz_xml, oth_gz_xml);
    } else if (pri_xml && fil_xml && oth_xml) {
        result = load_xml_metadata_2(hashtable, pri_xml, fil_xml, oth_xml);
    }

    // Cleanup memory allocated by libxml2
//    xmlCleanupParser();

    g_free(pri_gz_xml);
    g_free(fil_gz_xml);
    g_free(oth_gz_xml);
    g_free(pri_xml);
    g_free(fil_xml);
    g_free(oth_xml);

    return result;
}

