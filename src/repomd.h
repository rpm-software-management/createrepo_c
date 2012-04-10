#ifndef __C_CREATEREPOLIB_REPOMD_H__
#define __C_CREATEREPOLIB_REPOMD_H__

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "package.h"


struct repomdResult {
    char *pri_xml_location;
    char *fil_xml_location;
    char *oth_xml_location;
    char *pri_sqlite_location;
    char *fil_sqlite_location;
    char *oth_sqlite_location;
    char *groupfile_location;
    char *cgroupfile_location;
    char *update_info_location;
    char *repomd_xml;
};


void free_repomdresult(struct repomdResult *);

// all files except groupfile must be compressed (only group file should by plain noncompressed xml)
struct repomdResult *xml_repomd(const char *path, int rename_to_unique, const char *pri_xml,
                                const char *fil_xml, const char *oth_xml,
                                const char *pri_sqlite, const char *fil_sqlite,
                                const char *oth_sqlite, const char *groupfile,
                                const char *update_info, ChecksumType *checksum_type);

#endif /* __C_CREATEREPOLIB_REPOMD_H__ */
