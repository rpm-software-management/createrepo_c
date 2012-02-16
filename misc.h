#ifndef __C_CREATEREPOLIB_MISC_H__
#define __C_CREATEREPOLIB_MISC_H__

#include <glib.h>
#include "constants.h"

const char *flag_to_string(gint64 flags);

struct VersionStruct {
    char *epoch;
    char *version;
    char *release;
};

/*
 * BE CAREFULL! Returned structure had all strings malloced!!!
 * Be so kind and don't forget use free() for all its element, before end of structure lifecycle.
 */
struct VersionStruct string_to_version(const char *string, GStringChunk *chunk);

inline int is_primary(const char *filename);
char *compute_file_checksum(const char *filename, ChecksumType type);

struct HeaderRangeStruct {
    unsigned int start;
    unsigned int end;
};
struct HeaderRangeStruct get_header_byte_range(const char *filename);

const char *get_checksum_name_str(ChecksumType type);


#endif /* __C_CREATEREPOLIB_MISC_H__ */
