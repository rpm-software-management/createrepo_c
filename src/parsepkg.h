#ifndef __C_CREATEREPOLIB_PARSEPKG_H__
#define __C_CREATEREPOLIB_PARSEPKG_H__

#include <rpm/rpmlib.h>
#include <glib.h>
#include "constants.h"
#include "package.h"
#include "xml_dump.h"

/*
Package *parse_package(Header hdr, gint64 mtime, gint64 size, const char *checksum, const char *checksum_type,
                      const char *location_href, const char *location_base,
                      int changelog_limit, gint64 hdr_start, gint64 hdr_end);
*/

extern short initialized;
void init_package_parser();
void free_package_parser();


// TODO: swig map for struct stat stat_buf

// stat_buf can be NULL
struct XmlStruct xml_from_package_file(const char *filename, ChecksumType checksum_type,
                        const char *location_href, const char *location_base,
                        int changelog_limit, struct stat *stat_buf);

#endif /* __C_CREATEREPOLIB_PARSEPKG_H__ */
