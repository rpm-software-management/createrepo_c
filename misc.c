#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "constants.h"
#include "misc.h"

#define BUFFER_SIZE     4096


const char *flag_to_string(gint64 flags)
{
    flags &= 0xf;
    switch(flags) {
        case 0:
            return "";
        case 2:
            return "LT";
        case 4:
            return "GT";
        case 8:
            return "EQ";
        case 10:
            return "LE";
        case 12:
            return "GE";
        default:
            return "";
    }
}


/*
 * BE CAREFULL!
 *
 * In case chunk param is NULL:
 * Returned structure had all strings malloced!!!
 * Be so kind and don't forget use free() for all its element, before end of structure lifecycle.
 *
 * In case chunk is pointer to a GStringChunk:
 * Returned structure had all string inserted in the passed chunk.
 *
 */
struct VersionStruct string_to_version(const char *string, GStringChunk *chunk)
{
    struct VersionStruct ver;
    ver.epoch = NULL;
    ver.version = NULL;
    ver.release = NULL;

    if (!string || !(strlen(string))) {
        return ver;
    }

    char *ptr;  // These names are totally self explaining
    char *ptr2; //

    // Epoch
    ptr = strstr(string, ":");
    if (ptr) {
        // Check if epoch str is a number
        char *p = NULL;
        strtol(string, &p, 10);
        if (p == ptr) { // epoch str seems to be a number
            size_t len = ptr - string;
            if (chunk) {
                ver.epoch = g_string_chunk_insert_len(chunk, string, len);
            } else {
                ver.epoch = malloc(sizeof(char) * (len + 1));
                strncpy(ver.epoch, string, len);
                ver.epoch[len] = '\0';
            }
        }
    } else { // There is no epoch
        ptr = (char*) string-1;
    }

    if (!ver.epoch) {
        if (chunk) {
            ver.epoch = g_string_chunk_insert_const(chunk, "0");
        } else {
            ver.epoch = malloc(sizeof(char) * 2);
            ver.epoch[0] = '0';
            ver.epoch[1] = '\0';
        }
    }

    // Version + release
    ptr2 = strstr(ptr+1, "-");
    if (ptr2) {
        // Version
        size_t version_len = ptr2 - (ptr+1);
        if (chunk) {
            ver.version = g_string_chunk_insert_len(chunk, ptr+1, version_len);
        } else {
            ver.version = malloc(sizeof(char) * (version_len + 1));
            strncpy(ver.version, ptr+1, version_len);
            ver.version[version_len] = '\0';
        }

        // Release
        size_t release_len = strlen(ptr2+1);
        if (release_len) {
            if (chunk) {
                ver.release = g_string_chunk_insert_len(chunk, ptr2+1, release_len);
            } else {
                ver.release = malloc(sizeof(char) * (release_len+1));
                strcpy(ver.release, ptr2+1);
            }
        }
    } else { // Release is not here, just version
        if (chunk) {
            ver.version = g_string_chunk_insert_const(chunk, ptr+1);
        } else {
            ver.version = malloc(sizeof(char) * (strlen(ptr+1) + 1));
            strcpy(ver.version, ptr+1);
        }
    }

    return ver;
}


GRegex *pri_re_1 = NULL;
GRegex *pri_re_2 = NULL;
GRegex *pri_re_3 = NULL;

int is_primary(const char *filename)
{
    // Regex compilation
    if (!pri_re_1) {
        GRegexMatchFlags compile_flags = G_REGEX_OPTIMIZE|G_REGEX_MATCH_ANCHORED;
        pri_re_1 = g_regex_new(".*bin/.*", compile_flags, 0, NULL);
        pri_re_2 = g_regex_new("/etc/.*", compile_flags, 0, NULL);
        pri_re_3 = g_regex_new("/usr/lib/sendmail$", compile_flags, 0, NULL);
    }

    if (g_regex_match(pri_re_1, filename, 0, NULL)
        || g_regex_match(pri_re_2, filename, 0, NULL)
        || g_regex_match(pri_re_3, filename, 0, NULL))
    {
        return 1;
    }

    return 0;
}



char *compute_file_checksum(const char *filename, ChecksumType type)
{
    GChecksumType gchecksumtype;

    // Check if file exists and if it is a regular file (not a directory)

    if (!g_file_test(filename, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))) {
        return NULL;
    }

    // Convert our checksum type into glib type

    switch (type) {
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

    // Open file and initialize checksum structure

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return NULL;
    }

    // Calculate checksum

    GChecksum *checksum = g_checksum_new(gchecksumtype);
    unsigned char buffer[BUFFER_SIZE];

    while (1) {
        size_t input_len;
        input_len = fread((void *) buffer, sizeof(unsigned char), BUFFER_SIZE, fp);
        g_checksum_update(checksum, (const guchar *) buffer, input_len);
        if (input_len < BUFFER_SIZE) {
            break;
        }
    }

    fclose(fp);

    // Malloc space and get checksum

    const char *checksum_tmp_str = g_checksum_get_string(checksum);
    size_t checksum_len = strlen(checksum_tmp_str);
    char *checksum_str = (char *) malloc(sizeof(char) * (checksum_len + 1));
    if (!checksum_str) {
        g_checksum_free(checksum);
        return NULL;
    }
    strcpy(checksum_str, checksum_tmp_str);

    // Clean up

    g_checksum_free(checksum);

    return checksum_str;
}


#define VAL_LEN         4       // Len of numeric values in rpm

struct HeaderRangeStruct get_header_byte_range(const char *filename)
{
    /* Values readed by fread are 4 bytes long and stored as big-endian.
     * So there is htonl function to convert this big-endian number into host byte order.
     */
    FILE *fp = fopen(filename, "rb");
    fseek(fp, 104, SEEK_SET);
    unsigned int sigindex = 0;
    unsigned int sigdata  = 0;
    fread(&sigindex, VAL_LEN, 1, fp);
    sigindex = htonl(sigindex);
    fread(&sigdata, VAL_LEN, 1, fp);
    sigdata = htonl(sigdata);
    unsigned int sigindexsize = sigindex * 16;
    unsigned int sigsize = sigdata + sigindexsize;
    unsigned int disttoboundary = sigsize % 8;
    if (disttoboundary) {
        disttoboundary = 8 - disttoboundary;
    }
    unsigned int hdrstart = 112 + sigsize + disttoboundary;

    fseek(fp, hdrstart, SEEK_SET);
    fseek(fp, 8, SEEK_CUR);

    unsigned int hdrindex = 0;
    unsigned int hdrdata  = 0;
    fread(&hdrindex, VAL_LEN, 1, fp);
    hdrindex = htonl(hdrindex);
    fread(&hdrdata, VAL_LEN, 1, fp);
    hdrdata = htonl(hdrdata);
    unsigned int hdrindexsize = hdrindex * 16;
    unsigned int hdrsize = hdrdata + hdrindexsize + 16;
    unsigned int hdrend = hdrstart + hdrsize;

    fclose(fp);

    return (struct HeaderRangeStruct) {hdrstart, hdrend};
}
