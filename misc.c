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
 * BE CAREFULL! Returned structure had all strings malloced!!!
 * Be so kind and don't forget use free() for all its element, before end of structure lifecycle.
 */
struct VersionStruct string_to_version(const char *string)
{
    struct VersionStruct ver;
    ver.epoch = NULL;
    ver.version = NULL;
    ver.release = NULL;

    if (!string || !(strlen(string))) {
        ver.epoch = malloc(sizeof(char));
        ver.version = malloc(sizeof(char));
        ver.release = malloc(sizeof(char));
        ver.epoch[0] = '\0';
        ver.version[0] = '\0';
        ver.release[0] = '\0';
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
            ver.epoch = malloc(sizeof(char) * (len + 1));
            strncpy(ver.epoch, string, len);
            ver.epoch[len] = '\0';
        } else { // epoch is not a number
            ver.epoch = NULL;
        }
    } else {
        ver.epoch = NULL;
        ptr = string-1;
    }

    // Version + release
    ptr2 = strstr(ptr+1, "-");
    if (ptr2) {
        size_t len = ptr2 - (ptr+1);
        ver.version = malloc(sizeof(char) * (len + 1));
        strncpy(ver.version, ptr+1, len);
        ver.version[len] = '\0';
        ver.release = malloc(sizeof(char) * (strlen(ptr2+1) + 1));
        strcpy(ver.release, ptr2+1);
    } else {
        ver.version = malloc(sizeof(char) * (strlen(ptr+1) + 1));
        strcpy(ver.version, ptr+1);
    }

    // Malloc empty strings instead of NULL
    if (!ver.epoch) {
        ver.epoch = malloc(sizeof(char) * 2);
        ver.epoch[0] = '0';
        ver.epoch[1] = '\0';
    } else if (!ver.version) {
        ver.version = malloc(sizeof(char));
        ver.version[0] = '\0';
    } else if (!ver.release) {
        ver.release = malloc(sizeof(char));
        ver.release[0] = '\0';
    }

    return ver;
}
