import os
import os.path

from fixtures import PACKAGES
from base import BaseTestCase


class TestCaseCreaterepo_badparams(BaseTestCase):
    """Repo with 1 package"""

    def setup(self):
        self.copy_pkg(PACKAGES[0], self.indir)

    def test_01_createrepo_noinputdir(self):
        """No directory to index was specified"""
        res = self.run_cr("", c=True)
        self.assertTrue(res.rc)

    def test_02_createrepo_badinputdir(self):
        """Directory specified to index doesn't exist"""
        res = self.run_cr("somenonexistingdirectorytoindex/", c=True)
        self.assertTrue(res.rc)

    def test_03_createrepo_unknownparam(self):
        """Unknown param is specified"""
        res = self.run_cr(self.indir, "--someunknownparam", c=True)
        self.assertTrue(res.rc)

    def test_04_createrepo_badchecksumtype(self):
        """Unknown checksum type is specified"""
        res = self.run_cr(self.indir, "--checksum foobarunknownchecksum", c=True)
        self.assertTrue(res.rc)

    def test_05_createrepo_badcompressiontype(self):
        """Unknown compressin type is specified"""
        res = self.run_cr(self.indir, "--compress-type foobarunknowncompression", c=True)
        self.assertTrue(res.rc)

    def test_06_createrepo_badgroupfile(self):
        """Bad groupfile file specified"""
        res = self.run_cr(self.indir, "--groupfile badgroupfile", c=True)
        self.assertTrue(res.rc)

    def test_07_createrepo_badpkglist(self):
        """Bad pkglist file specified"""
        res = self.run_cr(self.indir, "--pkglist badpkglist", c=True)
        self.assertTrue(res.rc)

    def test_08_createrepo_retainoldmdbyagetogetherwithretainoldmd(self):
        """Both --retain-old-md-by-age and --retain-old-md are specified"""
        res = self.run_cr(self.indir, "--retain-old-md-by-age 1 --retain-old-md", c=True)
        self.assertTrue(res.rc)

    def test_09_createrepo_retainoldmdbyagewithbadage(self):
        """Both --retain-old-md-by-age and --retain-old-md are specified"""
        res = self.run_cr(self.indir, "--retain-old-md-by-age 55Z", c=True)
        self.assertTrue(res.rc)


class TestCaseCreaterepo_emptyrepo(BaseTestCase):
    """Empty input repository"""

    def test_01_createrepo(self):
        """Repo from empty directory"""
        res = self.assert_run_cr(self.indir, c=True)
        self.assert_repo_sanity(res.outdir)