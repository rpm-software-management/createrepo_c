import os.path

TEST_DATA_PATH = os.path.normpath(os.path.join(__file__, "../../../testdata"))

PACKAGES_PATH = os.path.join(TEST_DATA_PATH, "packages")
REPOS_PATH = TEST_DATA_PATH
COMPRESSED_FILES_PATH = os.path.join(TEST_DATA_PATH, "compressed_files")
TEST_FILES_PATH = os.path.join(TEST_DATA_PATH, "test_files")

# Test packages
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
REPO_00_PRIXML = os.path.join(REPO_00_PATH, "repodata/",
                              "dabe2ce5481d23de1f4f52bdcfee0f9af98316c9e0de2ce8123adeefa0dd08b9-primary.xml.gz")

REPO_01_PATH = os.path.join(REPOS_PATH, "repo_01")
REPO_02_PATH = os.path.join(REPOS_PATH, "repo_02")

# Other test files

FILE_BINARY = "binary_file"
FILE_BINARY_PATH = os.path.join(TEST_FILES_PATH, FILE_BINARY)
FILE_TEXT = "text_file"
FILE_TEXT = os.path.join(TEST_FILES_PATH, FILE_TEXT)
FILE_EMPTY = "empty_file"
FILE_EMPTY = os.path.join(TEST_FILES_PATH, FILE_EMPTY)

