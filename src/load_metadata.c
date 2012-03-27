#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <expat.h>
#include <libxml/xmlreader.h>
#include "package.h"
#include "logging.h"
#include "misc.h"
#include "load_metadata.h"
#include "compression_wrapper.h"

#undef MODULE
#define MODULE "load_metadata: "

#define FORMAT_XML      1
#define FORMAT_LEVEL    0

#define CHUNK_SIZE              8192
#define PKGS_REALLOC_STEP       2000


typedef enum {
    NONE_ELEM,
    NAME_ELEM,
    ARCH_ELEM,
    CHECKSUM_ELEM,
    SUMMARY_ELEM,
    DESCRIPTION_ELEM,
    PACKAGER_ELEM,
    URL_ELEM,
    RPM_LICENSE_ELEM,
    RPM_VENDOR_ELEM,
    RPM_GROUP_ELEM,
    RPM_BUILDHOST_ELEM,
    RPM_SOURCERPM_ELEM,
    FILE_ELEM,
    FILE_DIR_ELEM,
    FILE_GHOST_ELEM,
    CHANGELOG_ELEM
} PrimaryTextElement;


typedef enum {
    ROOT,
    METADATA,
    PACKAGE,
    FORMAT,
    PROVIDES,
    CONFLICTS,
    OBSOLETES,
    REQUIRES,
    // filelists
    FILELISTS,
    // other
    OTHERDATA
} PrimaryParserContext;



struct PrimaryParserData {
    int total_pkgs;
    int actual_pkg;
    int pkgs_size;
    Package **pkgs;

    GString *current_string;
    GHashTable *hashtable;
    Package *pkg;
    PrimaryParserContext context;
    PrimaryTextElement last_elem;
};



void free_values(gpointer data)
{
    package_free((Package *) data);
}



GHashTable *new_metadata_hashtable()
{
    GHashTable *hashtable = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, free_values);
    return hashtable;
}



void destroy_metadata_hashtable(GHashTable *hashtable)
{
    if (hashtable) {
        g_hash_table_destroy (hashtable);
    }
}



void free_metadata_location(struct MetadataLocation *ml)
{
    if (!ml) {
        return;
    }

    g_free(ml->pri_xml_href);
    g_free(ml->fil_xml_href);
    g_free(ml->oth_xml_href);
    g_free(ml->pri_sqlite_href);
    g_free(ml->fil_sqlite_href);
    g_free(ml->oth_sqlite_href);
    g_free(ml->groupfile_href);
    g_free(ml->cgroupfile_href);
    g_free(ml->repomd);
    g_free(ml);
}



// primary.xml parser handlers

void pri_start_handler(void *data, const char *el, const char **attr) {
    struct PrimaryParserData *ppd = (struct PrimaryParserData *) data;
    Package *pkg = ppd->pkg;
    int i;

    // <file> and <rpm:entry> are most frequently used tags in primary.xml

    // <file>
    if (!strcmp(el, "file")) {
        ppd->last_elem = FILE_ELEM;

    // <rpm:entry>
    } else if (!strcmp(el, "rpm:entry")) {
        Dependency *dependency;
        dependency = dependency_new();

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "name")) {
                dependency->name = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else if (!strcmp(attr[i], "flags")) {
                dependency->flags = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else if (!strcmp(attr[i], "epoch")) {
                dependency->epoch = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else if (!strcmp(attr[i], "ver")) {
                dependency->version = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else if (!strcmp(attr[i], "rel")) {
                dependency->release = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else if (!strcmp(attr[i], "pre")) {
                if (!strcmp(attr[i+1], "0") || !strcmp(attr[i+1], "FALSE")) {
                    dependency->pre = FALSE;
                } else {
                    dependency->pre = TRUE;
                }
            } else {
                g_warning(MODULE"%s: Unknown attribute \"%s\"", __func__, attr[i]);
            }
        }

        switch (ppd->context) {
            case PROVIDES:
                pkg->provides = g_slist_prepend(pkg->provides, dependency);
                break;
            case CONFLICTS:
                pkg->conflicts = g_slist_prepend(pkg->conflicts, dependency);
                break;
            case OBSOLETES:
                pkg->obsoletes = g_slist_prepend(pkg->obsoletes, dependency);
                break;
            case REQUIRES:
                pkg->requires = g_slist_prepend(pkg->requires, dependency);
                break;
            default:
                g_free(dependency);
                g_warning(MODULE"%s: Bad context (%d) for rpm:entry", __func__, ppd->context);
                break;
        }

    // <package>
    } else if (!strcmp(el, "package")) {
        // Check sanity
        if (ppd->context != METADATA) {
            g_critical(MODULE"%s: Package element: Bad XML context!", __func__);
            return;
        }
        if (ppd->pkg) {
            g_critical(MODULE"%s: Package element: Pkg pointer is not NULL", __func__);
            return;
        }

        ppd->context = PACKAGE;
        ppd->pkg = package_new();

    // <name>
    } else if (!strcmp(el, "name")) {
        ppd->last_elem = NAME_ELEM;

    // <arch>
    } else if (!strcmp(el, "arch")) {
        ppd->last_elem = ARCH_ELEM;

    // <version>
    } else if (!strcmp(el, "version")) {
        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "epoch")) {
                pkg->epoch = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else if (!strcmp(attr[i], "ver")) {
                pkg->version = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else if (!strcmp(attr[i], "rel")) {
                pkg->release = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else {
                g_warning(MODULE"%s: Unknown attribute \"%s\"", __func__, attr[i]);
            }
        }

    // <checksum>
    } else if (!strcmp(el, "checksum")) {
        ppd->last_elem = CHECKSUM_ELEM;

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "type")) {
                pkg->checksum_type = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            }
        }

    // <summary>
    } else if (!strcmp(el, "summary")) {
        ppd->last_elem = SUMMARY_ELEM;

    // <description>
    } else if (!strcmp(el, "description")) {
        ppd->last_elem = DESCRIPTION_ELEM;

    // <packager>
    } else if (!strcmp(el, "packager")) {
        ppd->last_elem = PACKAGER_ELEM;

    // <url>
    } else if (!strcmp(el, "url")) {
        ppd->last_elem = URL_ELEM;

    // <time>
    } else if (!strcmp(el, "time")) {
        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "file")) {
                pkg->time_file = g_ascii_strtoll(attr[i+1], NULL, 10);
            } else if (!strcmp(attr[i], "build")) {
                pkg->time_build = g_ascii_strtoll(attr[i+1], NULL, 10);
            } else {
                g_warning(MODULE"%s: Unknown attribute \"%s\"", __func__, attr[i]);
            }
        }

    // <size>
    } else if (!strcmp(el, "size")) {
        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "package")) {
                pkg->size_package = g_ascii_strtoll(attr[i+1], NULL, 10);
            } else if (!strcmp(attr[i], "installed")) {
                pkg->size_installed = g_ascii_strtoll(attr[i+1], NULL, 10);
            } else if (!strcmp(attr[i], "archive")) {
                pkg->size_archive = g_ascii_strtoll(attr[i+1], NULL, 10);
            } else {
                g_warning(MODULE"%s: Unknown attribute \"%s\"", __func__, attr[i]);
            }
        }

    // <location>
    } else if (!strcmp(el, "location")) {
        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "href")) {
                pkg->location_href = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else if (!strcmp(attr[i], "xml:base")) {
                pkg->location_base = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else {
                g_warning(MODULE"%s: Unknown attribute \"%s\"", __func__, attr[i]);
            }
        }

    // <format>
    } else if (!strcmp(el, "format")) {
        ppd->context = FORMAT;

    // <rpm:license>
    } else if (!strcmp(el, "rpm:license")) {
        ppd->last_elem = RPM_LICENSE_ELEM;

    // <rpm:vendor>
    } else if (!strcmp(el, "rpm:vendor")) {
        ppd->last_elem = RPM_VENDOR_ELEM;

    // <rpm:group>
    } else if (!strcmp(el, "rpm:group")) {
        ppd->last_elem = RPM_GROUP_ELEM;

    // <rpm:buildhost>
    } else if (!strcmp(el, "rpm:buildhost")) {
        ppd->last_elem = RPM_BUILDHOST_ELEM;

    // <rpm:sourcerpm>
    } else if (!strcmp(el, "rpm:sourcerpm")) {
        ppd->last_elem = RPM_SOURCERPM_ELEM;

    // <rpm:header-range>
    } else if (!strcmp(el, "rpm:header-range")) {
        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "start")) {
                pkg->rpm_header_start = g_ascii_strtoll(attr[i+1], NULL, 10);
            } else if (!strcmp(attr[i], "end")) {
                pkg->rpm_header_end = g_ascii_strtoll(attr[i+1], NULL, 10);
            } else {
                g_warning(MODULE"%s: Unknown attribute \"%s\"", __func__, attr[i]);
            }
        }

    // <rpm:provides>
    } else if (!strcmp(el, "rpm:provides")) {
        ppd->context = PROVIDES;

    // <rpm:conflicts>
    } else if (!strcmp(el, "rpm:conflicts")) {
        ppd->context = CONFLICTS;

    // <rpm:obsoletes>
    } else if (!strcmp(el, "rpm:obsoletes")) {
        ppd->context = OBSOLETES;

    // <rpm:requires>
    } else if (!strcmp(el, "rpm:requires")) {
        ppd->context = REQUIRES;

    // <metadata>
    } else if (!strcmp(el, "metadata")) {
        if (ppd->context != ROOT) {
            g_critical(MODULE"%s: Bad context (%d) for %s element", __func__, ppd->context, el);
            return;
        }
        ppd->context = METADATA;

    // unknown element
    } else {
        g_warning(MODULE"%s: Unknown element: %s", __func__, el);
    }
}



void pri_char_handler(void *data, const char *txt, int txtlen) {
    struct PrimaryParserData *ppd = (struct PrimaryParserData *) data;
    Package *pkg = ppd->pkg;

    if (!pkg || ppd->last_elem == NONE_ELEM) {
        return;
    }

    switch (ppd->last_elem) {
        case FILE_ELEM:
        case FILE_DIR_ELEM:
        case FILE_GHOST_ELEM:
            // Files are readed from filelists.xml not from primary.xml -> skip
            break;
        case NAME_ELEM:
        case ARCH_ELEM:
        case CHECKSUM_ELEM:
        case SUMMARY_ELEM:
        case DESCRIPTION_ELEM:
        case PACKAGER_ELEM:
        case URL_ELEM:
        case RPM_LICENSE_ELEM:
        case RPM_VENDOR_ELEM:
        case RPM_GROUP_ELEM:
        case RPM_BUILDHOST_ELEM:
        case RPM_SOURCERPM_ELEM:
            g_string_append_len(ppd->current_string, txt, (gsize) txtlen);
            break;
        default:
            g_warning(MODULE"%s: Unknown last xml element", __func__);
            break;
    }
}



void pri_end_handler(void *data, const char *el) {
    struct PrimaryParserData *ppd = (struct PrimaryParserData *) data;
    Package *pkg = ppd->pkg;


    // Store strings

    gchar *txt = ppd->current_string->str;
    gsize txtlen = ppd->current_string->len;

    if (ppd->last_elem != NONE_ELEM) {
        switch (ppd->last_elem) {
            case NAME_ELEM:
                pkg->name = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;
            case ARCH_ELEM:
                pkg->arch = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;
            case CHECKSUM_ELEM:
                pkg->pkgId = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;
            case SUMMARY_ELEM:
                pkg->summary = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;
            case DESCRIPTION_ELEM:
                pkg->description = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;
            case PACKAGER_ELEM:
                pkg->rpm_packager = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;
            case URL_ELEM:
                pkg->url = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;
            case RPM_LICENSE_ELEM:
                pkg->rpm_license = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;
            case RPM_VENDOR_ELEM:
                pkg->rpm_vendor = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;
            case RPM_GROUP_ELEM:
                pkg->rpm_group = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;
            case RPM_BUILDHOST_ELEM:
                pkg->rpm_buildhost = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;
            case RPM_SOURCERPM_ELEM:
                pkg->rpm_sourcerpm = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;
            case FILE_ELEM:
            case FILE_DIR_ELEM:
            case FILE_GHOST_ELEM:
            default:
                break;
        }

        ppd->last_elem = NONE_ELEM;
    }

    g_string_erase(ppd->current_string, 0, -1);


    // Set proper context

    if (!strcmp(el, "package")) {
        ppd->context = METADATA;

        if (ppd->pkg){
            // Store package into the hashtable
            char *key = pkg->pkgId;
            if (key && key[0] != '\0') {
                g_hash_table_insert(ppd->hashtable, key, ppd->pkg);
            } else {
                g_warning(MODULE"%s: Empty hashtable key!", __func__);
            }

            // Update PrimaryParserData
            ppd->pkg = NULL;
/*            ppd->total_pkgs++;
            if (ppd->total_pkgs > ppd->pkgs_size) {
                ppd->pkgs_size += PKGS_REALLOC_STEP;
                ppd->pkgs = realloc(ppd->pkgs, (sizeof(Package*) * PKGS_REALLOC_STEP));
                if (!ppd->pkgs) {
                    g_critical(MODULE"%s: Realloc fail! Ran out of memory.", __func__);
                    return;
                }
            }
            ppd->pkgs[ppd->actual_pkg] = ppd->pkg;
            ppd->actual_pkg++;
            */

            // Reverse lists
            pkg->requires  = g_slist_reverse(pkg->requires);
            pkg->provides  = g_slist_reverse(pkg->provides);
            pkg->conflicts = g_slist_reverse(pkg->conflicts);
            pkg->obsoletes  = g_slist_reverse(pkg->obsoletes);
        }
    } else if (!strcmp(el, "rpm:provides")) {
        ppd->context = FORMAT;
    } else if (!strcmp(el, "rpm:conflicts")) {
        ppd->context = FORMAT;
    } else if (!strcmp(el, "rpm:obsoletes")) {
        ppd->context = FORMAT;
    } else if (!strcmp(el, "rpm:requires")) {
        ppd->context = FORMAT;
    } else if (!strcmp(el, "format")) {
        ppd->context = PACKAGE;
    } else if (!strcmp(el, "metadata")) {
        ppd->context = ROOT;
    }
}


// filelists.xml parser handlers

void fil_start_handler(void *data, const char *el, const char **attr) {
    int i;
    struct PrimaryParserData *ppd = (struct PrimaryParserData *) data;
    Package *pkg = ppd->pkg;


    // <file>
    if (!strcmp(el, "file")) {
        assert(pkg);
        ppd->last_elem = FILE_ELEM;

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "type")) {
                if (!strcmp(attr[i+1], "dir")) {
                    ppd->last_elem = FILE_DIR_ELEM;
                } else if (!strcmp(attr[i+1], "ghost")) {
                    ppd->last_elem = FILE_GHOST_ELEM;
                }
            } else {
                g_warning(MODULE"%s: Unknown attribute \"%s\"", __func__, attr[i]);
            }
        }

    // <package>
    } else if (!strcmp(el, "package")) {
        // Check sanity
        if (ppd->context != FILELISTS) {
            g_critical(MODULE"%s: Package element: Bad XML context!", __func__);
            return;
        }
        if (ppd->pkg) {
            g_critical(MODULE"%s: Package element: Pkg pointer is not NULL", __func__);
            return;
        }

        ppd->context = PACKAGE;

        gchar *key;
        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "pkgid")) {
                key = (gchar *) attr[i+1];
            }
        }

        if (key) {
            ppd->pkg = (Package *) g_hash_table_lookup(ppd->hashtable, (gconstpointer) key);
            if (!ppd->pkg) {
                g_critical(MODULE"%s: Unknown package (package ID: %s)", __func__, key);
            }
        } else {
            g_critical(MODULE"%s: Package withou pkgid attribute found!", __func__);
        }

    // <version>
    } else if (!strcmp(el, "version")) {
        ;

    // <filelists>
    } else if (!strcmp(el, "filelists")) {
        if (ppd->context != ROOT) {
            g_critical(MODULE"%s: Bad context (%d) for %s element", __func__, ppd->context, el);
            return;
        }
        ppd->context = FILELISTS;

    // Unknown element
    } else {
        g_warning(MODULE"%s: Unknown element: %s", __func__, el);
    }
}



void fil_char_handler(void *data, const char *txt, int txtlen) {
    struct PrimaryParserData *ppd = (struct PrimaryParserData *) data;
    Package *pkg = ppd->pkg;

    if (!pkg || ppd->last_elem == NONE_ELEM) {
        return;
    }

    switch (ppd->last_elem) {
        case FILE_ELEM:
        case FILE_DIR_ELEM:
        case FILE_GHOST_ELEM:
            g_string_append_len(ppd->current_string, txt, (gsize) txtlen);
            break;
        case NAME_ELEM:
        case ARCH_ELEM:
        case CHECKSUM_ELEM:
        case SUMMARY_ELEM:
        case DESCRIPTION_ELEM:
        case PACKAGER_ELEM:
        case URL_ELEM:
        case RPM_LICENSE_ELEM:
        case RPM_VENDOR_ELEM:
        case RPM_GROUP_ELEM:
        case RPM_BUILDHOST_ELEM:
        case RPM_SOURCERPM_ELEM:
            g_warning(MODULE"%s: unsupported last xml element", __func__);
            break;
        default:
            g_warning(MODULE"%s: Unknown last xml element", __func__);
            break;
    }
}



void fil_end_handler(void *data, const char *el) {
    struct PrimaryParserData *ppd = (struct PrimaryParserData *) data;
    Package *pkg = ppd->pkg;


    // Store strings

    PackageFile *file;
    gchar *filename;
    gchar *txt = ppd->current_string->str;
    gsize txtlen = ppd->current_string->len;

    if (ppd->last_elem != NONE_ELEM) {
        switch (ppd->last_elem) {
            case NAME_ELEM:
            case ARCH_ELEM:
            case CHECKSUM_ELEM:
            case SUMMARY_ELEM:
            case DESCRIPTION_ELEM:
            case PACKAGER_ELEM:
            case URL_ELEM:
            case RPM_LICENSE_ELEM:
            case RPM_VENDOR_ELEM:
            case RPM_GROUP_ELEM:
            case RPM_BUILDHOST_ELEM:
            case RPM_SOURCERPM_ELEM:
                g_warning(MODULE"%s: Bad last xml element state", __func__);
                break;
            case FILE_ELEM:
            case FILE_DIR_ELEM:
            case FILE_GHOST_ELEM:
                if (!txt || txtlen == 0) {
                    g_warning(MODULE"%s: File with empty filename found!", __func__);
                    break;
                }

                file = package_file_new();
                filename = get_filename(txt);
                file->name = g_string_chunk_insert(pkg->chunk, filename);
                file->path = g_string_chunk_insert_len(pkg->chunk, txt, (txtlen - strlen(filename)));

                if (ppd->last_elem == FILE_ELEM) {
                    file->type = NULL;
                } else if (ppd->last_elem == FILE_DIR_ELEM) {
                    file->type = "dir";
                } else if (ppd->last_elem == FILE_GHOST_ELEM) {
                    file->type = "ghost";
                }

                pkg->files = g_slist_prepend(pkg->files, file);

            default:
                break;
        }

        ppd->last_elem = NONE_ELEM;
    }

    g_string_erase(ppd->current_string, 0, -1);


    // Set proper context

    if (!strcmp(el, "package")) {
        ppd->context = FILELISTS;
        ppd->pkg = NULL;

        // Reverse list of files
        pkg->files = g_slist_reverse(pkg->files);

    } else if (!strcmp(el, "filelists")) {
        ppd->context = ROOT;
    }
}


// other.xml parser handlers

void oth_start_handler(void *data, const char *el, const char **attr) {
    int i;
    struct PrimaryParserData *ppd = (struct PrimaryParserData *) data;
    Package *pkg = ppd->pkg;


    // <changelog>
    if (!strcmp(el, "changelog")) {
        assert(pkg);
        ppd->last_elem = CHANGELOG_ELEM;

        ChangelogEntry *changelog_entry;
        changelog_entry = changelog_entry_new();

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "author")) {
                changelog_entry->author = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else if (!strcmp(attr[i], "date")) {
                changelog_entry->date = g_ascii_strtoll(attr[i+1], NULL, 10);;
            } else {
                g_warning(MODULE"%s: Unknown attribute \"%s\"", __func__, attr[i]);
            }
        }

        pkg->changelogs = g_slist_prepend(pkg->changelogs, changelog_entry);

    // <package>
    } else if (!strcmp(el, "package")) {
        // Check sanity
        if (ppd->context != OTHERDATA) {
            g_critical(MODULE"%s: Package element: Bad XML context (%d)!", __func__, ppd->context);
            return;
        }
        if (ppd->pkg) {
            g_critical(MODULE"%s: Package element: Pkg pointer is not NULL", __func__);
            return;
        }

        ppd->context = PACKAGE;

        gchar *key;
        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "pkgid")) {
                key = (gchar *) attr[i+1];
            }
        }

        if (key) {
            ppd->pkg = (Package *) g_hash_table_lookup(ppd->hashtable, (gconstpointer) key);
            if (!ppd->pkg) {
                g_critical(MODULE"%s: Unknown package (package ID: %s)", __func__, key);
            }
        } else {
            g_critical(MODULE"%s: Package withou pkgid attribute found!", __func__);
        }

    // <version>
    } else if (!strcmp(el, "version")) {
        ;

    // <otherdata>
    } else if (!strcmp(el, "otherdata")) {
        if (ppd->context != ROOT) {
            g_critical(MODULE"%s: Bad context (%d) for %s element", __func__, ppd->context, el);
            return;
        }
        ppd->context = OTHERDATA;

    // Unknown element
    } else {
        g_warning(MODULE"%s: Unknown element: %s", __func__, el);
    }
}



void oth_char_handler(void *data, const char *txt, int txtlen) {
    struct PrimaryParserData *ppd = (struct PrimaryParserData *) data;
    Package *pkg = ppd->pkg;

    if (!pkg || ppd->last_elem == NONE_ELEM) {
        return;
    }

    switch (ppd->last_elem) {
        case CHANGELOG_ELEM:
            g_string_append_len(ppd->current_string, txt, (gsize) txtlen);
            break;
        case FILE_ELEM:
        case FILE_DIR_ELEM:
        case FILE_GHOST_ELEM:
        case NAME_ELEM:
        case ARCH_ELEM:
        case CHECKSUM_ELEM:
        case SUMMARY_ELEM:
        case DESCRIPTION_ELEM:
        case PACKAGER_ELEM:
        case URL_ELEM:
        case RPM_LICENSE_ELEM:
        case RPM_VENDOR_ELEM:
        case RPM_GROUP_ELEM:
        case RPM_BUILDHOST_ELEM:
        case RPM_SOURCERPM_ELEM:
            g_warning(MODULE"%s: unsupported last xml element", __func__);
            break;
        default:
            g_warning(MODULE"%s: Unknown last xml element", __func__);
            break;
    }
}



void oth_end_handler(void *data, const char *el) {
    struct PrimaryParserData *ppd = (struct PrimaryParserData *) data;
    Package *pkg = ppd->pkg;


    // Store strings

    gchar *txt = ppd->current_string->str;
    gsize txtlen = ppd->current_string->len;

    if (ppd->last_elem != NONE_ELEM) {
        switch (ppd->last_elem) {
            case CHANGELOG_ELEM:
                ((ChangelogEntry *) pkg->changelogs->data)->changelog = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;
            case NAME_ELEM:
            case ARCH_ELEM:
            case CHECKSUM_ELEM:
            case SUMMARY_ELEM:
            case DESCRIPTION_ELEM:
            case PACKAGER_ELEM:
            case URL_ELEM:
            case RPM_LICENSE_ELEM:
            case RPM_VENDOR_ELEM:
            case RPM_GROUP_ELEM:
            case RPM_BUILDHOST_ELEM:
            case RPM_SOURCERPM_ELEM:
            case FILE_ELEM:
            case FILE_DIR_ELEM:
            case FILE_GHOST_ELEM:
                g_warning(MODULE"%s: Bad last xml element state", __func__);
                break;
            default:
                break;
        }

        ppd->last_elem = NONE_ELEM;
    }

    g_string_erase(ppd->current_string, 0, -1);


    // Set proper context

    if (!strcmp(el, "package")) {
        ppd->context = OTHERDATA;
        ppd->pkg = NULL;

        // Reverse list of changelogs
        pkg->changelogs = g_slist_reverse(pkg->changelogs);

    } else if (!strcmp(el, "otherdata")) {
        ppd->context = ROOT;
    }
}



int load_xml_metadata(GHashTable *hashtable, const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path)
{
    CompressionType compression_type;
    CW_FILE *pri_xml_cwfile, *fil_xml_cwfile, *oth_xml_cwfile;
    XML_Parser pri_p, fil_p, oth_p;
    struct PrimaryParserData pri_pd;


    // Detect compression type

    compression_type = detect_compression(primary_xml_path);
    if (compression_type == UNKNOWN_COMPRESSION) {
        g_debug(MODULE"%s: Unknown compression", __func__);
        return 0;
    }


    // Open files

    if (!(pri_xml_cwfile = cw_open(primary_xml_path, CW_MODE_READ, compression_type))) {
        g_debug(MODULE"%s: Cannot open file: %s", __func__, primary_xml_path);
        return 0;
    }

    if (!(fil_xml_cwfile = cw_open(filelists_xml_path, CW_MODE_READ, compression_type))) {
        g_debug(MODULE"%s: Cannot open file: %s", __func__, filelists_xml_path);
        return 0;
    }

    if (!(oth_xml_cwfile = cw_open(other_xml_path, CW_MODE_READ, compression_type))) {
        g_debug(MODULE"%s: Cannot open file: %s", __func__, other_xml_path);
        return 0;
    }


    // Prepare parsers

    // XXX: Maybe try rely on package order and do not use hashtable
/*    pri_pd.total_pkgs = 0;
    pri_pd.actual_pkg = 0;
    pri_pd.pkgs_size = 0;
    pri_pd.pkgs = 0; */
    // XXX

    pri_pd.current_string = g_string_sized_new(1024);
    pri_pd.hashtable = hashtable;
    pri_pd.pkg = NULL;
    pri_pd.context = ROOT;
    pri_pd.last_elem = NONE_ELEM;

    pri_p = XML_ParserCreate(NULL);
    XML_SetUserData(pri_p, (void *) &pri_pd);
    XML_SetElementHandler(pri_p, pri_start_handler, pri_end_handler);
    XML_SetCharacterDataHandler(pri_p, pri_char_handler);

    fil_p = XML_ParserCreate(NULL);
    XML_SetUserData(fil_p, (void *) &pri_pd);
    XML_SetElementHandler(fil_p, fil_start_handler, fil_end_handler);
    XML_SetCharacterDataHandler(fil_p, fil_char_handler);

    oth_p = XML_ParserCreate(NULL);
    XML_SetUserData(oth_p, (void *) &pri_pd);
    XML_SetElementHandler(oth_p, oth_start_handler, oth_end_handler);
    XML_SetCharacterDataHandler(oth_p, oth_char_handler);


    // Parse

    // This loop should iterate over package chunks in primary.xml
    for (;;) {
        char *pri_buff;
        int pri_len;

        pri_buff = XML_GetBuffer(pri_p, CHUNK_SIZE);
        if (!pri_buff) {
            g_critical(MODULE"%s: Ran out of memory for parse", __func__);
            return 0;
        }

        pri_len = cw_read(pri_xml_cwfile, (void *) pri_buff, CHUNK_SIZE);
        if (pri_len < 0) {
            g_critical(MODULE"%s: Read error", __func__);
            return 0;
        }

        if (! XML_ParseBuffer(pri_p, pri_len, pri_len == 0)) {
            g_critical(MODULE"%s: Parse error at line: %d (%s)", __func__,
                                (int) XML_GetCurrentLineNumber(pri_p),
                                (char *) XML_ErrorString(XML_GetErrorCode(pri_p)));
            return 0;
        }

        if (pri_len == 0) {
            break;
        }
    }

    assert(!pri_pd.pkg);
    pri_pd.context = ROOT;
    pri_pd.last_elem = NONE_ELEM;

    // This loop should iterate over package chunks in filelists.xml
    for (;;) {
        char *fil_buff;
        int fil_len;

        fil_buff = XML_GetBuffer(fil_p, CHUNK_SIZE);
        if (!fil_buff) {
            g_critical(MODULE"%s: Ran out of memory for parse", __func__);
            return 0;
        }

        fil_len = cw_read(fil_xml_cwfile, (void *) fil_buff, CHUNK_SIZE);
        if (fil_len < 0) {
            g_critical(MODULE"%s: Read error", __func__);
            return 0;
        }

        if (! XML_ParseBuffer(fil_p, fil_len, fil_len == 0)) {
            g_critical(MODULE"%s: Parse error at line: %d (%s)", __func__,
                                (int) XML_GetCurrentLineNumber(fil_p),
                                (char *) XML_ErrorString(XML_GetErrorCode(fil_p)));
            return 0;
        }

        if (fil_len == 0) {
            break;
        }
    }

    assert(!pri_pd.pkg);
    pri_pd.context = ROOT;
    pri_pd.last_elem = NONE_ELEM;

    // This loop should iterate over package chunks in other.xml
    for (;;) {
        char *oth_buff;
        int oth_len;

        oth_buff = XML_GetBuffer(oth_p, CHUNK_SIZE);
        if (!oth_buff) {
            g_critical(MODULE"%s: Ran out of memory for parse", __func__);
            return 0;
        }

        oth_len = cw_read(oth_xml_cwfile, (void *) oth_buff, CHUNK_SIZE);
        if (oth_len < 0) {
            g_critical(MODULE"%s: Read error", __func__);
            return 0;
        }

        if (! XML_ParseBuffer(oth_p, oth_len, oth_len == 0)) {
            g_critical(MODULE"%s: Parse error at line: %d (%s)", __func__,
                                (int) XML_GetCurrentLineNumber(oth_p),
                                (char *) XML_ErrorString(XML_GetErrorCode(oth_p)));
            return 0;
        }

        if (oth_len == 0) {
            break;
        }
    }


    // Cleanup

    XML_ParserFree(pri_p);
    XML_ParserFree(fil_p);
    XML_ParserFree(oth_p);
    cw_close(pri_xml_cwfile);
    cw_close(fil_xml_cwfile);
    cw_close(oth_xml_cwfile);

    // TODO: free userdata
    g_string_free(pri_pd.current_string, TRUE);

    return 1;
}



int locate_and_load_xml_metadata(GHashTable *hashtable, const char *repopath)
{
    if (!hashtable || !repopath || !g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        return 0;
    }


    // Get paths of old metadata files from repomd

    struct MetadataLocation *ml;
    ml = locate_metadata_via_repomd(repopath);
    if (!ml) {
        return 0;
    }


    if (!ml->pri_xml_href || !ml->fil_xml_href || !ml->oth_xml_href) {
        // Some file(s) is/are missing
        free_metadata_location(ml);
        return 0;
    }


    // Load metadata

    int result;
    GHashTable *intern_hashtable; // key is checksum (pkgId)

    intern_hashtable = new_metadata_hashtable();
    result = load_xml_metadata(intern_hashtable, ml->pri_xml_href, ml->fil_xml_href, ml->oth_xml_href);
    g_debug(MODULE"%s: Parsed items: %d\n", __func__, g_hash_table_size(intern_hashtable));

    // Fill user hashtable and use user selected key

    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init (&iter, intern_hashtable);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
        Package *pkg = (Package *) value;
        gpointer new_key;

        // TODO: Switch and param for key selection
//        new_key = pkg->name;
//        new_key = pkg->pkgId;
        new_key = pkg->location_href;

        if (g_hash_table_lookup(hashtable, new_key)) {
            g_debug(MODULE"%s: Key \"%s\" already exists in hashtable\n", __func__, (char *) new_key);
            g_hash_table_iter_remove(&iter);
        } else {
            g_hash_table_insert(hashtable, new_key, value);
            g_hash_table_iter_steal(&iter);
        }
    }


    // Cleanup

    destroy_metadata_hashtable(intern_hashtable);
    free_metadata_location(ml);

    return result;
}



struct MetadataLocation *locate_metadata_via_repomd(const char *repopath) {

    if (!repopath || !g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        return NULL;
    }


    // Check if repopath ends with slash

    gboolean repopath_ends_with_slash = FALSE;

    if (g_str_has_suffix(repopath, "/")) {
        repopath_ends_with_slash = TRUE;
    }


    // Create path to repomd.xml and check if it exists

    gchar *repomd;

    if (repopath_ends_with_slash) {
        repomd = g_strconcat(repopath, "repodata/repomd.xml", NULL);
    } else {
        repomd = g_strconcat(repopath, "/repodata/repomd.xml", NULL);
    }

    if (!g_file_test(repomd, G_FILE_TEST_EXISTS)) {
        g_debug(MODULE"%s: %s doesn't exists", __func__, repomd);
        g_free(repomd);
        return NULL;
    }


    // Parse repomd.xml

    int ret;
    xmlChar *name;
    xmlTextReaderPtr reader;
    reader = xmlReaderForFile(repomd, NULL, XML_PARSE_NOBLANKS);

    ret = xmlTextReaderRead(reader);
    name = xmlTextReaderName(reader);
    if (g_strcmp0((char *) name, "repomd")) {
        g_warning(MODULE"%s: Bad xml - missing repomd element? (%s)", __func__, name);
        xmlFree(name);
        xmlFreeTextReader(reader);
        g_free(repomd);
        return NULL;
    }
    xmlFree(name);

    ret = xmlTextReaderRead(reader);
    name = xmlTextReaderName(reader);
    if (g_strcmp0((char *) name, "revision")) {
        g_warning(MODULE"%s: Bad xml - missing revision element? (%s)", __func__, name);
        xmlFree(name);
        xmlFreeTextReader(reader);
        g_free(repomd);
        return NULL;
    }
    xmlFree(name);


    // Parse data elements

    while (ret) {
        // Find first data element
        ret = xmlTextReaderRead(reader);
        name = xmlTextReaderName(reader);
        if (!g_strcmp0((char *) name, "data")) {
            xmlFree(name);
            break;
        }
        xmlFree(name);
    }

    if (!ret) {
        // No elements left -> Bad xml
        g_warning(MODULE"%s: Bad xml - missing data elements?", __func__);
        xmlFreeTextReader(reader);
        g_free(repomd);
        return NULL;
    }

    struct MetadataLocation *mdloc;
    mdloc = g_malloc0(sizeof(struct MetadataLocation));
    mdloc->repomd = repomd;

    xmlChar *data_type = NULL;
    xmlChar *location_href = NULL;

    while (ret) {
        if (xmlTextReaderNodeType(reader) != 1) {
            ret = xmlTextReaderNext(reader);
            continue;
        }

        xmlNodePtr data_node = xmlTextReaderExpand(reader);
        data_type = xmlGetProp(data_node, (xmlChar *) "type");
        xmlNodePtr sub_node = data_node->children;

        while (sub_node) {
            if (sub_node->type != XML_ELEMENT_NODE) {
                sub_node = xmlNextElementSibling(sub_node);
                continue;
            }

            if (!g_strcmp0((char *) sub_node->name, "location")) {
                location_href = xmlGetProp(sub_node, (xmlChar *) "href");
            }

            // TODO: Check repodata validity checksum? mtime? size?

            sub_node = xmlNextElementSibling(sub_node);
        }


        // Build absolute path

        gchar *full_location_href;
        if (repopath_ends_with_slash) {
            full_location_href = g_strconcat(repopath, (char *) location_href, NULL);
        } else {
            full_location_href = g_strconcat(repopath, "/", (char *) location_href, NULL);
        }


        // Store the path

        if (!g_strcmp0((char *) data_type, "primary")) {
            mdloc->pri_xml_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "filelists")) {
            mdloc->fil_xml_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "other")) {
            mdloc->oth_xml_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "primary_db")) {
            mdloc->pri_sqlite_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "filelists_db")) {
            mdloc->fil_sqlite_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "other_db")) {
            mdloc->oth_sqlite_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "group")) {
            mdloc->groupfile_href = full_location_href;
        } else if (!g_strcmp0((char *) data_type, "group_gz")) { // even with a createrepo param --xz this name has a _gz suffix
            mdloc->cgroupfile_href = full_location_href;
        }


        // Memory cleanup

        xmlFree(data_type);
        xmlFree(location_href);
        location_href = NULL;

        ret = xmlTextReaderNext(reader);
    }

    xmlFreeTextReader(reader);

    // Note: Do not free repomd! It is pointed from mdloc structure!

    return mdloc;
}


// Return list of non-null pointers on strings in the passed structure
GSList *get_list_of_md_locations (struct MetadataLocation *ml)
{
    GSList *list = NULL;

    if (!ml) {
        return list;
    }

    if (ml->pri_xml_href)    list = g_slist_prepend(list, (gpointer) ml->pri_xml_href);
    if (ml->fil_xml_href)    list = g_slist_prepend(list, (gpointer) ml->fil_xml_href);
    if (ml->oth_xml_href)    list = g_slist_prepend(list, (gpointer) ml->oth_xml_href);
    if (ml->pri_sqlite_href) list = g_slist_prepend(list, (gpointer) ml->pri_sqlite_href);
    if (ml->fil_sqlite_href) list = g_slist_prepend(list, (gpointer) ml->fil_sqlite_href);
    if (ml->oth_sqlite_href) list = g_slist_prepend(list, (gpointer) ml->oth_sqlite_href);
    if (ml->groupfile_href)  list = g_slist_prepend(list, (gpointer) ml->groupfile_href);
    if (ml->cgroupfile_href) list = g_slist_prepend(list, (gpointer) ml->cgroupfile_href);
    if (ml->repomd)          list = g_slist_prepend(list, (gpointer) ml->repomd);

    return list;
}



void free_list_of_md_locations(GSList *list)
{
    if (list) {
        g_slist_free(list);
    }
}



int remove_old_metadata(const char *repopath)
{
    if (!repopath || !g_file_test(repopath, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        return -1;
    }

    gchar *full_repopath;
    full_repopath = g_strconcat(repopath, "/repodata/", NULL);

    GDir *repodir;
    repodir = g_dir_open(full_repopath, 0, NULL);
    if (!repodir) {
        g_debug(MODULE"%s: Path %s doesn't exists", __func__, repopath);
        return -1;
    }


    // Remove all metadata listed in repomd.xml

    int removed_files = 0;

    struct MetadataLocation *ml;
    ml = locate_metadata_via_repomd(repopath);
    if (ml) {
        GSList *list = get_list_of_md_locations(ml);
        GSList *element;

        for (element=list; element; element=element->next) {
            gchar *path = (char *) element->data;

            g_debug("%s: Removing: %s (path obtained from repomd.xml)", __func__, path);
            if (g_remove(path) == -1) {
                g_warning("%s: remove_old_metadata: Cannot remove %s", __func__, path);
            } else {
                removed_files++;
            }
        }

        free_list_of_md_locations(list);
        free_metadata_location(ml);
    }


    // (Just for sure) List dir and remove all files which could be related to an old metadata

    const gchar *file;
    while ((file = g_dir_read_name (repodir))) {
        if (g_str_has_suffix(file, "primary.xml.gz") ||
            g_str_has_suffix(file, "filelists.xml.gz") ||
            g_str_has_suffix(file, "other.xml.gz") ||
            g_str_has_suffix(file, "primary.xml.bz2") ||
            g_str_has_suffix(file, "filelists.xml.bz2") ||
            g_str_has_suffix(file, "other.xml.bz2") ||
            g_str_has_suffix(file, "primary.xml") ||
            g_str_has_suffix(file, "filelists.xml") ||
            g_str_has_suffix(file, "other.xml") ||
            !g_strcmp0(file, "repomd.xml"))
        {
            gchar *path;
            path = g_strconcat(full_repopath, file, NULL);

            g_debug(MODULE"%s: Removing: %s", __func__, path);
            if (g_remove(path) == -1) {
                g_warning(MODULE"%s: Cannot remove %s", __func__, path);
            } else {
                removed_files++;
            }

            g_free(path);
        }
    }

    g_dir_close(repodir);
    g_free(full_repopath);

    return removed_files;
}
