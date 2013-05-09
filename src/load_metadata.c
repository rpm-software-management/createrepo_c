/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <expat.h>
#include "error.h"
#include "package.h"
#include "logging.h"
#include "misc.h"
#include "load_metadata.h"
#include "compression_wrapper.h"
#include "locate_metadata.h"

#define STRINGCHUNK_SIZE        16384
#define CHUNK_SIZE              8192
#define PKGS_REALLOC_STEP       2000


/** TODO:
 * This module has one known issue about a memory management.
 * The issue acts in cases when you are using one single string chunk
 * for all parsed packages.
 * (use_single_chunk != 0 during call of cr_metadata_new())
 *
 * Description of issue:
 * During parsing of primary.xml, all string from obtained during parsing are
 * stored into the chunk. When we have all the information from primary.xml and
 * we found out that we don't want the package (according the pkglist passed
 * via cr_metadata_new) and we drop the package (package is not inserted
 * into the hashtable of metadatas), all strings from primary.xml are yet
 * stored in the chunk and they remains there!
 *
 * This issue is not so important, but it shoud be fixed in future.
 *                                                      Tomas
 **/

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
    cr_Package **pkgs;

    GString *current_string;
    GHashTable *hashtable;
    cr_Package *pkg;
    ParserContext context;
    TextElement last_elem;

    GStringChunk *chunk;
    GHashTable *pkglist;

    gboolean error;
};



void
cr_free_values(gpointer data)
{
    cr_package_free((cr_Package *) data);
}



GHashTable *
cr_new_metadata_hashtable()
{
    GHashTable *hashtable = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  NULL, cr_free_values);
    return hashtable;
}



void
cr_destroy_metadata_hashtable(GHashTable *hashtable)
{
    if (hashtable)
        g_hash_table_destroy (hashtable);
}



cr_Metadata
cr_metadata_new(cr_HashTableKey key, int use_single_chunk, GSList *pkglist)
{
    cr_Metadata md;

    assert(key < CR_HT_KEY_SENTINEL);

    md = g_malloc0(sizeof(*md));
    md->key = key;
    md->ht = cr_new_metadata_hashtable();
    if (use_single_chunk)
        md->chunk = g_string_chunk_new(STRINGCHUNK_SIZE);

    if (pkglist) {
        // Create hashtable from pkglist
        // This hashtable is used for checking if the metadata of the package
        // should be included.
        // Purpose is to save memory - We load only metadata about
        // packages which we will probably use.
        // This hashtable is modified "on the fly" - If we found and load
        // a metadata about the package, we remove its record from the hashtable.
        // So if we met the metadata for this package again we will ignore it.
        md->pkglist_ht = g_hash_table_new_full(g_str_hash,
                                               g_str_equal,
                                               g_free,
                                               NULL);

       for (GSList *elem = pkglist; elem; elem = g_slist_next(elem))
            g_hash_table_insert(md->pkglist_ht, g_strdup(elem->data), NULL);
    }

    return md;
}



void
cr_metadata_free(cr_Metadata md)
{
    if (!md)
        return;

    cr_destroy_metadata_hashtable(md->ht);
    if (md->chunk)
        g_string_chunk_free(md->chunk);
    if (md->pkglist_ht)
        g_hash_table_destroy(md->pkglist_ht);
    g_free(md);
}



static inline gchar *
cr_chunk_insert_len_or_null (GStringChunk *chunk, const gchar *str, gssize len)
{
    if (!str || len <= 0)
        return NULL;

    return g_string_chunk_insert_len(chunk, str, len);
}



// primary.xml parser handlers

void
cr_pri_start_handler(void *data, const char *el, const char **attr)
{
    struct ParserData *ppd = (struct ParserData *) data;
    cr_Package *pkg = ppd->pkg;
    int i;

    // <file> and <rpm:entry> are most frequently used tags in primary.xml

    // <file>
    if (!strcmp(el, "file")) {
        ppd->last_elem = FILE_ELEM;

    // <rpm:entry>
    } else if (!strcmp(el, "rpm:entry")) {
        if (!pkg) {
            ppd->error = TRUE;
            g_critical("%s: Have <rpm:entry> but pkg object doesn't exist!",
                       __func__);
            return;
        }

        cr_Dependency *dependency;
        dependency = cr_dependency_new();

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
                g_warning("%s: Unknown attribute \"%s\"", __func__, attr[i]);
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
                g_warning("%s: Bad context (%d) for rpm:entry", __func__,
                          ppd->context);
                break;
        }

    // <package>
    } else if (!strcmp(el, "package")) {
        // Check sanity
        if (ppd->context != METADATA) {
            ppd->error = TRUE;
            g_critical("%s: Package element: Bad XML context!", __func__);
            return;
        }
        if (ppd->pkg) {
            ppd->error = TRUE;
            g_critical("%s: Package element: Pkg pointer is not NULL",
                       __func__);
            return;
        }

        ppd->context = PACKAGE;
        if (ppd->chunk) {
            ppd->pkg = cr_package_new_without_chunk();
            ppd->pkg->chunk = ppd->chunk;
        } else
            ppd->pkg = cr_package_new();

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
            g_critical("%s: Have <version> but pkg object doesn't exist!",
                       __func__);
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
                g_warning("%s: Unknown attribute \"%s\"", __func__, attr[i]);
            }
        }

    // <checksum>
    } else if (!strcmp(el, "checksum")) {
        ppd->last_elem = CHECKSUM_ELEM;

        if (!pkg) {
            ppd->error = TRUE;
            g_critical("%s: Have <checksum> but pkg object doesn't exist!",
                       __func__);
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
            g_critical("%s: Have <time> but pkg object doesn't exist!", __func__);
            return;
        }

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "file")) {
                pkg->time_file = g_ascii_strtoll(attr[i+1], NULL, 10);
            } else if (!strcmp(attr[i], "build")) {
                pkg->time_build = g_ascii_strtoll(attr[i+1], NULL, 10);
            } else {
                g_warning("%s: Unknown attribute \"%s\"", __func__, attr[i]);
            }
        }

    // <size>
    } else if (!strcmp(el, "size")) {
        if (!pkg) {
            ppd->error = TRUE;
            g_critical("%s: Have <size> but pkg object doesn't exist!",
                       __func__);
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
                g_warning("%s: Unknown attribute \"%s\"", __func__, attr[i]);
            }
        }

    // <location>
    } else if (!strcmp(el, "location")) {
        if (!pkg) {
            ppd->error = TRUE;
            g_critical("%s: Have <location> but pkg object doesn't exist!",
                       __func__);
            return;
        }

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "href")) {
                pkg->location_href = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else if (!strcmp(attr[i], "xml:base")) {
                pkg->location_base = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else {
                g_warning("%s: Unknown attribute \"%s\"", __func__, attr[i]);
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
            g_critical("%s: Have <rpm:header-range> but pkg object doesn't exist!",
                       __func__);
            return;
        }

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "start")) {
                pkg->rpm_header_start = g_ascii_strtoll(attr[i+1], NULL, 10);
            } else if (!strcmp(attr[i], "end")) {
                pkg->rpm_header_end = g_ascii_strtoll(attr[i+1], NULL, 10);
            } else {
                g_warning("%s: Unknown attribute \"%s\"", __func__, attr[i]);
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
            g_critical("%s: Bad context (%d) for %s element", __func__,
                       ppd->context, el);
            return;
        }
        ppd->context = METADATA;

    // unknown element
    } else {
        ppd->error = TRUE;
        g_warning("%s: Unknown element: %s", __func__, el);
    }
}



void
cr_pri_char_handler(void *data, const char *txt, int txtlen)
{
    struct ParserData *ppd = (struct ParserData *) data;
    cr_Package *pkg = ppd->pkg;

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
            g_warning("%s: Unknown xml element (%d) in primary.xml",
                      __func__, ppd->last_elem);
            break;
    }
}



void
cr_pri_end_handler(void *data, const char *el)
{
    struct ParserData *ppd = (struct ParserData *) data;
    cr_Package *pkg = ppd->pkg;


    // Store strings

    if (ppd->last_elem != NONE_ELEM && pkg) {
        gchar *txt = ppd->current_string->str;
        gsize txtlen = ppd->current_string->len;

        switch (ppd->last_elem) {
            case NAME_ELEM:
                pkg->name = cr_chunk_insert_len_or_null(pkg->chunk, txt, txtlen);
                break;
            case ARCH_ELEM:
                pkg->arch = cr_chunk_insert_len_or_null(pkg->chunk, txt, txtlen);
                break;
            case CHECKSUM_ELEM:
                pkg->pkgId = cr_chunk_insert_len_or_null(pkg->chunk, txt, txtlen);
                break;
            case SUMMARY_ELEM:
                pkg->summary = cr_chunk_insert_len_or_null(pkg->chunk, txt, txtlen);
                break;
            case DESCRIPTION_ELEM:
                pkg->description = cr_chunk_insert_len_or_null(pkg->chunk, txt, txtlen);
                break;
            case PACKAGER_ELEM:
                pkg->rpm_packager = cr_chunk_insert_len_or_null(pkg->chunk, txt, txtlen);
                break;
            case URL_ELEM:
                pkg->url = cr_chunk_insert_len_or_null(pkg->chunk, txt, txtlen);
                break;
            case RPM_LICENSE_ELEM:
                pkg->rpm_license = cr_chunk_insert_len_or_null(pkg->chunk, txt, txtlen);
                break;
            case RPM_VENDOR_ELEM:
                pkg->rpm_vendor = cr_chunk_insert_len_or_null(pkg->chunk, txt, txtlen);
                break;
            case RPM_GROUP_ELEM:
                pkg->rpm_group = cr_chunk_insert_len_or_null(pkg->chunk, txt, txtlen);
                break;
            case RPM_BUILDHOST_ELEM:
                pkg->rpm_buildhost = cr_chunk_insert_len_or_null(pkg->chunk, txt, txtlen);
                break;
            case RPM_SOURCERPM_ELEM:
                pkg->rpm_sourcerpm = cr_chunk_insert_len_or_null(pkg->chunk, txt, txtlen);
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
            gboolean store = TRUE;

            // Check if packages should be stored
            if (ppd->pkglist && pkg->location_href) {
                gchar *basename = cr_get_filename(pkg->location_href);
                store = g_hash_table_lookup_extended(ppd->pkglist,
                                                     basename,
                                                     NULL,
                                                     NULL);
                if (store)  // remove pkg from allowed packages
                    g_hash_table_remove(ppd->pkglist, basename);
            }

            if (ppd->chunk)
                pkg->chunk = NULL;

            if (store) {
                // Store package into the hashtable
                char *key = pkg->pkgId;
                if (key && key[0] != '\0') {
                    g_hash_table_replace(ppd->hashtable, key, ppd->pkg);
                } else {
                    g_warning("%s: Empty hashtable key!", __func__);
                }

                // Reverse lists
                pkg->requires  = g_slist_reverse(pkg->requires);
                pkg->provides  = g_slist_reverse(pkg->provides);
                pkg->conflicts = g_slist_reverse(pkg->conflicts);
                pkg->obsoletes = g_slist_reverse(pkg->obsoletes);
            } else
                cr_package_free(pkg);

            // Update ParserData
            ppd->pkg = NULL;
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

void
cr_fil_start_handler(void *data, const char *el, const char **attr)
{
    int i;
    struct ParserData *ppd = (struct ParserData *) data;
    cr_Package *pkg = ppd->pkg;

    if (!pkg && ppd->context == PACKAGE) {
        // There is no pkg object -> no need to parse package element
        return;
    }

    // <file>
    if (!strcmp(el, "file")) {
        ppd->last_elem = FILE_ELEM;

        if (ppd->context != PACKAGE) {
            ppd->error = TRUE;
            g_critical("%s: Bad context of <file> element!", __func__);
            return;
        }

        if (!pkg) {
            ppd->error = TRUE;
            g_critical("%s: Have <file> but pkg object doesn't exist!", __func__);
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
                g_warning("%s: Unknown attribute \"%s\"", __func__, attr[i]);
            }
        }

    // <package>
    } else if (!strcmp(el, "package")) {
        // Check sanity
        if (ppd->context != FILELISTS) {
            ppd->error = TRUE;
            g_critical("%s: Package element: Bad XML context!", __func__);
            return;
        }
        if (ppd->pkg) {
            ppd->error = TRUE;
            g_critical("%s: Package element: Pkg pointer is not NULL", __func__);
            return;
        }

        ppd->context = PACKAGE;

        gchar *key = NULL;
        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "pkgid")) {
                key = (gchar *) attr[i+1];
            }
        }

        if (key) {
            ppd->pkg = (cr_Package *) g_hash_table_lookup(ppd->hashtable,
                                                          (gconstpointer) key);
            // The package could not be in the hashtable. If the ppd->pkglist
            // is used and the package is not listed in it, then no record
            // in the hastable was created during parsing primary.xml.

            //if (!ppd->pkg) {
            //    ppd->error = TRUE;
            //    g_critical("%s: Unknown package (package ID: %s)",
            //               __func__, key);
            //}
        } else {
            ppd->error = TRUE;
            g_critical("%s: Package withou pkgid attribute found!", __func__);
        }

        if (ppd->pkg && ppd->chunk)
            ppd->pkg->chunk = ppd->chunk;

    // <version>
    } else if (!strcmp(el, "version")) {
        ;

    // <filelists>
    } else if (!strcmp(el, "filelists")) {
        if (ppd->context != ROOT) {
            ppd->error = TRUE;
            g_critical("%s: Bad context (%d) for %s element", __func__,
                       ppd->context, el);
            return;
        }
        ppd->context = FILELISTS;

    // Unknown element
    } else {
        ppd->error = TRUE;
        g_warning("%s: Unknown element: %s", __func__, el);
    }
}



void
cr_fil_char_handler(void *data, const char *txt, int txtlen)
{
    struct ParserData *ppd = (struct ParserData *) data;
    cr_Package *pkg = ppd->pkg;

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
            g_warning("%s: Unknown xml element (%d) in filelists.xml",
                      __func__, ppd->last_elem);
            break;
    }
}



void
cr_fil_end_handler(void *data, const char *el)
{
    struct ParserData *ppd = (struct ParserData *) data;
    cr_Package *pkg = ppd->pkg;


    // Store strings

    if (ppd->last_elem != NONE_ELEM && pkg) {
        cr_PackageFile *file;
        gchar *filename;
        gchar *txt = ppd->current_string->str;
        gsize txtlen = ppd->current_string->len;

        switch (ppd->last_elem) {
            case FILE_ELEM:
            case FILE_DIR_ELEM:
            case FILE_GHOST_ELEM:
                if (!txt || txtlen == 0) {
                    g_warning("%s: File with empty filename found!",
                              __func__);
                    break;
                }

                file = cr_package_file_new();
                filename = cr_get_filename(txt);
                file->name = g_string_chunk_insert(pkg->chunk, filename);
                txt[txtlen-strlen(filename)] = '\0';
                file->path = g_string_chunk_insert_const(pkg->chunk, txt);

                if (ppd->last_elem == FILE_ELEM) {
                    file->type = NULL; // "file";
                } else if (ppd->last_elem == FILE_DIR_ELEM) {
                    file->type = "dir";
                } else if (ppd->last_elem == FILE_GHOST_ELEM) {
                    file->type = "ghost";
                }

                pkg->files = g_slist_prepend(pkg->files, file);
                break;

            default:
                ppd->error = TRUE;
                g_warning("%s: Bad last xml element state (%d) in filelists.xml",
                          __func__, ppd->last_elem);
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
            if (ppd->chunk)
                pkg->chunk = NULL;
        }

    } else if (!strcmp(el, "filelists")) {
        ppd->context = ROOT;
    }
}


// other.xml parser handlers

void
cr_oth_start_handler(void *data, const char *el, const char **attr)
{
    int i;
    struct ParserData *ppd = (struct ParserData *) data;
    cr_Package *pkg = ppd->pkg;

    if (!pkg && ppd->context == PACKAGE) {
        // There is no pkg object -> no need to parse package element
        return;
    }

    // <changelog>
    if (!strcmp(el, "changelog")) {
        ppd->last_elem = CHANGELOG_ELEM;

        if (ppd->context != PACKAGE) {
            ppd->error = TRUE;
            g_critical("%s: Bad context of <changelog> element!", __func__);
            return;
        }

        if (!pkg) {
            ppd->error = TRUE;
            g_critical("%s: Have <changelog> but pkg object doesn't exist!",
                       __func__);
            return;
        }

        cr_ChangelogEntry *changelog_entry;
        changelog_entry = cr_changelog_entry_new();

        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "author")) {
                changelog_entry->author = g_string_chunk_insert(pkg->chunk, attr[i+1]);
            } else if (!strcmp(attr[i], "date")) {
                changelog_entry->date = g_ascii_strtoll(attr[i+1], NULL, 10);;
            } else {
                g_warning("%s: Unknown attribute \"%s\"", __func__, attr[i]);
            }
        }

        pkg->changelogs = g_slist_prepend(pkg->changelogs, changelog_entry);

    // <package>
    } else if (!strcmp(el, "package")) {
        // Check sanity
        if (ppd->context != OTHERDATA) {
            ppd->error = TRUE;
            g_critical("%s: Package element: Bad XML context (%d)!",
                       __func__, ppd->context);
            return;
        }
        if (ppd->pkg) {
            ppd->error = TRUE;
            g_critical("%s: Package element: Pkg pointer is not NULL",
                       __func__);
            return;
        }

        ppd->context = PACKAGE;

        gchar *key = NULL;
        for (i = 0; attr[i]; i += 2) {
            if (!strcmp(attr[i], "pkgid")) {
                key = (gchar *) attr[i+1];
            }
        }

        if (key) {
            ppd->pkg = (cr_Package *) g_hash_table_lookup(ppd->hashtable,
                                                       (gconstpointer) key);

            // The package could not be in the hashtable. If the ppd->pkglist
            // is used and the package is not listed in it, then no record
            // in the hastable was created during parsing primary.xml.

            //if (!ppd->pkg) {
            //    ppd->error = TRUE;
            //    g_critical("%s: Unknown package (package ID: %s)",
            //               __func__, key);
            //}
        } else {
            ppd->error = TRUE;
            g_critical("%s: Package withou pkgid attribute found!",
                       __func__);
        }

        if (ppd->pkg && ppd->chunk)
            ppd->pkg->chunk = ppd->chunk;

    // <version>
    } else if (!strcmp(el, "version")) {
        ;

    // <otherdata>
    } else if (!strcmp(el, "otherdata")) {
        if (ppd->context != ROOT) {
            ppd->error = TRUE;
            g_critical("%s: Bad context (%d) for %s element", __func__,
                       ppd->context, el);
            return;
        }
        ppd->context = OTHERDATA;

    // Unknown element
    } else {
        ppd->error = TRUE;
        g_warning("%s: Unknown element: %s", __func__, el);
    }
}



void
cr_oth_char_handler(void *data, const char *txt, int txtlen)
{
    struct ParserData *ppd = (struct ParserData *) data;
    cr_Package *pkg = ppd->pkg;

    if (!pkg || ppd->last_elem == NONE_ELEM) {
        return;
    }

    switch (ppd->last_elem) {
        case CHANGELOG_ELEM:
            g_string_append_len(ppd->current_string, txt, (gsize) txtlen);
            break;
        default:
            ppd->error = TRUE;
            g_warning("%s: Unknown xml element (%d) in other.xml",
                      __func__, ppd->last_elem);
            break;
    }
}



void
cr_oth_end_handler(void *data, const char *el)
{
    struct ParserData *ppd = (struct ParserData *) data;
    cr_Package *pkg = ppd->pkg;


    // Store strings

    if (ppd->last_elem != NONE_ELEM && pkg) {
        gchar *txt = ppd->current_string->str;
        gsize txtlen = ppd->current_string->len;

        switch (ppd->last_elem) {
            case CHANGELOG_ELEM:
                ((cr_ChangelogEntry *) pkg->changelogs->data)->changelog = \
                            g_string_chunk_insert_len(pkg->chunk, txt, txtlen);
                break;

            default:
                ppd->error = TRUE;
                g_warning("%s: Bad last xml element state (%d) in other.xml",
                          __func__, ppd->last_elem);
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
            if (ppd->chunk)
                pkg->chunk = NULL;
        }

    } else if (!strcmp(el, "otherdata")) {
        ppd->context = ROOT;
    }
}



static int
cr_load_xml_files(GHashTable *hashtable,
                  const char *primary_xml_path,
                  const char *filelists_xml_path,
                  const char *other_xml_path,
                  GStringChunk *chunk,
                  GHashTable *pkglist_ht,
                  GError **err)
{
    int ret = CRE_OK;
    cr_CompressionType compression_type;
    CR_FILE *pri_xml_cwfile, *fil_xml_cwfile, *oth_xml_cwfile;
    XML_Parser pri_p, fil_p, oth_p;
    struct ParserData parser_data;

    assert(!err || *err == NULL);

    // Detect compression type

    compression_type = cr_detect_compression(primary_xml_path);
    if (compression_type == CR_CW_UNKNOWN_COMPRESSION) {
        g_debug("%s: Unknown compression", __func__);
        g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_UNKNOWNCOMPRESSION,
                    "Unknown compression of %s", primary_xml_path);
        return CRE_UNKNOWNCOMPRESSION;
    }


    // Open files

    if (!(pri_xml_cwfile = cr_open(primary_xml_path, CR_CW_MODE_READ, compression_type))) {
        g_debug("%s: Cannot open file: %s", __func__, primary_xml_path);
        g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_IO,
                    "Cannot open %s", primary_xml_path);
        return CRE_IO;
    }

    if (!(fil_xml_cwfile = cr_open(filelists_xml_path, CR_CW_MODE_READ, compression_type))) {
        g_debug("%s: Cannot open file: %s", __func__, filelists_xml_path);
        g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_IO,
                    "Cannot open %s", primary_xml_path);
        return CRE_IO;
    }

    if (!(oth_xml_cwfile = cr_open(other_xml_path, CR_CW_MODE_READ, compression_type))) {
        g_debug("%s: Cannot open file: %s", __func__, other_xml_path);
        g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_IO,
                    "Cannot open %s", primary_xml_path);
        return CRE_IO;
    }


    // Prepare parsers

    parser_data.current_string = g_string_sized_new(1024);
    parser_data.hashtable = hashtable;
    parser_data.pkg = NULL;
    parser_data.context = ROOT;
    parser_data.last_elem = NONE_ELEM;
    parser_data.error = FALSE;
    parser_data.chunk = chunk;
    parser_data.pkglist = pkglist_ht;

    pri_p = XML_ParserCreate(NULL);
    XML_SetUserData(pri_p, (void *) &parser_data);
    XML_SetElementHandler(pri_p, cr_pri_start_handler, cr_pri_end_handler);
    XML_SetCharacterDataHandler(pri_p, cr_pri_char_handler);

    fil_p = XML_ParserCreate(NULL);
    XML_SetUserData(fil_p, (void *) &parser_data);
    XML_SetElementHandler(fil_p, cr_fil_start_handler, cr_fil_end_handler);
    XML_SetCharacterDataHandler(fil_p, cr_fil_char_handler);

    oth_p = XML_ParserCreate(NULL);
    XML_SetUserData(oth_p, (void *) &parser_data);
    XML_SetElementHandler(oth_p, cr_oth_start_handler, cr_oth_end_handler);
    XML_SetCharacterDataHandler(oth_p, cr_oth_char_handler);


    // Parse

    // This loop should iterate over package chunks in primary.xml
    for (;;) {
        char *pri_buff;
        int pri_len;

        pri_buff = XML_GetBuffer(pri_p, CHUNK_SIZE);
        if (!pri_buff) {
            g_critical("%s: Ran out of memory for parse", __func__);
            ret = CRE_MEMORY;
            g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_MEMORY,
                        "Cannot allocate buffer for primary xml parser");
            break;
        }

        pri_len = cr_read(pri_xml_cwfile, (void *) pri_buff, CHUNK_SIZE);
        if (pri_len < 0) {
            g_critical("%s: Read error", __func__);
            ret = CRE_IO;
            g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_IO,
                        "Error while reading primary");
            break;
        }

        if (! XML_ParseBuffer(pri_p, pri_len, pri_len == 0)) {
            g_critical("%s: Parse error at line: %d (%s)", __func__,
                                (int) XML_GetCurrentLineNumber(pri_p),
                                (char *) XML_ErrorString(XML_GetErrorCode(pri_p)));
            ret = CRE_XMLPARSER;
            g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_XMLPARSER,
                        "Primary parse error at line: %d (%s)",
                        (int) XML_GetCurrentLineNumber(pri_p),
                        (char *) XML_ErrorString(XML_GetErrorCode(pri_p)));
            break;
        }

        if (pri_len == 0)
            break;
    }

    if (parser_data.error || ret != CRE_OK)
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
            g_critical("%s: Ran out of memory for parse", __func__);
            ret = CRE_MEMORY;
            g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_MEMORY,
                        "Cannot allocate buffer for filelists xml parser");
            break;
        }

        fil_len = cr_read(fil_xml_cwfile, (void *) fil_buff, CHUNK_SIZE);
        if (fil_len < 0) {
            g_critical("%s: Read error", __func__);
            ret = CRE_IO;
            g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_IO,
                        "Error while reading filelists");
            break;
        }

        if (! XML_ParseBuffer(fil_p, fil_len, fil_len == 0)) {
            g_critical("%s: Parse error at line: %d (%s)", __func__,
                                (int) XML_GetCurrentLineNumber(fil_p),
                                (char *) XML_ErrorString(XML_GetErrorCode(fil_p)));
            ret = CRE_XMLPARSER;
            g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_XMLPARSER,
                        "Filelists parse error at line: %d (%s)",
                        (int) XML_GetCurrentLineNumber(fil_p),
                        (char *) XML_ErrorString(XML_GetErrorCode(fil_p)));
            break;
        }

        if (fil_len == 0)
            break;
    }

    if (parser_data.error || ret != CRE_OK)
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
            g_critical("%s: Ran out of memory for parse", __func__);
            ret = CRE_MEMORY;
            g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_MEMORY,
                        "Cannot allocate buffer for other xml parser");
            break;
        }

        oth_len = cr_read(oth_xml_cwfile, (void *) oth_buff, CHUNK_SIZE);
        if (oth_len < 0) {
            g_critical("%s: Read error", __func__);
            ret = CRE_IO;
            g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_IO,
                        "Error while reading filelists");
            break;
        }

        if (! XML_ParseBuffer(oth_p, oth_len, oth_len == 0)) {
            g_critical("%s: Parse error at line: %d (%s)", __func__,
                                (int) XML_GetCurrentLineNumber(oth_p),
                                (char *) XML_ErrorString(XML_GetErrorCode(oth_p)));
            ret = CRE_XMLPARSER;
            g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_XMLPARSER,
                        "Primary parse error at line: %d (%s)",
                        (int) XML_GetCurrentLineNumber(oth_p),
                        (char *) XML_ErrorString(XML_GetErrorCode(oth_p)));
            break;
        }

        if (oth_len == 0)
            break;
    }

// Goto label
cleanup:

    if (ret != CRE_OK) {
        // If there were error during parsing it is important to take care
        // about current processed package if shared chunk is used.
        if (chunk)
            parser_data.pkg->chunk = NULL;
    }

    // Cleanup

    XML_ParserFree(pri_p);
    XML_ParserFree(fil_p);
    XML_ParserFree(oth_p);
    cr_close(pri_xml_cwfile);
    cr_close(fil_xml_cwfile);
    cr_close(oth_xml_cwfile);

    g_string_free(parser_data.current_string, TRUE);

    if (parser_data.error && ret == CRE_OK) {
        ret = CRE_XMLDATA;
        g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_XMLDATA,
                    "Loaded metadata are bad");
    }

    return ret;
}



int
cr_metadata_load_xml(cr_Metadata md,
                     struct cr_MetadataLocation *ml,
                     GError **err)
{
    GError *tmp_err = NULL;

    assert(md);
    assert(ml);
    assert(!err || *err == NULL);

    if (!ml->pri_xml_href || !ml->fil_xml_href || !ml->oth_xml_href) {
        // Some file(s) is/are missing
        g_set_error(err, CR_LOAD_METADATA_ERROR, CRE_BADARG,
                    "Some file is missing");
        return CRE_BADARG;
    }


    // Load metadata

    int result;
    GHashTable *intern_hashtable;  // key is checksum (pkgId)

    intern_hashtable = cr_new_metadata_hashtable();
    result = cr_load_xml_files(intern_hashtable,
                               ml->pri_xml_href,
                               ml->fil_xml_href,
                               ml->oth_xml_href,
                               md->chunk,
                               md->pkglist_ht,
                               &tmp_err);

    if (result != CRE_OK) {
        g_critical("%s: Error encountered while parsing", __func__);
        g_propagate_prefixed_error(err, tmp_err,
                                   "Error encountered while parsing:");
        cr_destroy_metadata_hashtable(intern_hashtable);
        return result;
    }

    g_debug("%s: Parsed items: %d", __func__,
            g_hash_table_size(intern_hashtable));

    // Fill user hashtable and use user selected key

    GHashTableIter iter;
    gpointer p_key, p_value;

    g_hash_table_iter_init (&iter, intern_hashtable);
    while (g_hash_table_iter_next (&iter, &p_key, &p_value)) {
        cr_Package *pkg = (cr_Package *) p_value;
        gpointer new_key;

        switch (md->key) {
            case CR_HT_KEY_FILENAME:
                new_key = cr_get_filename(pkg->location_href);
                break;
            case CR_HT_KEY_HASH:
                new_key = pkg->pkgId;
                break;
            case CR_HT_KEY_NAME:
                new_key = pkg->name;
                break;
            default:
                // Well, this SHOULD never happend!
                // (md->key SHOULD be setted only by cr_metadata_new()
                // and it SHOULD set only valid key values)
                assert(0);
                g_critical("%s: Unknown hash table key selected", __func__);
                new_key = pkg->pkgId;
                break;
        }

        if (g_hash_table_lookup(md->ht, new_key)) {
            g_debug("%s: Key \"%s\" already exists in hashtable",
                    __func__, (char *) new_key);
            g_hash_table_iter_remove(&iter);
        } else {
            g_hash_table_insert(md->ht, new_key, p_value);
            g_hash_table_iter_steal(&iter);
        }
    }


    // Cleanup

    cr_destroy_metadata_hashtable(intern_hashtable);

    return CRE_OK;
}



int
cr_metadata_locate_and_load_xml(cr_Metadata md,
                                const char *repopath,
                                GError **err)
{
    int ret;
    struct cr_MetadataLocation *ml;

    assert(md);
    assert(repopath);

    ml = cr_locate_metadata(repopath, 1, NULL);
    ret = cr_metadata_load_xml(md, ml, err);
    cr_metadatalocation_free(ml);

    return ret;
}

