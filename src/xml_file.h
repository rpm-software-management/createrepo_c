/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2013  Tomas Mlcoch
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

#ifndef __C_CREATEREPOLIB_XML_FILE_H__
#define __C_CREATEREPOLIB_XML_FILE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include "compression_wrapper.h"
#include "package.h"

/** \defgroup   xml_file        XML file API.
 *  \addtogroup xml_file
 *  @{
 */

/** Supported types of xml files
 */
typedef enum {
    CR_XMLFILE_PRIMARY,     /*!< primary.xml */
    CR_XMLFILE_FILELISTS,   /*!< filelists.xml */
    CR_XMLFILE_OTHER,       /*!< other.xml */
    CR_XMLFILE_PRESTODELTA, /*!< prestodelta.xml */
    CR_XMLFILE_UPDATEINFO,  /*!< updateinfo.xml */
    CR_XMLFILE_SENTINEL,    /*!< sentinel of the list */
} cr_XmlFileType;

/** cr_XmlFile structure.
 */
typedef struct {
    CR_FILE *f; /*!<
        File */
    cr_XmlFileType type; /*!<
        Type of XML file. */
    int header; /*!<
        0 if no header was written yet. */
    int footer; /*!<
        0 if no footer was written yet. */
    long pkgs; /*!<
        Number of packages */
} cr_XmlFile;

/** Open a new primary XML file.
 * @param FILENAME      Filename.
 * @param COMTYPE       Type of compression.
 * @param ERR           GError **
 * @return              Opened cr_XmlFile or NULL on error
 */
#define cr_xmlfile_open_primary(FILENAME, COMTYPE, ERR) \
            cr_xmlfile_open(FILENAME, CR_XMLFILE_PRIMARY, COMTYPE, ERR)

/** Open a new primary XML file.
 * @param FILENAME      Filename.
 * @param COMTYPE       Type of compression.
 * @param STAT          cr_ContentStat object or NULL.
 * @param ERR           GError **
 * @return              Opened cr_XmlFile or NULL on error
 */
#define cr_xmlfile_sopen_primary(FILENAME, COMTYPE, STAT, ERR) \
            cr_xmlfile_sopen(FILENAME, CR_XMLFILE_PRIMARY, COMTYPE, STAT,  ERR)

/** Open a new filelists XML file.
 * @param FILENAME      Filename.
 * @param COMTYPE       Type of used compression.
 * @param ERR           GError **
 * @return              Opened cr_XmlFile or NULL on error
 */
#define cr_xmlfile_open_filelists(FILENAME, COMTYPE, ERR) \
            cr_xmlfile_open(FILENAME, CR_XMLFILE_FILELISTS, COMTYPE, ERR)

/** Open a new filelists XML file.
 * @param FILENAME      Filename.
 * @param COMTYPE       Type of compression.
 * @param STAT          cr_ContentStat object or NULL.
 * @param ERR           GError **
 * @return              Opened cr_XmlFile or NULL on error
 */
#define cr_xmlfile_sopen_filelists(FILENAME, COMTYPE, STAT, ERR) \
            cr_xmlfile_sopen(FILENAME, CR_XMLFILE_FILELISTS, COMTYPE, STAT, ERR)

/** Open a new other XML file.
 * @param FILENAME      Filename.
 * @param COMTYPE       Type of used compression.
 * @param ERR           GError **
 * @return              Opened cr_XmlFile or NULL on error
 */
#define cr_xmlfile_open_other(FILENAME, COMTYPE, ERR) \
            cr_xmlfile_open(FILENAME, CR_XMLFILE_OTHER, COMTYPE, ERR)

/** Open a new other XML file.
 * @param FILENAME      Filename.
 * @param COMTYPE       Type of compression.
 * @param STAT          cr_ContentStat object or NULL.
 * @param ERR           GError **
 * @return              Opened cr_XmlFile or NULL on error
 */
#define cr_xmlfile_sopen_other(FILENAME, COMTYPE, STAT, ERR) \
            cr_xmlfile_sopen(FILENAME, CR_XMLFILE_OTHER, COMTYPE, STAT, ERR)

/** Open a new prestodelta XML file.
 * @param FILENAME      Filename.
 * @param COMTYPE       Type of used compression.
 * @param ERR           GError **
 * @return              Opened cr_XmlFile or NULL on error
 */
#define cr_xmlfile_open_prestodelta(FILENAME, COMTYPE, ERR) \
            cr_xmlfile_open(FILENAME, CR_XMLFILE_PRESTODELTA, COMTYPE, ERR)

/** Open a new prestodelta XML file.
 * @param FILENAME      Filename.
 * @param COMTYPE       Type of compression.
 * @param STAT          cr_ContentStat object or NULL.
 * @param ERR           GError **
 * @return              Opened cr_XmlFile or NULL on error
 */
#define cr_xmlfile_sopen_prestodelta(FILENAME, COMTYPE, STAT, ERR) \
            cr_xmlfile_sopen(FILENAME, CR_XMLFILE_PRESTODELTA, COMTYPE, STAT, ERR)

/** Open a new updateinfo XML file.
 * @param FILENAME      Filename.
 * @param COMTYPE       Type of used compression.
 * @param ERR           GError **
 * @return              Opened cr_XmlFile or NULL on error
 */
#define cr_xmlfile_open_updateinfo(FILENAME, COMTYPE, ERR) \
            cr_xmlfile_open(FILENAME, CR_XMLFILE_UPDATEINFO, COMTYPE, ERR)

/** Open a new updateinfo XML file.
 * @param FILENAME      Filename.
 * @param COMTYPE       Type of compression.
 * @param STAT          cr_ContentStat object or NULL.
 * @param ERR           GError **
 * @return              Opened cr_XmlFile or NULL on error
 */
#define cr_xmlfile_sopen_updateinfo(FILENAME, COMTYPE, STAT, ERR) \
            cr_xmlfile_sopen(FILENAME, CR_XMLFILE_UPDATEINFO, COMTYPE, STAT, ERR)

/** Open a new XML file with stats.
 * Note: Opened file must not exists! This function cannot
 * open existing file!.
 * @param FILENAME      Filename.
 * @param TYPE          Type of XML file.
 * @param COMTYPE       Type of used compression.
 * @param ERR           **GError
 * @return              Opened cr_XmlFile or NULL on error
 */
#define cr_xmlfile_open(FILENAME, TYPE, COMTYPE, ERR) \
            cr_xmlfile_sopen(FILENAME, TYPE, COMTYPE, NULL, ERR)

/** Open a new XML file.
 * Note: Opened file must not exists! This function cannot
 * open existing file!.
 * @param filename      Filename.
 * @param type          Type of XML file.
 * @param comtype       Type of used compression.
 * @param stat          pointer to cr_ContentStat or NULL
 * @param err           **GError
 * @return              Opened cr_XmlFile or NULL on error
 */
cr_XmlFile *cr_xmlfile_sopen(const char *filename,
                             cr_XmlFileType type,
                             cr_CompressionType comtype,
                             cr_ContentStat *stat,
                             GError **err);

/** Set total number of packages that will be in the file.
 * This number must be set before any write operation
 * (cr_xml_add_pkg, cr_xml_file_add_chunk, ..).
 * @param f             An opened cr_XmlFile
 * @param num           Total number of packages in the file.
 * @param err           **GError
 * @return              cr_Error code
 */
int cr_xmlfile_set_num_of_pkgs(cr_XmlFile *f, long num, GError **err);

/** Add package to the xml file.
 * @param f             An opened cr_XmlFile
 * @param pkg           Package object.
 * @param err           **GError
 * @return              cr_Error code
 */
int cr_xmlfile_add_pkg(cr_XmlFile *f, cr_Package *pkg, GError **err);

/** Add (write) string with XML chunk into the file.
 * Note: Because of writing, in case of multithreaded program, should be
 * guarded by locks, this function could be much more effective than
 * cr_xml_file_add_pkg(). In case of _add_pkg() function, creating of
 * string with xml chunk is done in a critical section. In _add_chunk()
 * function, you could just dump XML whenever you want and in the
 * critical section do only writting.
 * @param f             An opened cr_XmlFile
 * @param chunk         String with XML chunk.
 * @param err           **GError
 * @return              cr_Error code
 */
int cr_xmlfile_add_chunk(cr_XmlFile *f, const char *chunk, GError **err);

/** Close an opened cr_XmlFile.
 * @param f             An opened cr_XmlFile
 * @param err           **GError
 * @return              cr_Error code
 */
int cr_xmlfile_close(cr_XmlFile *f, GError **err);

/** Rewrite package count field in repodata header in xml file.
 * In order to do this we have to decompress and after the change
 * compress the whole file again, so entirely new file is created.
 * @param original_filename     Current file with wrong value in header
 * @param package_count         Actual package count (desired value in header)
 * @param task_count            Task count (current value in header)
 * @param file_stat             cr_ContentStat for stats of the new file, it will be modified
 * @param zck_dict_file         Optional path to zck dictionary
 * @param err                   **GError
 */
void cr_rewrite_header_package_count(gchar *original_filename,
                                     cr_CompressionType xml_compression,
                                     int package_count,
                                     int task_count,
                                     cr_ContentStat *file_stat,
                                     gchar *zck_dict_file,
                                     GError **err);
 

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_XML_FILE_H__ */
