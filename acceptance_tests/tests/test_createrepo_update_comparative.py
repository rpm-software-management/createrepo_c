import os
import os.path

from fixtures import PACKAGES
from base import BaseTestCase


class TestCaseCreaterepoUpdateComparative_emptyrepo(BaseTestCase):
    """Empty input repository"""

    def test_01_createrepoupdate(self):
        """Repo from empty directory"""
        self.assert_same_results(self.indir)
        self.assert_same_results(self.indir, "--update")

    def test_02_createrepoupdate_double(self):
        """Repo from empty directory"""
        self.assert_same_results(self.indir)
        self.assert_same_results(self.indir, "--update")
        self.assert_same_results(self.indir, "--update")

    def test_03_createrepoupdate_relativepath(self):
        """Repo from empty directory - specified by relative path"""
        self.assert_same_results(os.path.relpath(self.indir))
        self.assert_same_results(os.path.relpath(self.indir), "--update")

    def test_04_createrepoupdate_simplemdfilenames(self):
        """Repo from empty directory - specified by relative path"""
        self.assert_same_results(os.path.relpath(self.indir))
        self.assert_same_results(os.path.relpath(self.indir), "--update --simple-md-filenames")


class TestCaseCreaterepoUpdateComparative_regularrepo(BaseTestCase):
    """Repo with 3 packages"""

    def setup(self):
        self.indir_addpkg(PACKAGES[0])
        self.indir_addpkg(PACKAGES[1])
        self.indir_addpkg(PACKAGES[2])

    def test_01_createrepoupdate(self):
        """Repo from empty directory"""
        self.assert_same_results(self.indir)
        self.assert_same_results(self.indir, "--update")

    def test_02_createrepoupdate_double(self):
        """Repo from empty directory"""
        self.assert_same_results(self.indir)
        self.assert_same_results(self.indir, "--update")
        self.assert_same_results(self.indir, "--update")

    def test_03_createrepoupdate_relativepath(self):
        """Repo from empty directory - specified by relative path"""
        self.assert_same_results(os.path.relpath(self.indir))
        self.assert_same_results(os.path.relpath(self.indir), "--update")

    def test_04_createrepoupdate_simplemdfilenames(self):
        """Repo from empty directory - specified by relative path"""
        self.assert_same_results(os.path.relpath(self.indir))
        self.assert_same_results(os.path.relpath(self.indir), "--update --simple-md-filenames")


class TestCaseCreaterepoUpdateComparative_regularrepoandupdaterepo(BaseTestCase):
    """Repo with 3 packages and repo that will be used as --update-md-path"""

    def setup(self):
        self.indir_addpkg(PACKAGES[0])
        self.indir_addpkg(PACKAGES[1])
        self.indir_addpkg(PACKAGES[2])
        res = self.assert_run_cr(self.indir)
        self.update_md_path = os.path.join(self.tdir, "update_md_path")
        os.rename(res.outdir, self.update_md_path)
        os.rename(res.logfile, os.path.join(self.tdir, "out_createrepo-update_md_path"))

    def test_01_createrepoupdate_updatemdpath(self):
        """Repo from empty directory"""
        self.assert_same_results(self.indir, "--update --update-md-path %s" % self.update_md_path)

    def test_02_createrepoupdate_updatemdpathandupdate(self):
        """Repo from empty directory"""
        self.assert_same_results(self.indir, "--update --update-md-path %s" % self.update_md_path)
        self.assert_same_results(self.indir, "--update")

    def test_03_createrepoupdate_updatemdpathdouble(self):
        """Repo from empty directory"""
        self.assert_same_results(self.indir, "--update --update-md-path %s" % self.update_md_path)
        self.assert_same_results(self.indir, "--update --update-md-path %s" % self.update_md_path)