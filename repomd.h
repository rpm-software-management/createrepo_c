#ifndef __C_CREATEREPOLIB_REPOMD_H__
#define __C_CREATEREPOLIB_REPOMD_H__

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "package.h"

typedef struct _repomdData {
    char *location_href;
    char *checksum;
    char *checksum_type;
    char *checksum_open;
    char *checksum_open_type;
    long timestamp;
    long size;
    long size_open;
    int db_ver;

    GStringChunk *chunk;
} repomdData;

repomdData *new_repomddata();
void free_repomddata(repomdData *md);

// Function modifies params structures!
char *xml_repomd_2(const char *path, repomdData *pri_xml, repomdData *fil_xml, repomdData *oth_xml,
                 repomdData *pri_sqlite, repomdData *fil_sqlite, repomdData *oth_sqlite, ChecksumType *checksum_type);

char *xml_repomd(const char *path, const char *pri_xml, const char *fil_xml, const char *oth_xml,
                 const char *pri_sqlite, const char *fil_sqlite, const char *oth_sqlite, ChecksumType *checksum_type);

#endif /* __C_CREATEREPOLIB_REPOMD_H__ */
