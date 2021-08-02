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

#ifndef __C_CREATEREPOLIB_TEST_FIXTURES_H__
#define __C_CREATEREPOLIB_TEST_FIXTURES_H__

#define TMPDIR_TEMPLATE                 "/tmp/cr_testXXXXXX"

#define TEST_DATA_PATH                  "testdata/"

#define TEST_COMPRESSED_FILES_PATH      TEST_DATA_PATH"compressed_files/"
#define TEST_MODIFIED_REPO_FILES_PATH   TEST_DATA_PATH"modified_repo_files/"
#define TEST_PACKAGES_PATH              TEST_DATA_PATH"packages/"
#define TEST_REPO_00                    TEST_DATA_PATH"repo_00/"
#define TEST_REPO_01                    TEST_DATA_PATH"repo_01/"
#define TEST_REPO_02                    TEST_DATA_PATH"repo_02/"
#define TEST_REPO_03                    TEST_DATA_PATH"repo_03/"
#define TEST_REPO_KOJI_01               TEST_DATA_PATH"repo_koji_01/"
#define TEST_REPO_KOJI_02               TEST_DATA_PATH"repo_koji_02/"
#define TEST_FILES_PATH                 TEST_DATA_PATH"test_files/"
#define TEST_UPDATEINFO_FILES_PATH      TEST_DATA_PATH"updateinfo_files/"
#define TEST_REPO_WITH_ADDITIONAL_METADATA    TEST_DATA_PATH"repo_with_additional_metadata/"
#define TEST_REPODATA_SNIPPETS          TEST_DATA_PATH"repodata_snippets/"

// Repo files

#define TEST_REPO_00_REPOMD     TEST_REPO_00"repodata/repomd.xml"
#define TEST_REPO_00_PRIMARY    TEST_REPO_00"repodata/1cb61ea996355add02b1426ed4c1780ea75ce0c04c5d1107c025c3fbd7d8bcae-primary.xml.gz"
#define TEST_REPO_00_FILELISTS  TEST_REPO_00"repodata/95a4415d859d7120efb6b3cf964c07bebbff9a5275ca673e6e74a97bcbfb2a5f-filelists.xml.gz"
#define TEST_REPO_00_OTHER      TEST_REPO_00"repodata/ef3e20691954c3d1318ec3071a982da339f4ed76967ded668b795c9e070aaab6-other.xml.gz"

#define TEST_REPO_01_REPOMD     TEST_REPO_01"repodata/repomd.xml"
#define TEST_REPO_01_PRIMARY    TEST_REPO_01"repodata/6c662d665c24de9a0f62c17d8fa50622307739d7376f0d19097ca96c6d7f5e3e-primary.xml.gz"
#define TEST_REPO_01_FILELISTS  TEST_REPO_01"repodata/c7db035d0e6f1b2e883a7fa3229e2d2be70c05a8b8d2b57dbb5f9c1a67483b6c-filelists.xml.gz"
#define TEST_REPO_01_OTHER      TEST_REPO_01"repodata/b752a73d9efd4006d740f943db5fb7c2dd77a8324bd99da92e86bd55a2c126ef-other.xml.gz"

#define TEST_REPO_02_REPOMD     TEST_REPO_02"repodata/repomd.xml"
#define TEST_REPO_02_PRIMARY    TEST_REPO_02"repodata/bcde64b04916a2a72fdc257d61bc922c70b3d58e953499180585f7a360ce86cf-primary.xml.gz"
#define TEST_REPO_02_FILELISTS  TEST_REPO_02"repodata/3b7e6ecd01af9cb674aff6458186911d7081bb5676d5562a21a963afc8a8bcc7-filelists.xml.gz"
#define TEST_REPO_02_OTHER      TEST_REPO_02"repodata/ab5d3edeea50f9b4ec5ee13e4d25c147e318e3a433dbabc94d3461f58ac28255-other.xml.gz"

// REPO_03 is a copy of REPO_01 with some module metadata
#define TEST_REPO_03_REPOMD     TEST_REPO_03"repodata/repomd.xml"
#define TEST_REPO_03_PRIMARY    TEST_REPO_03"repodata/1cb61ea996355add02b1426ed4c1780ea75ce0c04c5d1107c025c3fbd7d8bcae-primary.xml.gz"
#define TEST_REPO_03_FILELISTS  TEST_REPO_03"repodata/95a4415d859d7120efb6b3cf964c07bebbff9a5275ca673e6e74a97bcbfb2a5f-filelists.xml.gz"
#define TEST_REPO_03_OTHER      TEST_REPO_03"repodata/ef3e20691954c3d1318ec3071a982da339f4ed76967ded668b795c9e070aaab6-other.xml.gz"
#define TEST_REPO_03_MODULEMD   TEST_REPO_03"repodata/a850093e240506c728d6ce26a6fc51d6a7fe10730c67988d13afa7dd82df82d5-modules.yaml.xz"

#define TEST_REPO_WITH_ADDITIONAL_METADATA_REPOMD                TEST_REPO_WITH_ADDITIONAL_METADATA"repodata/repomd.xml"
#define TEST_REPO_WITH_ADDITIONAL_METADATA_PRIMARY_XML_GZ        TEST_REPO_WITH_ADDITIONAL_METADATA"repodata/490a2a494a3827b8a356f728ac36bc02fb009b0eaea173c890e727bb54219037-primary.xml.gz"
#define TEST_REPO_WITH_ADDITIONAL_METADATA_PRIMARY_SQLITE_BZ2    TEST_REPO_WITH_ADDITIONAL_METADATA"repodata/1e12239bf5cb07ec73c74482c35e80dabe30dbe2fdd57bd9e557d987cbacc8c2-primary.sqlite.bz2"
#define TEST_REPO_WITH_ADDITIONAL_METADATA_FILELISTS_XML_GZ      TEST_REPO_WITH_ADDITIONAL_METADATA"repodata/ba5a4fdbb20e7b9b70d9a9abd974bcab1065b1e81d711f80e06ad8cae30c4183-filelists.xml.gz"
#define TEST_REPO_WITH_ADDITIONAL_METADATA_FILELISTS_SQLITE_BZ2  TEST_REPO_WITH_ADDITIONAL_METADATA"repodata/4f4de7d3254a033b84626f330bc6adb8a3c1a4a20f0ddbe30a5692a041318c81-filelists.sqlite.bz2"
#define TEST_REPO_WITH_ADDITIONAL_METADATA_OTHER_XML_GZ          TEST_REPO_WITH_ADDITIONAL_METADATA"repodata/fd458a424a3f3e0dadc95b806674b79055c24e73637e47ad5a6e57926aa1b9d1-other.xml.gz"
#define TEST_REPO_WITH_ADDITIONAL_METADATA_OTHER_SQLITE_BZ2      TEST_REPO_WITH_ADDITIONAL_METADATA"repodata/8b13cba732c1a02b841f43d6791ca68788d45f376787d9f3ccf68e75f01af499-other.sqlite.bz2"

// Modified repo files (MFR)

#define TEST_MRF_BAD_TYPE_FIL   TEST_MODIFIED_REPO_FILES_PATH"bad_file_type-filelists.xml"
#define TEST_MRF_NO_PKGID_FIL   TEST_MODIFIED_REPO_FILES_PATH"no_pkgid-filelists.xml"
#define TEST_MRF_NO_PKGID_OTH   TEST_MODIFIED_REPO_FILES_PATH"no_pkgid-other.xml"
#define TEST_MRF_MISSING_TYPE_REPOMD TEST_MODIFIED_REPO_FILES_PATH"missing_type-repomd.xml"
#define TEST_MRF_UE_PRI_00      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_00-primary.xml"
#define TEST_MRF_UE_PRI_01      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_01-primary.xml"
#define TEST_MRF_UE_PRI_02      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_02-primary.xml"
#define TEST_MRF_UE_FIL_00      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_00-filelists.xml"
#define TEST_MRF_UE_FIL_01      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_01-filelists.xml"
#define TEST_MRF_UE_FIL_02      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_02-filelists.xml"
#define TEST_MRF_UE_OTH_00      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_00-other.xml"
#define TEST_MRF_UE_OTH_01      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_01-other.xml"
#define TEST_MRF_UE_OTH_02      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_02-other.xml"
#define TEST_LONG_PRIMARY       TEST_MODIFIED_REPO_FILES_PATH"long_primary.xml"
#define TEST_DIFF_ORDER_FILELISTS TEST_MODIFIED_REPO_FILES_PATH"repo_02_different_order_filelists.xml"

// Test files

#define TEST_EMPTY_FILE          TEST_FILES_PATH"empty_file"
#define TEST_TEXT_FILE           TEST_FILES_PATH"text_file"
#define TEST_TEXT_FILE_SHA256SUM "2f395bdfa2750978965e4781ddf224c89646c7d7a1569b7ebb023b170f7bd8bb"
#define TEST_TEXT_FILE_GZ        TEST_FILES_PATH"text_file.gz"
#define TEST_TEXT_FILE_XZ        TEST_FILES_PATH"text_file.xz"
#define TEST_SQLITE_FILE         TEST_FILES_PATH"sqlite_file.sqlite"
#define TEST_BINARY_FILE         TEST_FILES_PATH"binary_file"

// Repodata snippets

#define TEST_FILELISTS_SNIPPET_01   TEST_REPODATA_SNIPPETS"filelists_snippet_01.xml"
#define TEST_FILELISTS_SNIPPET_02   TEST_REPODATA_SNIPPETS"filelists_snippet_02.xml"

// Other

#define NON_EXIST_FILE          "/tmp/foobarfile.which.should.not.exists"

// Updateinfo files

#define TEST_UPDATEINFO_00      TEST_UPDATEINFO_FILES_PATH"updateinfo_00.xml"
#define TEST_UPDATEINFO_01      TEST_UPDATEINFO_FILES_PATH"updateinfo_01.xml"
#define TEST_UPDATEINFO_02      TEST_UPDATEINFO_FILES_PATH"updateinfo_02.xml.xz"
#define TEST_UPDATEINFO_03      TEST_UPDATEINFO_FILES_PATH"updateinfo_03.xml"

#include "createrepo/package.h"

cr_Package *
get_package()
{
    cr_Package *p;
    cr_Dependency *dep;
    cr_PackageFile *file;

    p = cr_package_new();
    p->pkgId = "123456";
    p->name = "foo";
    p->arch = "x86_64";
    p->version = "1.2.3";
    p->epoch = "1";
    p->release = "2";
    p->summary = "foo package";
    p->description = "super cool package";
    p->url = "http://package.com";
    p->time_file = 123456;
    p->time_build = 234567;
    p->rpm_license = "GPL";
    p->rpm_vendor = NULL;
    p->rpm_group = NULL;
    p->rpm_buildhost = NULL;
    p->rpm_sourcerpm = "foo.src.rpm";
    p->rpm_header_start = 20;
    p->rpm_header_end = 120;
    p->rpm_packager = NULL;
    p->size_package = 123;
    p->size_installed = 20;
    p->size_archive = 30;
    p->location_href = "foo.rpm";
    p->location_base = "/test/";
    p->checksum_type = "sha256";

    dep = cr_dependency_new();
    dep->name = "foobar_provide";
    dep->flags = NULL;
    dep->pre = FALSE;
    p->provides = (g_slist_prepend(p->provides, dep));

    dep = cr_dependency_new();
    dep->name = "foobar_dep";
    dep->flags = NULL;
    dep->pre = FALSE;
    dep->epoch = "3";
    p->requires = (g_slist_prepend(p->requires, dep));

    dep = cr_dependency_new();
    dep->name = "foobar_pre_dep";
    dep->flags = "LE";
    dep->epoch = "3";
    dep->pre = TRUE;
    p->requires = g_slist_prepend(p->requires, dep);

    file = cr_package_file_new();
    file->type = "";
    file->path = "/bin/";
    file->name = "foo";
    p->files = g_slist_prepend(p->files, file);

    file = cr_package_file_new();
    file->type = "dir";
    file->path = "/var/foo/";
    file->name = NULL;
    p->files = g_slist_prepend(p->files, file);

    file = cr_package_file_new();
    file->type = "dir";
    file->path = "/var/foo/";
    file->name = "baz";
    p->files = g_slist_prepend(p->files, file);
    return p;
}

cr_Package *
get_empty_package()
{
    cr_Package *p;
    cr_Dependency *dep;
    cr_PackageFile *file;

    p = cr_package_new();
    p->name = "foo";

    dep = cr_dependency_new();
    dep->name   = NULL;
    dep->flags  = NULL;
    dep->pre    = FALSE;
    p->requires = (g_slist_prepend(p->requires, dep));

    dep = cr_dependency_new();
    dep->name   = NULL;
    dep->flags  = NULL;
    dep->pre    = TRUE;
    p->requires = g_slist_prepend(p->requires, dep);

    file = cr_package_file_new();
    file->type = NULL;
    file->path = NULL;
    file->name = NULL;
    p->files   = g_slist_prepend(p->files, file);

    return p;
}

#endif
