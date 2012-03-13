#include <glib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "logging.h"
#include "constants.h"
#include "misc.h"

#undef MODULE
#define MODULE "misc: "


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
 * BE CAREFUL!
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

    const char *ptr;  // These names are totally self explaining
    const char *ptr2; //


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
                ver.epoch = g_strndup(string, len);
            }
        }
    } else { // There is no epoch
        ptr = (char*) string-1;
    }

    if (!ver.epoch) {
        if (chunk) {
            ver.epoch = g_string_chunk_insert_const(chunk, "0");
        } else {
            ver.epoch = g_strdup("0");
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
            ver.version = g_strndup(ptr+1, version_len);
        }

        // Release
        size_t release_len = strlen(ptr2+1);
        if (release_len) {
            if (chunk) {
                ver.release = g_string_chunk_insert_len(chunk, ptr2+1, release_len);
            } else {
                ver.release = g_strndup(ptr2+1, release_len);
            }
        }
    } else { // Release is not here, just version
        if (chunk) {
            ver.version = g_string_chunk_insert_const(chunk, ptr+1);
        } else {
            ver.version = g_strdup(ptr+1);
        }
    }

    return ver;
}



inline int is_primary(const char *filename)
{
/*
    This optimal piece of code cannot be used because of yum...
    We must match any string that contains "bin/" in dirname

    Response to my question from packaging team:
    ....
    It must still contain that. Atm. it's defined as taking anything
    with 'bin/' in the path. The idea was that it'd match /usr/kerberos/bin/
    and /opt/blah/sbin. So that is what all versions of createrepo generate,
    and what yum all versions of yum expect to be generated.
    We can't change one side, without breaking the expectation of the
    other.
    There have been plans to change the repodata, and one of the changes
    would almost certainly be in how files are represented ... likely via.
    lists of "known" paths, that can be computed at createrepo time.

    if (!strncmp(filename, "/bin/", 5)) {
        return 1;
    }

    if (!strncmp(filename, "/sbin/", 6)) {
        return 1;
    }

    if (!strncmp(filename, "/etc/", 5)) {
        return 1;
    }

    if (!strncmp(filename, "/usr/", 5)) {
        if (!strncmp(filename+5, "bin/", 4)) {
            return 1;
        }

        if (!strncmp(filename+5, "sbin/", 5)) {
            return 1;
        }

        if (!strcmp(filename+5, "lib/sendmail")) {
            return 1;
        }
    }
*/

    if (!strncmp(filename, "/etc/", 5)) {
        return 1;
    }

    if (!strcmp(filename, "/usr/lib/sendmail")) {
        return 1;
    }

    if (strstr(filename, "bin/")) {
        return 1;
    }

    return 0;
}



char *compute_file_checksum(const char *filename, ChecksumType type)
{
    GChecksumType gchecksumtype;


    // Check if file exists and if it is a regular file (not a directory)

    if (!g_file_test(filename, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))) {
        g_critical(MODULE"compute_file_checksum: File %s doesn't exists", filename);
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
        default:
            g_critical(MODULE"compute_file_checksum: Unknown checksum type");
            return NULL;
    };


    // Open file and initialize checksum structure

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        g_critical(MODULE"compute_file_checksum: Cannot open %s (%s)", filename, strerror(errno));
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


    // Get checksum

    char *checksum_str = g_strdup(g_checksum_get_string(checksum));
    g_checksum_free(checksum);

    if (!checksum_str) {
        g_critical(MODULE"compute_file_checksum: Cannot get checksum %s (low memory?)", filename);
    }

    return checksum_str;
}



#define VAL_LEN         4       // Len of numeric values in rpm

struct HeaderRangeStruct get_header_byte_range(const char *filename)
{
    /* Values readed by fread are 4 bytes long and stored as big-endian.
     * So there is htonl function to convert this big-endian number into host byte order.
     */

    struct HeaderRangeStruct results;

    results.start = 0;
    results.end   = 0;


    // Open file

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        g_critical(MODULE"get_header_byte_range: Cannot open file %s (%s)", filename, strerror(errno));
        return results;
    }


    // Get header range

    if (fseek(fp, 104, SEEK_SET) != 0) {
        g_critical(MODULE"get_header_byte_range: fseek fail on %s (%s)", filename, strerror(errno));
        fclose(fp);
        return results;
    }

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


    // Check sanity

    if (hdrend < hdrstart) {
        g_critical(MODULE"get_header_byte_range: sanity check fail on %s (%d > %d))", filename, hdrstart, hdrend);
        return results;
    }

    results.start = hdrstart;
    results.end   = hdrend;

    return results;
}


const char *get_checksum_name_str(ChecksumType type)
{
    char *name = NULL;

    switch (type) {
        case PKG_CHECKSUM_MD5:
            name = "md5";
            break;
        case PKG_CHECKSUM_SHA1:
            name = "sha1";
            break;
        case PKG_CHECKSUM_SHA256:
            name = "sha256";
            break;
        default:
            g_debug(MODULE"get_checksum_name_str: Unknown checksum");
            break;
    }

    return name;
}


char *get_filename(const char *filepath)
{
    char *filename = NULL;

    if (!filepath) {
        return filename;
    }

    filename = (char *) filepath;
    size_t x = 0;

    while (filepath[x] != '\0') {
        if (filepath[x] == '/') {
            filename = (char *) filepath+(x+1);
        }
        x++;
    }

    return filename;
}


int copy_file(const char *src, const char *dst)
{
    size_t readed;
    char buf[BUFFER_SIZE];

    FILE *orig;
    FILE *new;

    if ((orig = fopen(src, "r")) == NULL) {
        g_debug(MODULE"copy_file: Cannot open source file %s (%s)", src, strerror(errno));
        return CR_COPY_ERR;
    }

    if ((new = fopen(dst, "w")) == NULL) {
        g_debug(MODULE"copy_file: Cannot open destination file %s (%s)", dst, strerror(errno));
        fclose(orig);
        return CR_COPY_ERR;
    }

    while ((readed = fread(buf, 1, BUFFER_SIZE, orig)) > 0) {
        if (fwrite(buf, 1, readed, new) != readed) {
            g_debug(MODULE"copy_file: Error while copy %s -> %s (%s)", src, dst, strerror(errno));
            fclose(new);
            fclose(orig);
            return CR_COPY_ERR;
        }

        if (readed != BUFFER_SIZE && ferror(orig)) {
            g_debug(MODULE"copy_file: Error while copy %s -> %s (%s)", src, dst, strerror(errno));
            fclose(new);
            fclose(orig);
            return CR_COPY_ERR;
        }
    }

    fclose(new);
    fclose(orig);

    return CR_COPY_OK;
}

/*
long int get_file_size(const char *path)
{
    if (!path) {
        return -1;
    }

    FILE *f;
    if ((f = fopen(path, "r")) == NULL) {
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        return -1;
    }

    long int size = ftell(f);

    fclose(f);

    return size;
}
*/
