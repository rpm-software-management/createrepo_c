#ifndef __C_CREATEREPOLIB_REPOMD__
#define __C_CREATEREPOLIB_REPOMD__

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
    // TODO XXX: string chunk
} repomdData;

repomdData *new_repomddata();
void free_repomddata(repomdData *md);

// Function modifies params structures!
char *xml_repomd(const char *path, repomdData *pri_xml, repomdData *fil_xml, repomdData *oth_xml,
                 repomdData *pri_sqlite, repomdData *fil_sqlite, repomdData *oth_sqlite);

#endif /* __C_CREATEREPOLIB_REPOMD__ */
