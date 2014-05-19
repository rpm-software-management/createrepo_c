import os
import os.path

from fixtures import PACKAGES
from base import BaseTestCase


class TestCaseCreaterepoComparative_emptyrepo(BaseTestCase):
    """Empty input repository"""

    def test_01_createrepo(self):
        """Repo from empty directory"""
        self.assert_same_results(self.indir)

    def test_02_createrepo_relativepath(self):
        """Repo from empty directory - specified by relative path"""
        self.assert_same_results(os.path.relpath(self.indir))

    def test_03_createrepo_distrotag(self):
        """--distro"""
        self.assert_same_results(self.indir, "--distro DISTRO-TAG")

    def test_04_createrepo_distrotag(self):
        """--distro"""
        self.assert_same_results(self.indir, "--distro CPEID,Footag")

    def test_05_createrepo_distrotag(self):
        """--distro"""
        self.assert_same_results(self.indir, "--distro cpeid,tag_a --distro tag_b")

    def test_06_createrepo_contenttag(self):
        """--content"""
        self.assert_same_results(self.indir, "--content contenttag_a")

    def test_07_createrepo_contenttag(self):
        """--content"""
        self.assert_same_results(self.indir, "--content contenttag_a --content contettag_b")

    def test_08_createrepo_repotag(self):
        """--repo"""
        self.assert_same_results(self.indir, "--repo repotag_a")

    def test_09_createrepo_repotag(self):
        """--repo"""
        self.assert_same_results(self.indir, "--repo repotag_a --repo repotag_b")

    def test_10_createrepo_nodatabase(self):
        """--no-database"""
        self.assert_same_results(self.indir, "--no-database")

    def test_11_createrepo_uniquemdfilenames(self):
        """--unique-md-filenames"""
        self.assert_same_results(self.indir, "--unique-md-filenames")

    def test_12_createrepo_simplemdfilenames(self):
        """--simple-md-filenames"""
        self.assert_same_results(self.indir, "--simple-md-filenames")

class TestCaseCreaterepoComparative_regularrepo(BaseTestCase):
    """Repo with 3 packages"""

    def setup(self):
        self.copy_pkg(PACKAGES[0], self.indir)
        self.copy_pkg(PACKAGES[1], self.indir)
        self.copy_pkg(PACKAGES[2], self.indir)

    def test_01_createrepo(self):
        """Regular createrepo"""
        self.assert_same_results(self.indir)

    def test_02_createrepo_relativepath(self):
        """Regular createrepo"""
        self.assert_same_results(os.path.relpath(self.indir))

    def test_03_createrepo_excludes(self):
        """--excludes * param"""
        self.assert_same_results(self.indir, "--excludes *")