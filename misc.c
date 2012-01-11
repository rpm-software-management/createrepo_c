#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include "misc.h"


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

