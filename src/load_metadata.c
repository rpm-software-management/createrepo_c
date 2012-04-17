#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <expat.h>
#include "package.h"
#include "logging.h"
#include "misc.h"
#include "load_metadata.h"
#include "compression_wrapper.h"
#include "locate_metadata.h"

#undef MODULE
#define MODULE "load_metadata: "

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
} TextElement;



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
} ParserContext;



struct ParserData {
    int total_pkgs;
    int actual_pkg;
    int pkgs_size;
    Package **pkgs;

    GString *current_string;
    GHashTable *hashtable;
    Package *pkg;
    ParserContext context;
    TextElement last_elem;

    gboolean error;
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



// primary.xml parser handlers

void pri_start_handler(void *data, const char *el, const char **attr) {
    struct ParserData *ppd = (struct ParserData *) data;
    Package *pkg = ppd->pkg;
    int i;

    // <file> and <rpm:entry> are most frequently used tags in primary.xml

    // <file>
    if (!strcmp(el, "file")) {
        ppd->last_elem = FILE_ELEM;

    // <rpm:entry>
    } else if (!strcmp(el, "rpm:entry")) {
        if (!pkg) {
            ppd->error = TRUE;
            g_critical(MODULE"%s: Have <rpm:entry> but pkg object doesn't exist!", __func__);
            return;
        }

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
            ppd->error = TRUE;
            g_critical(MODULE"%s: Package element: Bad XML context!", __func__);
            return;
        }
        if (ppd->pkg) {
            ppd->error = TRUE;
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
        if (!pkg) {
            ppd->error = TRUE;
            g_critical(MODULE"%s: Have <version> but pkg object doesn't exist!", __func__);
            return;
        }

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

        if (!pkg) {
            ppd->error = TRUE;
            g_critical(MODULE"%s: Have <checksum> but pkg object doesn't exist!", __func__);
            return;
        }

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
        if (!pkg) {
            ppd->error = TRUE;
            g_critical(MODULE"%s: Have <time> but pkg object doesn't exist!", __func__);
            return;
        }

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
        if (!pkg) {
            ppd->error = TRUE;
            g_critical(MODULE"%s: Have <size> but pkg object doesn't exist!", __func__);
            return;
        }

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
        if (!pkg) {
            ppd->error = TRUE;
            g_critical(MODULE"%s: Have <location> but pkg object doesn't exist!", __func__);
            return;
        }

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
        if (!pkg) {
            ppd->error = TRUE;
            g_critical(MODULE"%s: Have <rpm:header-range> but pkg object doesn't exist!", __func__);
            return;
        }

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
            ppd->error = TRUE;
            g_critical(MODULE"%s: Bad context (%d) for %s element", __func__, ppd->context, el);
            return;
        }
        ppd->context = METADATA;

    // unknown element
    } else {
        ppd->error = TRUE;
        g_warning(MODULE"%s: Unknown element: %s", __func__, el);
    }
}



void pri_char_handler(void *data, const char *txt, int txtlen) {
    struct ParserData *ppd = (struct ParserData *) data;
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
            ppd->error = TRUE;
            g_warning(MODULE"%s: Unknown xml element (%d) in primary.xml", __func__, ppd->last_elem);
            break;
    }
}



void pri_end_handler(void *data, const char *el) {
    struct ParserData *ppd = (struct ParserData *) data;
    Package *pkg = ppd->pkg;


    // Store strings

    if (ppd->last_elem != NONE_ELEM && pkg) {
        gchar *txt = ppd->current_string->str;
        gsize txtlen = ppd->current_string->len;

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

        if (ppd->pkg) {
            // Store package into the hashtable
            char *key = pkg->pkgId;
            if (key && key[0] != '\0') {
                g_hash_table_replace(ppd->hashtable, key, ppd->pkg);
            } else {
                g_warning(MODULE"%s: Empty hashtable key!", __func__);
            }

            // Update ParserData
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
    struct ParserData *ppd = (struct ParserData *) data;
    Package *pkg = ppd->pkg;


    // <file>
    if (!strcmp(el, "file")) {
        ppd->last_elem = FILE_ELEM;

        if (!pkg) {
            ppd->error = TRUE;
            g_critical(MODULE"%s: Have <file> but pkg object doesn't exist!", __func__);
            return;
        }

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
            ppd->error = TRUE;
            g_critical(MODULE"%s: Package element: Bad XML context!", __func__);
            return;
        }
        if (ppd->pkg) {
            ppd->error = TRUE;
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
                ppd->error = TRUE;
                g_critical(MODULE"%s: Unknown package (package ID: %s)", __func__, key);
            }
        } else {
            ppd->error = TRUE;
            g_critical(MODULE"%s: Package withou pkgid attribute found!", __func__);
        }

    // <version>
    } else if (!strcmp(el, "version")) {
        ;

    // <filelists>
    } else if (!strcmp(el, "filelists")) {
        if (ppd->context != ROOT) {
            ppd->error = TRUE;
            g_critical(MODULE"%s: Bad context (%d) for %s element", __func__, ppd->context, el);
            return;
        }
        ppd->context = FILELISTS;

    // Unknown element
    } else {
        ppd->error = TRUE;
        g_warning(MODULE"%s: Unknown element: %s", __func__, el);
    }
}



void fil_char_handler(void *data, const char *txt, int txtlen) {
    struct ParserData *ppd = (struct ParserData *) data;
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
        default:
            ppd->error = TRUE;
            g_warning(MODULE"%s: Unknown xml element (%d) in filelists.xml", __func__, ppd->last_elem);
            break;
    }
}



void fil_end_handler(void *data, const char *el) {
    struct ParserData *ppd = (struct ParserData *) data;
    Package *pkg = ppd->pkg;


    // Store strings

    if (ppd->last_elem != NONE_ELEM && pkg) {
        PackageFile *file;
        gchar *filename;
        gchar *txt = ppd->current_string->str;
        gsize txtlen = ppd->current_string->len;

        switch (ppd->last_elem) {
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
                break;

            default:
                ppd->error = TRUE;
                g_warning(MODULE"%s: Bad last xml element state (%d) in filelists.xml", __func__, ppd->last_elem);
                break;
        }

        ppd->last_elem = NONE_ELEM;
    }

    g_string_erase(ppd->current_string, 0, -1);


    // Set proper context

    if (!strcmp(el, "package")) {
        ppd->context = FILELISTS;

        if (pkg) {
            // Reverse list of files
            pkg->files = g_slist_reverse(pkg->files);
            ppd->pkg = NULL;
        }

    } else if (!strcmp(el, "filelists")) {
        ppd->context = ROOT;
    }
}


// other.xml parser handlers

void oth_start_handler(void *data, const char *el, const char **attr) {
    int i;
    struct ParserData *ppd = (struct ParserData *) data;
    Package *pkg = ppd->pkg;


    // <changelog>
    if (!strcmp(el, "changelog")) {
        ppd->last_elem = CHANGELOG_ELEM;

        if (!pkg) {
            ppd->error = TRUE;
            g_critical(MODULE"%s: Have <changelog> but pkg object doesn't exist!", __func__);
            return;
        }

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
            ppd->error = TRUE;
            g_critical(MODULE"%s: Package element: Bad XML context (%d)!", __func__, ppd->context);
            return;
        }
        if (ppd->pkg) {
            ppd->error = TRUE;
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
                ppd->error = TRUE;
                g_critical(MODULE"%s: Unknown package (package ID: %s)", __func__, key);
            }
        } else {
            ppd->error = TRUE;
            g_critical(MODULE"%s: Package withou pkgid attribute found!", __func__);
        }

    // <version>
    } else if (!strcmp(el, "version")) {
        ;

    // <otherdata>
    } else if (!strcmp(el, "otherdata")) {
        if (ppd->context != ROOT) {
            ppd->error = TRUE;
            g_critical(MODULE"%s: Bad context (%d) for %s element", __func__, ppd->context, el);
            return;
        }
        ppd->context = OTHERDATA;

    // Unknown element
    } else {
        ppd->error = TRUE;
        g_warning(MODULE"%s: Unknown element: %s", __func__, el);
    }
}



void oth_char_handler(void *data, const char *txt, int txtlen) {
    struct ParserData *ppd = (struct ParserData *) data;
    Package *pkg = ppd->pkg;

    if (!pkg || ppd->last_elem == NONE_ELEM) {
        return;
    }

    switch (ppd->last_elem) {
        case CHANGELOG_ELEM:
            g_string_append_len(ppd->current_string, txt, (gsize) txtlen);
            break;
        default:
            ppd->error = TRUE;
            g_warning(MODULE"%s: Unknown xml element (%d) in other.xml", __func__, ppd->last_elem);
            break;
    }
}



void oth_end_handler(void *data, const char *el) {
    struct ParserData *ppd = (struct ParserData *) data;
    Package *pkg = ppd->pkg;


    // Store strings

    if (ppd->last_elem != NONE_ELEM && pkg) {
        gchar *txt = ppd->current_string->str;
        gsize txtlen = ppd->current_string->len;

        switch (ppd->last_elem) {
            case CHANGELOG_ELEM:
                ((ChangelogEntry *) pkg->changelogs->data)->changelog = g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;

            default:
                ppd->error = TRUE;
                g_warning(MODULE"%s: Bad last xml element state (%d) in other.xml", __func__, ppd->last_elem);
                break;
        }

        ppd->last_elem = NONE_ELEM;
    }

    g_string_erase(ppd->current_string, 0, -1);


    // Set proper context

    if (!strcmp(el, "package")) {
        ppd->context = OTHERDATA;

        if (pkg) {
            // Reverse list of changelogs
            pkg->changelogs = g_slist_reverse(pkg->changelogs);
            ppd->pkg = NULL;
        }

    } else if (!strcmp(el, "otherdata")) {
        ppd->context = ROOT;
    }
}



int load_xml_metadata(GHashTable *hashtable, const char *primary_xml_path, const char *filelists_xml_path, const char *other_xml_path)
{
    CompressionType compression_type;
    CW_FILE *pri_xml_cwfile, *fil_xml_cwfile, *oth_xml_cwfile;
    XML_Parser pri_p, fil_p, oth_p;
    struct ParserData parser_data;


    // Detect compression type

    compression_type = detect_compression(primary_xml_path);
    if (compression_type == UNKNOWN_COMPRESSION) {
        g_debug(MODULE"%s: Unknown compression", __func__);
        return LOAD_METADATA_ERR;
    }


    // Open files

    if (!(pri_xml_cwfile = cw_open(primary_xml_path, CW_MODE_READ, compression_type))) {
        g_debug(MODULE"%s: Cannot open file: %s", __func__, primary_xml_path);
        return LOAD_METADATA_ERR;
    }

    if (!(fil_xml_cwfile = cw_open(filelists_xml_path, CW_MODE_READ, compression_type))) {
        g_debug(MODULE"%s: Cannot open file: %s", __func__, filelists_xml_path);
        return LOAD_METADATA_ERR;
    }

    if (!(oth_xml_cwfile = cw_open(other_xml_path, CW_MODE_READ, compression_type))) {
        g_debug(MODULE"%s: Cannot open file: %s", __func__, other_xml_path);
        return LOAD_METADATA_ERR;
    }


    // Prepare parsers

    // XXX: Maybe try rely on package order and do not use hashtable
/*    parser_data.total_pkgs = 0;
    parser_data.actual_pkg = 0;
    parser_data.pkgs_size = 0;
    parser_data.pkgs = 0; */
    // XXX

    parser_data.current_string = g_string_sized_new(1024);
    parser_data.hashtable = hashtable;
    parser_data.pkg = NULL;
    parser_data.context = ROOT;
    parser_data.last_elem = NONE_ELEM;
    parser_data.error = FALSE;

    pri_p = XML_ParserCreate(NULL);
    XML_SetUserData(pri_p, (void *) &parser_data);
    XML_SetElementHandler(pri_p, pri_start_handler, pri_end_handler);
    XML_SetCharacterDataHandler(pri_p, pri_char_handler);

    fil_p = XML_ParserCreate(NULL);
    XML_SetUserData(fil_p, (void *) &parser_data);
    XML_SetElementHandler(fil_p, fil_start_handler, fil_end_handler);
    XML_SetCharacterDataHandler(fil_p, fil_char_handler);

    oth_p = XML_ParserCreate(NULL);
    XML_SetUserData(oth_p, (void *) &parser_data);
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
            return LOAD_METADATA_ERR;
        }

        pri_len = cw_read(pri_xml_cwfile, (void *) pri_buff, CHUNK_SIZE);
        if (pri_len < 0) {
            g_critical(MODULE"%s: Read error", __func__);
            return LOAD_METADATA_ERR;
        }

        if (! XML_ParseBuffer(pri_p, pri_len, pri_len == 0)) {
            g_critical(MODULE"%s: Parse error at line: %d (%s)", __func__,
                                (int) XML_GetCurrentLineNumber(pri_p),
                                (char *) XML_ErrorString(XML_GetErrorCode(pri_p)));
            return LOAD_METADATA_ERR;
        }

        if (pri_len == 0) {
            break;
        }
    }

    if (parser_data.error)
        goto cleanup;

    assert(!parser_data.pkg);
    parser_data.context = ROOT;
    parser_data.last_elem = NONE_ELEM;

    // This loop should iterate over package chunks in filelists.xml
    for (;;) {
        char *fil_buff;
        int fil_len;

        fil_buff = XML_GetBuffer(fil_p, CHUNK_SIZE);
        if (!fil_buff) {
            g_critical(MODULE"%s: Ran out of memory for parse", __func__);
            return LOAD_METADATA_ERR;
        }

        fil_len = cw_read(fil_xml_cwfile, (void *) fil_buff, CHUNK_SIZE);
        if (fil_len < 0) {
            g_critical(MODULE"%s: Read error", __func__);
            return LOAD_METADATA_ERR;
        }

        if (! XML_ParseBuffer(fil_p, fil_len, fil_len == 0)) {
            g_critical(MODULE"%s: Parse error at line: %d (%s)", __func__,
                                (int) XML_GetCurrentLineNumber(fil_p),
                                (char *) XML_ErrorString(XML_GetErrorCode(fil_p)));
            return LOAD_METADATA_ERR;
        }

        if (fil_len == 0) {
            break;
        }
    }

    if (parser_data.error)
        goto cleanup;

    assert(!parser_data.pkg);
    parser_data.context = ROOT;
    parser_data.last_elem = NONE_ELEM;

    // This loop should iterate over package chunks in other.xml
    for (;;) {
        char *oth_buff;
        int oth_len;

        oth_buff = XML_GetBuffer(oth_p, CHUNK_SIZE);
        if (!oth_buff) {
            g_critical(MODULE"%s: Ran out of memory for parse", __func__);
            return LOAD_METADATA_ERR;
        }

        oth_len = cw_read(oth_xml_cwfile, (void *) oth_buff, CHUNK_SIZE);
        if (oth_len < 0) {
            g_critical(MODULE"%s: Read error", __func__);
            return LOAD_METADATA_ERR;
        }

        if (! XML_ParseBuffer(oth_p, oth_len, oth_len == 0)) {
            g_critical(MODULE"%s: Parse error at line: %d (%s)", __func__,
                                (int) XML_GetCurrentLineNumber(oth_p),
                                (char *) XML_ErrorString(XML_GetErrorCode(oth_p)));
            return LOAD_METADATA_ERR;
        }

        if (oth_len == 0) {
            break;
        }
    }

// Goto label
cleanup:

    // Cleanup

    XML_ParserFree(pri_p);
    XML_ParserFree(fil_p);
    XML_ParserFree(oth_p);
    cw_close(pri_xml_cwfile);
    cw_close(fil_xml_cwfile);
    cw_close(oth_xml_cwfile);

    g_string_free(parser_data.current_string, TRUE);


    if (parser_data.error) {
        return LOAD_METADATA_ERR;
    }

    return LOAD_METADATA_OK;
}



int locate_and_load_xml_metadata(GHashTable *hashtable, const char *repopath, HashTableKey key)
{
    if (!hashtable || !repopath) {
        return LOAD_METADATA_ERR;
    }


    // Get paths of old metadata files from repomd

    struct MetadataLocation *ml;
    ml = get_metadata_location(repopath);
    if (!ml) {
        return LOAD_METADATA_ERR;
    }


    if (!ml->pri_xml_href || !ml->fil_xml_href || !ml->oth_xml_href) {
        // Some file(s) is/are missing
        free_metadata_location(ml);
        return LOAD_METADATA_ERR;
    }


    // Load metadata

    int result;
    GHashTable *intern_hashtable;  // key is checksum (pkgId)

    intern_hashtable = new_metadata_hashtable();
    result = load_xml_metadata(intern_hashtable, ml->pri_xml_href, ml->fil_xml_href, ml->oth_xml_href);

    if (result == LOAD_METADATA_ERR) {
        g_critical(MODULE"%s: Error encountered while parsing", __func__);
        destroy_metadata_hashtable(intern_hashtable);
        free_metadata_location(ml);
        return result;
    }

    g_debug(MODULE"%s: Parsed items: %d", __func__, g_hash_table_size(intern_hashtable));

    // Fill user hashtable and use user selected key

    GHashTableIter iter;
    gpointer p_key, p_value;

    g_hash_table_iter_init (&iter, intern_hashtable);
    while (g_hash_table_iter_next (&iter, &p_key, &p_value)) {
        Package *pkg = (Package *) p_value;
        gpointer new_key;

        switch (key) {
            case HT_KEY_FILENAME:
                new_key = get_filename(pkg->location_href);
                break;
            case HT_KEY_HASH:
                new_key = pkg->pkgId;
                break;
            case HT_KEY_NAME:
                new_key = pkg->name;
                break;
            default:
                g_critical(MODULE"%s: Unknown hash table key selected", __func__);
                new_key = pkg->pkgId;
                break;
        }

        if (g_hash_table_lookup(hashtable, new_key)) {
            g_debug(MODULE"%s: Key \"%s\" already exists in hashtable", __func__, (char *) new_key);
            g_hash_table_iter_remove(&iter);
        } else {
            g_hash_table_insert(hashtable, new_key, p_value);
            g_hash_table_iter_steal(&iter);
        }
    }


    // Cleanup

    destroy_metadata_hashtable(intern_hashtable);
    free_metadata_location(ml);

    return result;
}
