#ifndef __C_CREATEREPOLIB_LOCATE_METADATA_H__
#define __C_CREATEREPOLIB_LOCATE_METADATA_H__


struct MetadataLocation {
    char *pri_xml_href;
    char *fil_xml_href;
    char *oth_xml_href;
    char *pri_sqlite_href;
    char *fil_sqlite_href;
    char *oth_sqlite_href;
    char *groupfile_href;
    char *cgroupfile_href; // compressed groupfile location
    char *repomd;
    char *tmp_dir; // Path to dir where temporary metadata are stored (if metadata were downloaded)
};

struct MetadataLocation *get_metadata_location(const char *);
void free_metadata_location(struct MetadataLocation *);

int remove_metadata(const char *repopath);

#endif /* __C_CREATEREPOLIB_LOCATE_METADATA_H__ */
