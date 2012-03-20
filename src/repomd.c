#include <glib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <zlib.h>
#include "logging.h"
#include "misc.h"
#include "repomd.h"
#include "compression_wrapper.h"

#undef MODULE
#define MODULE "repomd: "


#define DEFAULT_CHECKSUM                "sha256"
#define DEFAULT_CHECKSUM_ENUM_VAL        PKG_CHECKSUM_SHA256

#define DEFAULT_DATABASE_VERSION        10

#define BUFFER_SIZE          8192

#define RPM_NS          "http://linux.duke.edu/metadata/rpm"
#define XMLNS_NS        "http://linux.duke.edu/metadata/repo"


#define REPOMD_OK       0
#define REPOMD_ERR      1


struct repomdData {
    const char *location_href;
    char *checksum;
    char *checksum_type;
    char *checksum_open;
    char *checksum_open_type;
    long timestamp;
    long size;
    long size_open;
    int db_ver;

    GStringChunk *chunk;
};


typedef struct _contentStat {
    char *checksum;
    long size;
} contentStat;


struct repomdData *new_repomddata();
void free_repomddata(struct repomdData *);
void free_repomdresult(struct repomdResult *);


struct repomdData *new_repomddata()
{
    struct repomdData *md = (struct repomdData *) g_malloc0(sizeof(struct repomdData));
    md->chunk = g_string_chunk_new(1024);
    return md;
}



void free_repomddata(struct repomdData *md)
{
    if (!md) {
        return;
    }

    g_string_chunk_free(md->chunk);
    g_free(md);
}



void free_repomdresult(struct repomdResult *rr)
{
    if (!rr) {
        return;
    }

    g_free(rr->pri_xml_location);
    g_free(rr->fil_xml_location);
    g_free(rr->oth_xml_location);
    g_free(rr->pri_sqlite_location);
    g_free(rr->fil_sqlite_location);
    g_free(rr->oth_sqlite_location);
    g_free(rr->groupfile_location);
    g_free(rr->cgroupfile_location);
    g_free(rr->repomd_xml);

    g_free(rr);
}



contentStat *get_compressed_content_stat(const char *filename, ChecksumType checksum_type)
{
    if (!g_file_test(filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
        return NULL;
    }


    // Open compressed file

    CW_FILE *cwfile;
    if (!(cwfile = cw_open(filename, CW_MODE_READ, AUTO_DETECT_COMPRESSION))) {
        return NULL;
    }


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
        default:
            g_critical(MODULE"%s: Unknown checksum type", __func__);
            return NULL;
    };


    // Read compressed file and calculate checksum and size

    GChecksum *checksum = g_checksum_new(gchecksumtype);
    if (!checksum) {
        g_critical(MODULE"%s: g_checksum_new() failed", __func__);
        return NULL;
    }

    long size = 0;
    long readed;
    unsigned char buffer[BUFFER_SIZE];

    do {
        readed = cw_read(cwfile, (void *) buffer, BUFFER_SIZE);
        g_checksum_update (checksum, buffer, readed);
        size += readed;
    } while (readed == BUFFER_SIZE);


    // Create result structure

    contentStat* result = g_malloc(sizeof(contentStat));
    if (result) {
        result->checksum = g_strdup(g_checksum_get_string(checksum));
        result->size = size;
    }


    // Clean up

    g_checksum_free(checksum);
    cw_close(cwfile);

    return result;
}



int fill_missing_data(const char *base_path, struct repomdData *md, ChecksumType *checksum_type) {
    if (!md || !(md->location_href) || !strlen(md->location_href)) {
        // Nothing to do
        return REPOMD_ERR;
    }

    const char *checksum_str = DEFAULT_CHECKSUM;
    ChecksumType checksum_t = DEFAULT_CHECKSUM_ENUM_VAL;

    if (checksum_type) {
        checksum_str = get_checksum_name_str(*checksum_type);
        checksum_t = *checksum_type;
    }

    gchar *path = g_strconcat(base_path, "/", md->location_href, NULL);

    if (!g_file_test(path, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR)) {
        // File doesn't exists
        g_warning(MODULE"%s: File %s doesn't exists", __func__, path);
        return REPOMD_ERR;
    }


    // Compute checksum of compressed file

    if (!md->checksum_type || !md->checksum) {
        gchar *chksum;
        md->checksum_type = g_string_chunk_insert(md->chunk, checksum_str);
        chksum = compute_file_checksum(path, checksum_t);
        md->checksum = g_string_chunk_insert(md->chunk, chksum);
        g_free(chksum);
    }


    // Compute checksum of non compressed content and its size

    if (!md->checksum_open_type || !md->checksum_open || !md->size_open) {
        if (detect_compression(path) != UNKNOWN_COMPRESSION &&
            detect_compression(path) != NO_COMPRESSION)
        {
            // File compressed by supported algorithm
            contentStat *open_stat = NULL;
            open_stat = get_compressed_content_stat(path, checksum_t);
            md->checksum_open_type = g_string_chunk_insert(md->chunk, checksum_str);
            md->checksum_open = g_string_chunk_insert(md->chunk, open_stat->checksum);
            if (!md->size_open) {
                md->size_open = open_stat->size;
            }
            g_free(open_stat->checksum);
            g_free(open_stat);
        } else {
            // Unknown compression
            g_warning(MODULE"%s: File \"%s\" compressed by an unsupported type of compression", __func__, path);
            md->checksum_open_type = g_string_chunk_insert(md->chunk, "UNKNOWN");
            md->checksum_open = g_string_chunk_insert(md->chunk, "file_compressed_by_an_unsupported_type_of_compression");
            md->size_open = -1;
        }
    }


    // Get timestamp and size of compressed file

    if (!md->timestamp || !md->size) {
        struct stat buf;
        if (!stat(path, &buf)) {
            if (!md->timestamp) {
                md->timestamp = buf.st_mtime;
            }
            if (!md->size) {
                md->size = buf.st_size;
            }
        } else {
            g_warning(MODULE"%s: Stat on file \"%s\" failed", __func__, path);
        }
    }


    // Set db version

    if (!md->db_ver) {
        md->db_ver = DEFAULT_DATABASE_VERSION;
    }

    g_free(path);

    return REPOMD_OK;
}


void process_groupfile(const char *base_path, struct repomdData *groupfile,
                       struct repomdData *cgroupfile, ChecksumType *checksum_type)
{
    if (!groupfile || !(groupfile->location_href) || !strlen(groupfile->location_href) || !cgroupfile) {
        return;
    }


    // Checksum stuff

    const char *checksum_str = DEFAULT_CHECKSUM;
    ChecksumType checksum_t = DEFAULT_CHECKSUM_ENUM_VAL;

    if (checksum_type) {
        checksum_str = get_checksum_name_str(*checksum_type);
        checksum_t = *checksum_type;
    }


    // Paths

    gchar *clocation_href = g_strconcat(groupfile->location_href, ".gz", NULL);
    cgroupfile->location_href = g_string_chunk_insert(cgroupfile->chunk, clocation_href);
    g_free(clocation_href);

    gchar *path = g_strconcat(base_path, "/", groupfile->location_href, NULL);
    gchar *cpath = g_strconcat(base_path, "/", cgroupfile->location_href, NULL);

    if (!g_file_test(path, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_REGULAR)) {
        // File doesn't exists
        g_warning(MODULE"%s: File %s doesn't exists", __func__, path);
        g_free(path);
        g_free(cpath);
        return;
    }


    // Compress file + get size of non compressed file

    int readed;
    char buf[BUFFER_SIZE];
    CW_FILE *cw_plain;
    CW_FILE *cw_compressed;

    cw_plain = cw_open(path, CW_MODE_READ, NO_COMPRESSION);
    cw_compressed = cw_open(cpath, CW_MODE_WRITE, GZ_COMPRESSION);

    while ((readed = cw_read(cw_plain, buf, BUFFER_SIZE)) > 0) {
        if (cw_write(cw_compressed, buf, (unsigned int) readed) == CW_ERR) {
            g_debug(MODULE"%s: Error while groupfile compression", __func__);
            break;
        }
    }

    cw_close(cw_compressed);
    cw_close(cw_plain);

    if (readed == CW_ERR) {
        g_debug(MODULE"%s: Error while groupfile compression", __func__);
    }


    // Compute checksums

    gchar *checksum;
    gchar *cchecksum;
    checksum = compute_file_checksum(path, checksum_t);
    cchecksum = compute_file_checksum(cpath, checksum_t);


    // Get stats

    long gf_size = -1, cgf_size = -1;
    long gf_time = -1, cgf_time = -1;
    struct stat gf_stat, cgf_stat;

    if (stat(path, &gf_stat)) {
        g_debug(MODULE"%s: Error while stat() on %s", __func__, path);
    } else {
        gf_size = gf_stat.st_size;
        gf_time = gf_stat.st_mtime;
    }

    if (stat(cpath, &cgf_stat)) {
        g_debug(MODULE"%s: Error while stat() on %s", __func__, path);
    } else {
        cgf_size = cgf_stat.st_size;
        cgf_time = cgf_stat.st_mtime;
    }


    // Results

    groupfile->checksum = g_string_chunk_insert(groupfile->chunk, checksum);
    groupfile->checksum_type = g_string_chunk_insert(groupfile->chunk, checksum_str);
    groupfile->checksum_open = NULL;
    groupfile->checksum_open_type = NULL;
    groupfile->timestamp = gf_time;
    groupfile->size = gf_size;
    groupfile->size_open = -1;

    cgroupfile->checksum = g_string_chunk_insert(cgroupfile->chunk, cchecksum);
    cgroupfile->checksum_type = g_string_chunk_insert(cgroupfile->chunk, checksum_str);
    cgroupfile->checksum_open = g_string_chunk_insert(groupfile->chunk, checksum);
    cgroupfile->checksum_open_type = g_string_chunk_insert(groupfile->chunk, checksum_str);
    cgroupfile->timestamp = cgf_time;
    cgroupfile->size = cgf_size;
    cgroupfile->size_open = gf_size;

    g_free(checksum);
    g_free(cchecksum);
    g_free(path);
    g_free(cpath);
}


void dump_data_items(xmlTextWriterPtr writer, struct repomdData *md, const xmlChar *type)
{
    if (!writer || !md || !type) {
        return;
    }

    xmlTextWriterStartElement(writer, BAD_CAST "data");
    xmlTextWriterWriteAttribute(writer, BAD_CAST "type", type);

    xmlTextWriterStartElement(writer, BAD_CAST "checksum");
    xmlTextWriterWriteAttribute(writer, BAD_CAST "type", (xmlChar *) md->checksum_type);
    xmlTextWriterWriteString(writer, BAD_CAST md->checksum);
    xmlTextWriterEndElement(writer);

    if (md->checksum_open) {
        xmlTextWriterStartElement(writer, BAD_CAST "open-checksum");
        xmlTextWriterWriteAttribute(writer, BAD_CAST "type", (xmlChar *) md->checksum_open_type);
        xmlTextWriterWriteString(writer, BAD_CAST md->checksum_open);
        xmlTextWriterEndElement(writer);
    }

    xmlTextWriterStartElement(writer, BAD_CAST "location");
    xmlTextWriterWriteAttribute(writer, BAD_CAST "href", (xmlChar *) md->location_href);
    xmlTextWriterEndElement(writer);

    xmlTextWriterStartElement(writer, BAD_CAST "timestamp");
    xmlTextWriterWriteFormatString(writer, "%ld", md->timestamp);
    xmlTextWriterEndElement(writer);

    xmlTextWriterStartElement(writer, BAD_CAST "size");
    xmlTextWriterWriteFormatString(writer, "%ld", md->size);
    xmlTextWriterEndElement(writer);

    if (md->size_open != -1) {
        xmlTextWriterStartElement(writer, BAD_CAST "open-size");
        xmlTextWriterWriteFormatString(writer, "%ld", md->size_open);
        xmlTextWriterEndElement(writer);
    }

    if (g_str_has_suffix((char *)type, "_db")) {
        xmlTextWriterStartElement(writer, BAD_CAST "database_version");
        xmlTextWriterWriteFormatString(writer, "%d", md->db_ver);
        xmlTextWriterEndElement(writer);
    }

    xmlTextWriterEndElement(writer);  // data element end
}


char *repomd_xml_dump(long revision, struct repomdData *pri_xml, struct repomdData *fil_xml, struct repomdData *oth_xml,
                 struct repomdData *pri_sqlite, struct repomdData *fil_sqlite, struct repomdData *oth_sqlite,
                 struct repomdData *groupfile, struct repomdData *cgroupfile)
{
    xmlBufferPtr buf = xmlBufferCreate();
    if (!buf) {
        g_critical(MODULE"%s: Error creating the xml buffer", __func__);
        return NULL;
    }

    xmlTextWriterPtr writer = xmlNewTextWriterMemory(buf, 0);
    if (writer == NULL) {
        g_critical(MODULE"%s: Error creating the xml writer", __func__);
        return NULL;
    }


    // Start of XML document

    int rc;

    rc = xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL);
    if (rc < 0) {
        g_critical(MODULE"%s: Error at xmlTextWriterStartDocument\n", __func__);
        return NULL;
    }

    rc = xmlTextWriterStartElement(writer, BAD_CAST "repomd");
    if (rc < 0) {
        g_critical(MODULE"%s: Error at xmlTextWriterStartElement repomd\n", __func__);
        return NULL;
    }
    xmlTextWriterWriteAttribute(writer, BAD_CAST "xmlns:rpm", (xmlChar *) RPM_NS);
    xmlTextWriterWriteAttribute(writer, BAD_CAST "xmlns", (xmlChar *) XMLNS_NS);

    rc = xmlTextWriterStartElement(writer, BAD_CAST "revision");
    if (rc < 0) {
        g_critical(MODULE"%s: Error at xmlTextWriterStartElement revision\n", __func__);
        return NULL;
    }

    xmlTextWriterWriteFormatString(writer, "%ld", revision);

    xmlTextWriterEndElement(writer); // revision element end

    dump_data_items(writer, pri_xml, (const xmlChar *) "primary");
    dump_data_items(writer, fil_xml, (const xmlChar *) "filelists");
    dump_data_items(writer, oth_xml, (const xmlChar *) "other");
    dump_data_items(writer, pri_sqlite, (const xmlChar *) "primary_db");
    dump_data_items(writer, fil_sqlite, (const xmlChar *) "filelists_db");
    dump_data_items(writer, oth_sqlite, (const xmlChar *) "other_db");
    dump_data_items(writer, groupfile, (const xmlChar *) "group");
    dump_data_items(writer, cgroupfile, (const xmlChar *) "group_gz");

    xmlTextWriterEndElement(writer); // repomd element end

    xmlFreeTextWriter(writer);

    char *result = g_strdup((char*) buf->content);


    // Clean up

    xmlBufferFree(buf);

    return result;
}


void rename_file(const char *base_path, struct repomdData *md)
{
    if (!md || !(md->location_href) || !strlen(md->location_href)) {
        return;
    }

    if (!md->checksum) {
        return;
    }

    gchar *path = g_strconcat(base_path, "/", md->location_href, NULL);
    gchar *location_href_path_prefix = NULL;
    const gchar *location_href_filename_only = NULL;

    int x = strlen(md->location_href);
    for (; x > 0; x--) {
        if (md->location_href[x] == '/') {
            location_href_path_prefix = g_strndup(md->location_href, x+1);
            location_href_filename_only = md->location_href + x + 1;
            break;
        }
    }

    gchar *new_location_href = g_strconcat(location_href_path_prefix, md->checksum, "-", location_href_filename_only, NULL);
    gchar *new_path = g_strconcat(base_path, "/", new_location_href, NULL);

    g_free(location_href_path_prefix);

    // Rename
    if (g_file_test (new_path, G_FILE_TEST_EXISTS)) {
        if (remove(new_path)) {
            g_critical(MODULE"%s: Cannot delete old %s", __func__, new_path);
            g_free(path);
            g_free(new_location_href);
            g_free(new_path);
            return;
        }
    }
    if (rename(path, new_path)) {
        g_critical(MODULE"%s: Cannot rename %s to %s", __func__, path, new_path);
        g_free(path);
        g_free(new_location_href);
        g_free(new_path);
        return;
    }

    md->location_href = g_string_chunk_insert(md->chunk, new_location_href);

    g_free(path);
    g_free(new_path);
    g_free(new_location_href);
}


// groupfile is expected uncompressed!
struct repomdResult *xml_repomd_2(const char *path, int rename_to_unique,
                                  struct repomdData *pri_xml, struct repomdData *fil_xml, struct repomdData *oth_xml,
                                  struct repomdData *pri_sqlite, struct repomdData *fil_sqlite, struct repomdData *oth_sqlite,
                                  struct repomdData *groupfile, struct repomdData *cgroupfile, ChecksumType *checksum_type)
{
    if (!path) {
        return NULL;
    }

    struct repomdResult *res = g_malloc(sizeof(struct repomdResult));


    // Fill all missing stuff

    fill_missing_data(path, pri_xml, checksum_type);
    fill_missing_data(path, fil_xml, checksum_type);
    fill_missing_data(path, oth_xml, checksum_type);
    fill_missing_data(path, pri_sqlite, checksum_type);
    fill_missing_data(path, fil_sqlite, checksum_type);
    fill_missing_data(path, oth_sqlite, checksum_type);

    process_groupfile(path, groupfile, cgroupfile, checksum_type);


    // Include checksum in the metadata filename

    if (rename_to_unique) {
        rename_file(path, pri_xml);
        rename_file(path, fil_xml);
        rename_file(path, oth_xml);
        rename_file(path, pri_sqlite);
        rename_file(path, fil_sqlite);
        rename_file(path, oth_sqlite);
        rename_file(path, groupfile);
        rename_file(path, cgroupfile);
    }


    // Set locations in output structure

    res->pri_xml_location = pri_xml ? g_strdup(pri_xml->location_href) : NULL;
    res->fil_xml_location = fil_xml ? g_strdup(fil_xml->location_href) : NULL;
    res->oth_xml_location = oth_xml ? g_strdup(oth_xml->location_href) : NULL;
    res->pri_sqlite_location = pri_sqlite ? g_strdup(pri_sqlite->location_href) : NULL;
    res->fil_sqlite_location = fil_sqlite ? g_strdup(fil_sqlite->location_href) : NULL;
    res->oth_sqlite_location = oth_sqlite ? g_strdup(oth_sqlite->location_href) : NULL;
    res->groupfile_location = groupfile ? g_strdup(groupfile->location_href) : NULL;
    res->cgroupfile_location = cgroupfile ? g_strdup(cgroupfile->location_href) : NULL;

    // Get revision

    long revision = (long) time(NULL);


    // Dump xml

    res->repomd_xml = repomd_xml_dump(revision, pri_xml, fil_xml, oth_xml, pri_sqlite, fil_sqlite, oth_sqlite, groupfile, cgroupfile);

    return res;
}



struct repomdResult *xml_repomd(const char *path, int rename_to_unique, const char *pri_xml, const char *fil_xml, const char *oth_xml,
                 const char *pri_sqlite, const char *fil_sqlite, const char *oth_sqlite, const char *groupfile, ChecksumType *checksum_type)
{
    if (!path) {
        return NULL;
    }

    struct repomdData *pri_xml_rd    = NULL;
    struct repomdData *fil_xml_rd    = NULL;
    struct repomdData *oth_xml_rd    = NULL;
    struct repomdData *pri_sqlite_rd = NULL;
    struct repomdData *fil_sqlite_rd = NULL;
    struct repomdData *oth_sqlite_rd = NULL;
    struct repomdData *groupfile_rd  = NULL;
    struct repomdData *cgroupfile_rd = NULL;

    if (pri_xml) {
        pri_xml_rd = new_repomddata();
        pri_xml_rd->location_href = pri_xml;
    }
    if (fil_xml) {
        fil_xml_rd = new_repomddata();
        fil_xml_rd->location_href = fil_xml;
    }
    if (oth_xml) {
        oth_xml_rd = new_repomddata();
        oth_xml_rd->location_href = oth_xml;
    }
    if (pri_sqlite) {
        pri_sqlite_rd = new_repomddata();
        pri_sqlite_rd->location_href = pri_sqlite;
    }
    if (fil_sqlite) {
        fil_sqlite_rd = new_repomddata();
        fil_sqlite_rd->location_href = fil_sqlite;
    }
    if (oth_sqlite) {
        oth_sqlite_rd = new_repomddata();
        oth_sqlite_rd->location_href = oth_sqlite;
    }
    if (groupfile) {
        groupfile_rd = new_repomddata();
        groupfile_rd->location_href = groupfile;
        cgroupfile_rd = new_repomddata();
        cgroupfile_rd->location_href = groupfile;
    }

    // Dump xml

    struct repomdResult *res = xml_repomd_2(path, rename_to_unique,
                                            pri_xml_rd, fil_xml_rd, oth_xml_rd,
                                            pri_sqlite_rd, fil_sqlite_rd, oth_sqlite_rd,
                                            groupfile_rd, cgroupfile_rd, checksum_type);

    free_repomddata(pri_xml_rd);
    free_repomddata(fil_xml_rd);
    free_repomddata(oth_xml_rd);
    free_repomddata(pri_sqlite_rd);
    free_repomddata(fil_sqlite_rd);
    free_repomddata(oth_sqlite_rd);
    free_repomddata(groupfile_rd);
    free_repomddata(cgroupfile_rd);

    return res;
}
