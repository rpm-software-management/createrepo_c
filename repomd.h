#ifndef __C_CREATEREPOLIB_REPOMD_H__
#define __C_CREATEREPOLIB_REPOMD_H__

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "package.h"

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


struct repomdResult {
    char *pri_xml_location;
    char *fil_xml_location;
    char *oth_xml_location;
    char *pri_sqlite_location;
    char *fil_sqlite_location;
    char *oth_sqlite_location;
    char *repomd_xml;
};


struct repomdData *new_repomddata();
void free_repomddata(struct repomdData *);
void free_repomdresult(struct repomdResult *);

struct repomdResult *xml_repomd(const char *path, int rename_to_unique, const char *pri_xml, const char *fil_xml, const char *oth_xml,
                 const char *pri_sqlite, const char *fil_sqlite, const char *oth_sqlite, ChecksumType *checksum_type);

#endif /* __C_CREATEREPOLIB_REPOMD_H__ */
