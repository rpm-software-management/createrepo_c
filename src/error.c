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

GQuark
cr_checksum_error_quark(void)
{
    return g_quark_from_static_string("cr_checksum_error");
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
