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
#define TEST_FILES_PATH                 TEST_DATA_PATH"test_files/"

// Repo files

#define TEST_REPO_00_PRIMARY    TEST_REPO_00"repodata/dabe2ce5481d23de1f4f52bdcfee0f9af98316c9e0de2ce8123adeefa0dd08b9-primary.xml.gz"
#define TEST_REPO_00_FILELISTS  TEST_REPO_00"repodata/401dc19bda88c82c403423fb835844d64345f7e95f5b9835888189c03834cc93-filelists.xml.gz"
#define TEST_REPO_00_OTHER      TEST_REPO_00"repodata/6bf9672d0862e8ef8b8ff05a2fd0208a922b1f5978e6589d87944c88259cb670-other.xml.gz"

#define TEST_REPO_01_PRIMARY    TEST_REPO_01"repodata/6c662d665c24de9a0f62c17d8fa50622307739d7376f0d19097ca96c6d7f5e3e-primary.xml.gz"
#define TEST_REPO_01_FILELISTS  TEST_REPO_01"repodata/c7db035d0e6f1b2e883a7fa3229e2d2be70c05a8b8d2b57dbb5f9c1a67483b6c-filelists.xml.gz"
#define TEST_REPO_01_OTHER      TEST_REPO_01"repodata/b752a73d9efd4006d740f943db5fb7c2dd77a8324bd99da92e86bd55a2c126ef-other.xml.gz"

#define TEST_REPO_02_PRIMARY    TEST_REPO_02"repodata/bcde64b04916a2a72fdc257d61bc922c70b3d58e953499180585f7a360ce86cf-primary.xml.gz"
#define TEST_REPO_02_FILELISTS  TEST_REPO_02"repodata/3b7e6ecd01af9cb674aff6458186911d7081bb5676d5562a21a963afc8a8bcc7-filelists.xml.gz"
#define TEST_REPO_02_OTHER      TEST_REPO_02"repodata/ab5d3edeea50f9b4ec5ee13e4d25c147e318e3a433dbabc94d3461f58ac28255-other.xml.gz"

// Modified repo files (MFR)

#define TEST_MRF_NO_PKGID_FIL   TEST_MODIFIED_REPO_FILES_PATH"no_pkgid-filelists.xml"
#define TEST_MRF_NO_PKGID_OTH   TEST_MODIFIED_REPO_FILES_PATH"no_pkgid-other.xml"
#define TEST_MRF_UE_PRI_00      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_00-primary.xml"
#define TEST_MRF_UE_PRI_01      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_01-primary.xml"
#define TEST_MRF_UE_PRI_02      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_02-primary.xml"
#define TEST_MRF_UE_FIL_00      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_00-filelists.xml"
#define TEST_MRF_UE_FIL_01      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_01-filelists.xml"
#define TEST_MRF_UE_FIL_02      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_02-filelists.xml"
#define TEST_MRF_UE_OTH_00      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_00-other.xml"
#define TEST_MRF_UE_OTH_01      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_01-other.xml"
#define TEST_MRF_UE_OTH_02      TEST_MODIFIED_REPO_FILES_PATH"unknown_element_02-other.xml"

#endif
