#include <glib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <zlib.h>
#include "logging.h"
#include "misc.h"
#include "repomd.h"

#define DEFAULT_CHECKSUM                "sha256"
#define DEFAULT_CHECKSUM_ENUM_VAL        PKG_CHECKSUM_SHA256

#define GZ_BUFFER_SIZE   131072  // 1024*128



typedef struct _contentStat {
    char *checksum;
    long size;
} contentStat;


repomdData *new_repomddata()
{
    return (repomdData *) g_malloc0(sizeof(repomdData));
}



void free_repomddata(repomdData *md)
{
    g_free(md);
}



contentStat *get_gz_compressed_content_stat(const char *filename, ChecksumType checksum_type)
{
    if (!g_file_test(filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
        return NULL;
    }


    // Open compressed file

    gzFile gzfile;
    if (!(gzfile = gzopen(filename, "rb"))) {
        return NULL;
    }
    gzbuffer(gzfile, GZ_BUFFER_SIZE);


    // Create checksum structure

    GChecksumType gchecksumtype;
    switch (checksum_type) {
        case PKG_CHECKSUM_MD5:
            gchecksumtype = G_CHECKSUM_MD5;
            break;
        case PKG_CHECKSUM_SHA1:
            gchecksumtype = G_CHECKSUM_SHA1;
            break;
        case PKG_CHECKSUM_SHA256:
            gchecksumtype = G_CHECKSUM_SHA256;
            break;
    };


    // Read compressed file and calculate checksum and size

    GChecksum *checksum = g_checksum_new(gchecksumtype);
    long size = 0;

    long readed;
    unsigned char buffer[GZ_BUFFER_SIZE];

    do {
        readed = gzread(gzfile, (void *) buffer, GZ_BUFFER_SIZE);
        g_checksum_update (checksum, buffer, readed);
        size += readed;
    } while (readed == GZ_BUFFER_SIZE);


    // Create result structure

    contentStat* result = g_malloc(sizeof(contentStat));
    if (result) {
        result->checksum = g_strdup(g_checksum_get_string(checksum));
        result->size = size;
    }


    // Clean up

    g_checksum_free(checksum);
    gzclose(gzfile);

    return result;
}



int fill_missing_data(const char *base_path, repomdData *md) {
    if (!md || !(md->location_href) || !strlen(md->location_href)) {
        // Nothing to do
        return 0;
    }

    gchar *path = g_strconcat(base_path, "/", md->location_href, NULL);

    if (!g_file_test(path, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR)) {
        return 0;
    }


    // Compute checksum of compressed file

    if (!md->checksum_type || !md->checksum) {
        md->checksum_type = g_strdup(DEFAULT_CHECKSUM);
        md->checksum = compute_file_checksum(path, DEFAULT_CHECKSUM_ENUM_VAL);
    }


    // Compute checksum of non compressed content and its size

    if (!md->checksum_open_type || !md->checksum_open || !md->size_open) {
        if (g_str_has_suffix(path, ".gz") || g_str_has_suffix(path, ".gzip")) {
            // File compressed by gzip
            contentStat *open_stat = NULL;
            open_stat = get_gz_compressed_content_stat(path, DEFAULT_CHECKSUM_ENUM_VAL);
            md->checksum_open_type = g_strdup(DEFAULT_CHECKSUM);
            md->checksum_open = open_stat->checksum;
            md->size_open = open_stat->size;
            g_free(open_stat);
        } else {
            // Unknown suffix -> unknown compression
            g_warning( "File \"%s\" compressed by an unsupported type of compression", path);
            md->checksum_open_type = g_strdup("UNKNOWN");
            md->checksum_open = g_strdup("file_compressed_by_an_unsupported_type_of_compression");
            md->size_open = -1;
        }
    }


    // Get timestamp and size of compressed file

    if (!md->timestamp || !md->size) {
        struct stat buf;
        if (!stat(path, &buf)) {
            md->timestamp = buf.st_mtime;
            md->size      = buf.st_size;
        } else {
            g_warning( "Stat on file \"%s\" failed", path);
        }
    }


    // Set db version

    if (!md->db_ver) {
        md->db_ver = 10;
    }
}



void dump_data_items(xmlTextWriterPtr writer, repomdData *md, const char *type)
{
    if (!writer || !md || !type) {
        return;
    }

    xmlTextWriterStartElement(writer, BAD_CAST "data");
    xmlTextWriterWriteAttribute(writer, BAD_CAST "type", type);

    xmlTextWriterStartElement(writer, BAD_CAST "checksum");
    xmlTextWriterWriteAttribute(writer, BAD_CAST "type", md->checksum_type);
    xmlTextWriterWriteString(writer, BAD_CAST md->checksum);
    xmlTextWriterEndElement(writer);

    xmlTextWriterStartElement(writer, BAD_CAST "open-checksum");
    xmlTextWriterWriteAttribute(writer, BAD_CAST "type", md->checksum_open_type);
    xmlTextWriterWriteString(writer, BAD_CAST md->checksum_open);
    xmlTextWriterEndElement(writer);

    xmlTextWriterStartElement(writer, BAD_CAST "location");
    xmlTextWriterWriteAttribute(writer, BAD_CAST "href", md->location_href);
    xmlTextWriterEndElement(writer);

    xmlTextWriterStartElement(writer, BAD_CAST "timestamp");
    xmlTextWriterWriteFormatString(writer, "%ld", md->timestamp);
    xmlTextWriterEndElement(writer);

    xmlTextWriterStartElement(writer, BAD_CAST "size");
    xmlTextWriterWriteFormatString(writer, "%ld", md->size);
    xmlTextWriterEndElement(writer);

    xmlTextWriterStartElement(writer, BAD_CAST "open-size");
    xmlTextWriterWriteFormatString(writer, "%ld", md->size_open);
    xmlTextWriterEndElement(writer);

    if (g_str_has_suffix(type, "_db")) {
        xmlTextWriterStartElement(writer, BAD_CAST "database_version");
        xmlTextWriterWriteFormatString(writer, "%ld", md->db_ver);
        xmlTextWriterEndElement(writer);
    }

    xmlTextWriterEndElement(writer);  // data element end
}


char *repomd_xml_dump(long revision, repomdData *pri_xml, repomdData *fil_xml, repomdData *oth_xml,
                 repomdData *pri_sqlite, repomdData *fil_sqlite, repomdData *oth_sqlite)
{
    xmlBufferPtr buf = xmlBufferCreate();
    if (!buf) {
        g_critical("Error creating the xml buffer");
        return NULL;
    }

    xmlTextWriterPtr writer = xmlNewTextWriterMemory(buf, 0);
    if (writer == NULL) {
        g_critical("Error creating the xml writer");
        return NULL;
    }


    // Start of XML document

    int rc;

    rc = xmlTextWriterStartDocument(writer, NULL, NULL, NULL);
    if (rc < 0) {
        g_critical("Error at xmlTextWriterStartDocument\n");
        return NULL;
    }

    rc = xmlTextWriterStartElement(writer, BAD_CAST "repomd");
    if (rc < 0) {
        g_critical("Error at xmlTextWriterStartElement repomd\n");
        return NULL;
    }

    rc = xmlTextWriterStartElement(writer, BAD_CAST "revision");
    if (rc < 0) {
        g_critical("Error at xmlTextWriterStartElement revision\n");
        return NULL;
    }

    xmlTextWriterWriteFormatString(writer, "%ld", revision);

    xmlTextWriterEndElement(writer); // revision element end

    dump_data_items(writer, pri_xml, "primary");
    dump_data_items(writer, fil_xml, "filelists");
    dump_data_items(writer, oth_xml, "other");
    dump_data_items(writer, pri_sqlite, "primary_db");
    dump_data_items(writer, fil_sqlite, "filelists_db");
    dump_data_items(writer, oth_sqlite, "other_db");

    xmlTextWriterEndElement(writer); // repomd element end

    xmlFreeTextWriter(writer);

    char *result = g_strdup((char*) buf->content);


    // Clean up

    xmlBufferFree(buf);

    return result;
}



char *xml_repomd(const char *path, repomdData *pri_xml, repomdData *fil_xml, repomdData *oth_xml,
                 repomdData *pri_sqlite, repomdData *fil_sqlite, repomdData *oth_sqlite)
{
    if (!path) {
        return NULL;
    }


    // Fill all missing stuff

    fill_missing_data(path, pri_xml);
    fill_missing_data(path, fil_xml);
    fill_missing_data(path, oth_xml);
    fill_missing_data(path, pri_sqlite);
    fill_missing_data(path, fil_sqlite);
    fill_missing_data(path, oth_sqlite);


    // Get revision

    long revision = (long) time(NULL);


    // Dump xml

    char *res = repomd_xml_dump(revision, pri_xml, fil_xml, oth_xml, pri_sqlite, fil_sqlite, oth_sqlite);

    return res;
}
