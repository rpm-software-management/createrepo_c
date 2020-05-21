import os.path

TEST_DATA_PATH = os.path.normpath(os.path.join(__file__, "../../../testdata"))

COMPRESSED_FILES_PATH = os.path.join(TEST_DATA_PATH, "compressed_files")
MODIFIED_REPO_FILES_PATH = os.path.join(TEST_DATA_PATH, "modified_repo_files")
PACKAGES_PATH = os.path.join(TEST_DATA_PATH, "packages")
REPOS_PATH = TEST_DATA_PATH
TEST_FILES_PATH = os.path.join(TEST_DATA_PATH, "test_files")
REPODATA_SNIPPETS = os.path.join(TEST_DATA_PATH, "repodata_snippets")
TEST_UPDATEINFO_FILES_PATH = os.path.join(TEST_DATA_PATH, "updateinfo_files/")

# Modified repo files

PRIMARY_ERROR_00_PATH = os.path.join(MODIFIED_REPO_FILES_PATH,
                        "error_00-primary.xml")
PRIMARY_MULTI_WARN_00_PATH = os.path.join(MODIFIED_REPO_FILES_PATH,
                             "multiple_warnings_00-primary.xml")

FILELISTS_ERROR_00_PATH = os.path.join(MODIFIED_REPO_FILES_PATH,
                          "error_00-filelists.xml")
FILELISTS_MULTI_WARN_00_PATH = os.path.join(MODIFIED_REPO_FILES_PATH,
                               "multiple_warnings_00-filelists.xml")

OTHER_ERROR_00_PATH = os.path.join(MODIFIED_REPO_FILES_PATH,
                      "error_00-other.xml")
OTHER_MULTI_WARN_00_PATH = os.path.join(MODIFIED_REPO_FILES_PATH,
                           "multiple_warnings_00-other.xml")

# Packages

PKG_ARCHER = "Archer-3.4.5-6.x86_64.rpm"
PKG_ARCHER_PATH = os.path.join(PACKAGES_PATH, PKG_ARCHER)

PKG_BALICEK_ISO88591 = "balicek-iso88591-1.1.1-1.x86_64.rpm"
PKG_BALICEK_ISO88591_PATH = os.path.join(PACKAGES_PATH, PKG_BALICEK_ISO88591)

PKG_BALICEK_ISO88592 = "balicek-iso88592-1.1.1-1.x86_64.rpm"
PKG_BALICEK_ISO88592_PATH = os.path.join(PACKAGES_PATH, PKG_BALICEK_ISO88592)

PKG_BALICEK_UTF8 = "balicek-utf8-1.1.1-1.x86_64.rpm"
PKG_BALICEK_UTF8_PATH = os.path.join(PACKAGES_PATH, PKG_BALICEK_UTF8)

PKG_EMPTY = "empty-0-0.x86_64.rpm"
PKG_EMPTY_PATH = os.path.join(PACKAGES_PATH, PKG_EMPTY)

PKG_EMPTY_SRC = "empty-0-0.x86_64.rpm"
PKG_EMPTY_SRC_PATH = os.path.join(PACKAGES_PATH, PKG_EMPTY_SRC)

PKG_FAKE_BASH = "fake_bash-1.1.1-1.x86_64.rpm"
PKG_FAKE_BASH_PATH = os.path.join(PACKAGES_PATH, PKG_FAKE_BASH)

PKG_SUPER_KERNEL = "super_kernel-6.0.1-2.x86_64.rpm"
PKG_SUPER_KERNEL_PATH = os.path.join(PACKAGES_PATH, PKG_SUPER_KERNEL)

# Test repositories

REPO_00_PATH = os.path.join(REPOS_PATH, "repo_00")
REPO_00_REPOMD = os.path.join(REPO_00_PATH, "repodata/repomd.xml")
REPO_00_PRIXML = os.path.join(REPO_00_PATH, "repodata/",
    "1cb61ea996355add02b1426ed4c1780ea75ce0c04c5d1107c025c3fbd7d8bcae-primary.xml.gz")
REPO_00_FILXML = os.path.join(REPO_00_PATH, "repodata/",
    "95a4415d859d7120efb6b3cf964c07bebbff9a5275ca673e6e74a97bcbfb2a5f-filelists.xml.gz")
REPO_00_OTHXML = os.path.join(REPO_00_PATH, "repodata/",
    "ef3e20691954c3d1318ec3071a982da339f4ed76967ded668b795c9e070aaab6-other.xml.gz")
REPO_00_PRIZCK = os.path.join(REPO_00_PATH, "repodata/",
    "e0ac03cd77e95e724dbf90ded0dba664e233315a8940051dd8882c56b9878595-primary.xml.zck")
REPO_00_FILZCK = os.path.join(REPO_00_PATH, "repodata/",
    "2e7db4492173b6c437fd1299dc335e63d09f24cbdadeac5175a61b787c2f7a44-filelists.xml.zck")
REPO_00_OTHZCK = os.path.join(REPO_00_PATH, "repodata/",
    "a939c4765106655c3f7a13fb41d0f239824efa66bcd6c1e6c044a854012bda75-other.xml.zck")

REPO_01_PATH = os.path.join(REPOS_PATH, "repo_01")
REPO_01_REPOMD = os.path.join(REPO_01_PATH, "repodata/repomd.xml")
REPO_01_PRIXML = os.path.join(REPO_01_PATH, "repodata/",
    "6c662d665c24de9a0f62c17d8fa50622307739d7376f0d19097ca96c6d7f5e3e-primary.xml.gz")
REPO_01_FILXML = os.path.join(REPO_01_PATH, "repodata/",
    "c7db035d0e6f1b2e883a7fa3229e2d2be70c05a8b8d2b57dbb5f9c1a67483b6c-filelists.xml.gz")
REPO_01_OTHXML = os.path.join(REPO_01_PATH, "repodata/",
    "b752a73d9efd4006d740f943db5fb7c2dd77a8324bd99da92e86bd55a2c126ef-other.xml.gz")

REPO_02_PATH = os.path.join(REPOS_PATH, "repo_02")
REPO_02_REPOMD = os.path.join(REPO_02_PATH, "repodata/repomd.xml")
REPO_02_PRIXML = os.path.join(REPO_02_PATH, "repodata/",
    "bcde64b04916a2a72fdc257d61bc922c70b3d58e953499180585f7a360ce86cf-primary.xml.gz")
REPO_02_FILXML = os.path.join(REPO_02_PATH, "repodata/",
    "3b7e6ecd01af9cb674aff6458186911d7081bb5676d5562a21a963afc8a8bcc7-filelists.xml.gz")
REPO_02_OTHXML = os.path.join(REPO_02_PATH, "repodata/",
    "ab5d3edeea50f9b4ec5ee13e4d25c147e318e3a433dbabc94d3461f58ac28255-other.xml.gz")

REPO_WITH_ADDITIONAL_METADATA = os.path.join(REPOS_PATH, "repo_with_additional_metadata")

# Test files

FILE_BINARY = "binary_file"
FILE_BINARY_PATH = os.path.join(TEST_FILES_PATH, FILE_BINARY)
FILE_TEXT = "text_file"
FILE_TEXT = os.path.join(TEST_FILES_PATH, FILE_TEXT)
FILE_TEXT_SHA256SUM = "2f395bdfa2750978965e4781ddf224c89646c7d7a1569b7ebb023b170f7bd8bb"
FILE_TEXT_GZ = FILE_TEXT+".gz"
FILE_EMPTY = "empty_file"
FILE_EMPTY = os.path.join(TEST_FILES_PATH, FILE_EMPTY)

# Test snippets

PRIMARY_SNIPPET_01 = os.path.join(REPODATA_SNIPPETS, "primary_snippet_01.xml")
PRIMARY_SNIPPET_02 = os.path.join(REPODATA_SNIPPETS, "primary_snippet_02.xml")
FILELISTS_SNIPPET_01 = os.path.join(REPODATA_SNIPPETS, "filelists_snippet_01.xml")
FILELISTS_SNIPPET_02 = os.path.join(REPODATA_SNIPPETS, "filelists_snippet_02.xml")
OTHER_SNIPPET_01 = os.path.join(REPODATA_SNIPPETS, "other_snippet_01.xml")
OTHER_SNIPPET_02 = os.path.join(REPODATA_SNIPPETS, "other_snippet_02.xml")

# Test updateinfo files

TEST_UPDATEINFO_03 = os.path.join(TEST_UPDATEINFO_FILES_PATH, "updateinfo_03.xml")
