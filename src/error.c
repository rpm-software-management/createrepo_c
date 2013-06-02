/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2013      Tomas Mlcoch
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

#include "error.h"

const char *
cr_strerror(cr_Error rc)
{
    switch (rc) {
        case CRE_OK:
            return "No error";
        case CRE_ERROR:
            return "No specified error";
        case CRE_IO:
            return "Input/Output error";
        case CRE_MEMORY:
            return "Out of memory";
        case CRE_STAT:
            return "stat() system call failed";
        case CRE_DB:
            return "Database error";
        case CRE_BADARG:
            return "Bad function argument(s)";
        case CRE_NOFILE:
            return "File doesn't exist";
        case CRE_NODIR:
            return "Directory doesn't exist";
        case CRE_EXISTS:
            return "File/Directory already exists";
        case CRE_UNKNOWNCHECKSUMTYPE:
            return "Unknown/Unsupported checksum type";
        case CRE_UNKNOWNCOMPRESSION:
            return "Unknown/Usupported compression";
        case CRE_XMLPARSER:
            return "Error while parsing XML";
        case CRE_XMLDATA:
            return "Loaded XML data are bad";
        case CRE_CBINTERRUPTED:
            return "Interrupted by callback";
        case CRE_BADXMLPRIMARY:
            return "Bad primary XML";
        case CRE_BADXMLFILELISTS:
            return "Bad filelists XML";
        case CRE_BADXMLOTHER:
            return "Bad other XML";
        case CRE_MAGIC:
            return "Magic Number Recognition Library (libmagic) error";
        case CRE_GZ:
            return "Gzip library related error";
        case CRE_BZ2:
            return "Bzip2 library related error";
        case CRE_XZ:
            return "XZ (lzma) library related error";
        case CRE_OPENSSL:
            return "OpenSSL library related error";
        case CRE_CURL:
            return "Curl library related error";
        default:
            return "Unknown error";
    }
}

GQuark
cr_checksum_error_quark(void)
{
    return g_quark_from_static_string("cr_checksum_error");
}

GQuark
cr_compression_wrapper_error_quark(void)
{
    return g_quark_from_static_string("cr_compression_wrapper_error");
}

GQuark
cr_db_error_quark(void)
{
    return g_quark_from_static_string("cr_db_error");
}

GQuark
cr_load_metadata_error_quark(void)
{
    return g_quark_from_static_string("cr_load_metadata_error");
}

GQuark
cr_locate_metadata_error_quark(void)
{
    return g_quark_from_static_string("cr_locate_metadata_error");
}

GQuark
cr_parsepkg_error_quark(void)
{
    return g_quark_from_static_string("cr_parsepkg_error");
}

GQuark
cr_misc_error_quark(void)
{
    return g_quark_from_static_string("cr_misc_error");
}

GQuark
cr_repomd_error_quark(void)
{
    return g_quark_from_static_string("cr_repomd_error");
}

GQuark
cr_repomd_record_error_quark(void)
{
    return g_quark_from_static_string("cr_repomd_record_error");
}

GQuark
cr_xml_dump_filelists_error_quark(void)
{
    return g_quark_from_static_string("cr_xml_dump_filelists_error");
}

GQuark
cr_xml_dump_other_error_quark(void)
{
    return g_quark_from_static_string("cr_xml_dump_other_error");
}

GQuark
cr_xml_dump_primary_error_quark(void)
{
    return g_quark_from_static_string("cr_xml_dump_primary_error");
}

GQuark
cr_xml_file_error_quark(void)
{
    return g_quark_from_static_string("cr_xml_file_error");
}

GQuark
cr_xml_parser_error_quark(void)
{
    return g_quark_from_static_string("cr_xml_parser_error");
}

GQuark
cr_xml_parser_fil_error_quark(void)
{
    return g_quark_from_static_string("cr_xml_parser_filelists_error");
}

GQuark
cr_xml_parser_oth_error_quark(void)
{
    return g_quark_from_static_string("cr_xml_parser_other_error");
}

GQuark
cr_xml_parser_pri_error_quark(void)
{
    return g_quark_from_static_string("cr_xml_parser_primary_error");
}
