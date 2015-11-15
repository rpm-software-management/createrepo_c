import os
import time
import os.path

from .fixtures import PACKAGES
from .base import BaseTestCase

BASE_XML_PATTERNS_SIMPLE = ["repomd\.xml", "primary\.xml\..+", "filelists\.xml\..+", "other\.xml\..+"]
BASE_XML_PATTERNS_UNIQUE = ["repomd\.xml", ".*-primary\.xml\..+", ".*-filelists\.xml\..+", ".*-other\.xml\..+"]

DBS_PATTERNS = [".*primary\.sqlite\..+", ".*filelists\.sqlite\..+", ".*other\.sqlite\..+"]
DBS_PATTERNS_SIMPLE = ["primary\.sqlite\..+", "filelists\.sqlite\..+", "other\.sqlite\..+"]
DBS_PATTERNS_UNIQUE = [".*-primary\.sqlite\..+", ".*-filelists\.sqlite\..+", ".*-other\.sqlite\..+"]

DBS_PATTERNS_UNIQUE_MD5 = ["[0-9a-z]{32}-primary\.sqlite\..+", ".*-filelists\.sqlite\..+", ".*-other\.sqlite\..+"]

DBS_PATTERNS_SIMPLE_GZ = ["primary\.sqlite\.gz", "filelists\.sqlite\.gz", "other\.sqlite\.gz"]
DBS_PATTERNS_SIMPLE_BZ2 = ["primary\.sqlite\.bz2", "filelists\.sqlite\.bz2", "other\.sqlite\.bz2"]
DBS_PATTERNS_SIMPLE_XZ = ["primary\.sqlite\.xz", "filelists\.sqlite\.xz", "other\.sqlite\.xz"]


class TestCaseSqliterepo_badparams(BaseTestCase):
    """Use case with bad commandline arguments"""

    def setup(self):
        self.indir_addpkg(PACKAGES[0])


class TestCaseSqliterepo(BaseTestCase):
    """Base use cases"""

    def setup(self):
        self.indir_addpkg(PACKAGES[0])

    def test_01_sqliterepo(self):
        """Sqlitedbs already exists, sqliterepo without --force should fail"""
        cr_res = self.run_cr(self.indir, c=True)
        sq_res = self.run_sqlr(cr_res.outdir)
        self.assertTrue(sq_res.rc)
        self.assertTrue("already has sqlitedb present" in sq_res.out)

    def test_02_sqliterepo(self):
        """Sqlitedbs should be created"""
        outdir = self.tdir_makedirs("repository")
        self.assert_run_cr(self.indir, args="--no-database", c=True, outdir=outdir)
        self.assert_run_sqlr(outdir)
        # Check that DBs really exists
        self.assert_repo_files(outdir, DBS_PATTERNS, additional_files_allowed=True)

    def test_03_sqliterepo(self):
        """Sqlitedbs with simple md filenames should be created"""
        outdir = self.tdir_makedirs("repository")
        self.assert_run_cr(self.indir, args="--no-database --simple-md-filenames", c=True, outdir=outdir)
        self.assert_run_sqlr(outdir)
        # Check that DBs really exists
        self.assert_repo_files(outdir, DBS_PATTERNS_SIMPLE, additional_files_allowed=True)

    def test_04_sqliterepo(self):
        """Sqlitedbs with unique md filenames should be created"""
        outdir = self.tdir_makedirs("repository")
        self.assert_run_cr(self.indir, args="--no-database --unique-md-filenames", c=True, outdir=outdir)
        self.assert_run_sqlr(outdir)
        # Check that DBs really exists
        self.assert_repo_files(outdir, DBS_PATTERNS_UNIQUE, additional_files_allowed=True)

    def test_05_sqliterepo(self):
        """--xz is used (old bz2 DBs should be removed)"""
        outdir = self.tdir_makedirs("repository")
        self.assert_run_cr(self.indir, args="--database --simple-md-filenames", c=True, outdir=outdir)
        self.assert_run_sqlr(outdir, args="--force --xz")

        # Check that DBs really exists
        self.assert_repo_files(outdir,
                               DBS_PATTERNS_SIMPLE_XZ+BASE_XML_PATTERNS_SIMPLE,
                               additional_files_allowed=False)

    def test_06_sqliterepo(self):
        """DBs already exists but --force is used sqlitedbs should be created"""
        outdir = self.tdir_makedirs("repository")
        self.assert_run_cr(self.indir, args="--database --simple-md-filenames", c=True, outdir=outdir)
        old_primary_ts = os.path.getmtime(os.path.join(outdir, "repodata", "primary.sqlite.bz2"))

        self.assert_run_sqlr(outdir, args="--force")
        new_primary_ts = os.path.getmtime(os.path.join(outdir, "repodata", "primary.sqlite.bz2"))

        # Check that DBs really exists
        self.assert_repo_files(outdir, DBS_PATTERNS, additional_files_allowed=True)

        # Check that DBs are newer than the old ones
        self.assertTrue(old_primary_ts < new_primary_ts)

    def test_07_sqliterepo(self):
        """--force and --keep-old and --xz are used (old bz2 DBs should be keeped)"""
        outdir = self.tdir_makedirs("repository")
        self.assert_run_cr(self.indir, args="--database --simple-md-filenames", c=True, outdir=outdir)
        self.assert_run_sqlr(outdir, args="--force --keep-old --xz")

        # Check that DBs really exists
        self.assert_repo_files(outdir, DBS_PATTERNS_SIMPLE_BZ2, additional_files_allowed=True)
        self.assert_repo_files(outdir, DBS_PATTERNS_SIMPLE_XZ, additional_files_allowed=True)

    def test_08_sqliterepo(self):
        """--local-sqlite is used and old DBs exists (--force is also used)"""
        outdir = self.tdir_makedirs("repository")
        self.assert_run_cr(self.indir, args="--database --simple-md-filenames", c=True, outdir=outdir)
        old_primary_ts = os.path.getmtime(os.path.join(outdir, "repodata", "primary.sqlite.bz2"))

        self.assert_run_sqlr(outdir, args="--local-sqlite --force")
        new_primary_ts = os.path.getmtime(os.path.join(outdir, "repodata", "primary.sqlite.bz2"))

        # Check that DBs really exists
        expected_files = DBS_PATTERNS_SIMPLE_BZ2 + BASE_XML_PATTERNS_SIMPLE
        self.assert_repo_files(outdir, expected_files, additional_files_allowed=False)

        # Check that DBs are newer than the old ones
        self.assertTrue(old_primary_ts < new_primary_ts)

    def test_09_sqliterepo(self):
        """--local-sqlite is used with --xz and --force and --keep-old"""
        outdir = self.tdir_makedirs("repository")
        self.assert_run_cr(self.indir, args="--database --simple-md-filenames", c=True, outdir=outdir)
        self.assert_run_sqlr(outdir, args="--local-sqlite --force --keep-old --xz")

        # Check that DBs really exists
        self.assert_repo_files(outdir, DBS_PATTERNS_SIMPLE_BZ2, additional_files_allowed=True)
        self.assert_repo_files(outdir, DBS_PATTERNS_SIMPLE_XZ, additional_files_allowed=True)

    def test_10_sqliterepo(self):
        """--compress-type used"""
        outdir = self.tdir_makedirs("repository")
        self.assert_run_cr(self.indir, args="--database --simple-md-filenames", c=True, outdir=outdir)
        self.assert_run_sqlr(outdir, args="--compress-type gz --force")

        # Check that DBs really exists
        expected_files = DBS_PATTERNS_SIMPLE_GZ + BASE_XML_PATTERNS_SIMPLE
        self.assert_repo_files(outdir, expected_files, additional_files_allowed=False)

    def test_11_sqliterepo(self):
        """--checksum used"""
        outdir = self.tdir_makedirs("repository")
        self.assert_run_cr(self.indir, args="--database --unique-md-filenames", c=True, outdir=outdir)
        self.assert_run_sqlr(outdir, args="--checksum md5 --force")

        # Check that DBs really exists
        expected_files = DBS_PATTERNS_UNIQUE_MD5 + BASE_XML_PATTERNS_UNIQUE
        self.assert_repo_files(outdir, expected_files, additional_files_allowed=False)