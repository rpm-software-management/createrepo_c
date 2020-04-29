/*
 * Copyright (C) 2018 Red Hat, Inc.
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __C_CREATEREPOLIB_MERGEREPO_C_H__
#define __C_CREATEREPOLIB_MERGEREPO_C_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "compression_wrapper.h"

#define DEFAULT_DB_COMPRESSION_TYPE             CR_CW_BZ2_COMPRESSION
#define DEFAULT_GROUPFILE_COMPRESSION_TYPE      CR_CW_GZ_COMPRESSION

typedef enum {
    MM_DEFAULT,
    // NA == Name, Arch
    MM_FIRST_FROM_IDENTICAL_NA = MM_DEFAULT,
    MM_NEWEST_FROM_IDENTICAL_NA,
    MM_WITH_HIGHEST_NEVRA,
    MM_FIRST_FROM_IDENTICAL_NEVRA,
    MM_ALL_WITH_IDENTICAL_NEVRA
} MergeMethod;

struct CmdOptions {

    // Items filled by cmd option parser

    gboolean version;
    char **repos;
    char *repo_prefix_search;
    char *repo_prefix_replace;
    char *archlist;
    gboolean database;
    gboolean no_database;
    gboolean verbose;
    char *outputdir;
    char *outputrepo;
    gboolean nogroups;
    gboolean noupdateinfo;
    char *compress_type;
    gboolean zck_compression;
    char *zck_dict_dir;
    char *merge_method_str;
    gboolean all;
    char *noarch_repo_url;
    gboolean unique_md_filenames;
    gboolean simple_md_filenames;
    gboolean omit_baseurl;

    // Koji mergerepos specific options
    gboolean koji;
    gboolean koji_simple;
    gboolean pkgorigins;
    gboolean arch_expand;
    char *groupfile;
    char *blocked;

    // Items filled by check_arguments()

    char *out_dir;
    char *out_repo;
    char *tmp_out_repo;
    GSList *repo_list;
    GSList *arch_list;
    cr_CompressionType db_compression_type;
    cr_CompressionType groupfile_compression_type;
    MergeMethod merge_method;
};

#ifdef __cplusplus
}
#endif

#endif /* __C_CREATEREPOLIB_MERGEREPO_C_H__ */
