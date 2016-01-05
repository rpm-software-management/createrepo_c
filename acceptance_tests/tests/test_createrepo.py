import os
import os.path

from .fixtures import PACKAGES
from .base import BaseTestCase


class TestCaseCreaterepo_badparams(BaseTestCase):
    """Use case with bad commandline arguments"""

    def setup(self):
        self.indir_addpkg(PACKAGES[0])

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

    def setup(self):
        self.fn_comps = self.indir_mkfile("comps.xml", '<?xml version="1.0"?><comps/>')

    def test_01_createrepo(self):
        """Repo from empty directory"""
        res = self.assert_run_cr(self.indir, c=True)
        self.assert_repo_sanity(res.outdir)
        self.assert_repo_files(res.outdir,
                               ["repomd.xml$",
                                "[a-z0-9]{64}-primary.xml.gz$",
                                "[a-z0-9]{64}-filelists.xml.gz$",
                                "[a-z0-9]{64}-other.xml.gz$",
                                "[a-z0-9]{64}-primary.sqlite.bz2$",
                                "[a-z0-9]{64}-filelists.sqlite.bz2$",
                                "[a-z0-9]{64}-other.sqlite.bz2$",
                                ],
                               additional_files_allowed=False)

    def test_02_createrepo_database(self):
        """--database"""
        res = self.assert_run_cr(self.indir, "--database", c=True)
        self.assert_repo_sanity(res.outdir)
        self.assert_repo_files(res.outdir,
                               ["repomd.xml$",
                                "[a-z0-9]+-primary.xml.gz$",
                                "[a-z0-9]+-filelists.xml.gz$",
                                "[a-z0-9]+-other.xml.gz$",
                                "[a-z0-9]+-primary.sqlite.bz2$",
                                "[a-z0-9]+-filelists.sqlite.bz2$",
                                "[a-z0-9]+-other.sqlite.bz2$",
                                ],
                               additional_files_allowed=False)

    def test_03_createrepo_nodatabase(self):
        """--database"""
        res = self.assert_run_cr(self.indir, "--no-database", c=True)
        self.assert_repo_sanity(res.outdir)
        self.assert_repo_files(res.outdir,
                               ["repomd.xml$",
                                "[a-z0-9]+-primary.xml.gz$",
                                "[a-z0-9]+-filelists.xml.gz$",
                                "[a-z0-9]+-other.xml.gz$",
                                ],
                               additional_files_allowed=False)

    def test_04_createrepo_groupfile(self):
        """--groupfile"""
        res = self.assert_run_cr(self.indir, "--groupfile %s" % self.fn_comps, c=True)
        self.assert_repo_sanity(res.outdir)
        self.assert_repo_files(res.outdir,
                               ["repomd.xml$",
                                "[a-z0-9]+-primary.xml.gz$",
                                "[a-z0-9]+-filelists.xml.gz$",
                                "[a-z0-9]+-other.xml.gz$",
                                "[a-z0-9]+-primary.sqlite.bz2$",
                                "[a-z0-9]+-filelists.sqlite.bz2$",
                                "[a-z0-9]+-other.sqlite.bz2$",
                                "[a-z0-9]+-comps.xml$",
                                "[a-z0-9]+-comps.xml.gz$",
                                ],
                               additional_files_allowed=False)

    def test_05_createrepo_checksum(self):
        """--checksum sha and --groupfile"""
        res = self.assert_run_cr(self.indir,
                                 "--checksum %(checksum)s --groupfile %(groupfile)s" % {
                                    'checksum': "sha1",
                                    'groupfile': self.fn_comps },
                                 c=True)
        self.assert_repo_sanity(res.outdir)
        self.assert_repo_files(res.outdir,
                               ["repomd.xml$",
                                "[a-z0-9]{40}-primary.xml.gz$",
                                "[a-z0-9]{40}-filelists.xml.gz$",
                                "[a-z0-9]{40}-other.xml.gz$",
                                "[a-z0-9]{40}-primary.sqlite.bz2$",
                                "[a-z0-9]{40}-filelists.sqlite.bz2$",
                                "[a-z0-9]{40}-other.sqlite.bz2$",
                                "[a-z0-9]{40}-comps.xml$",
                                "[a-z0-9]{40}-comps.xml.gz$",
                                ],
                               additional_files_allowed=False)

    def test_06_createrepo_simplemdfilenames(self):
        """--simple-md-filenames and --groupfile"""
        res = self.assert_run_cr(self.indir,
                                 "--simple-md-filenames --groupfile %(groupfile)s" % {
                                    'groupfile': self.fn_comps },
                                 c=True)
        self.assert_repo_sanity(res.outdir)
        self.assert_repo_files(res.outdir,
                               ["repomd.xml$",
                                "primary.xml.gz$",
                                "filelists.xml.gz$",
                                "other.xml.gz$",
                                "primary.sqlite.bz2$",
                                "filelists.sqlite.bz2$",
                                "other.sqlite.bz2$",
                                "comps.xml$",
                                "comps.xml.gz$",
                                ],
                               additional_files_allowed=False)

    def test_07_createrepo_xz(self):
        """--xz and --groupfile"""
        res = self.assert_run_cr(self.indir,
                                 "--xz --groupfile %(groupfile)s" % {
                                    'groupfile': self.fn_comps },
                                 c=True)
        self.assert_repo_sanity(res.outdir)
        self.assert_repo_files(res.outdir,
                               ["repomd.xml$",
                                "[a-z0-9]{64}-primary.xml.gz$",
                                "[a-z0-9]{64}-filelists.xml.gz$",
                                "[a-z0-9]{64}-other.xml.gz$",
                                "[a-z0-9]{64}-primary.sqlite.xz$",
                                "[a-z0-9]{64}-filelists.sqlite.xz$",
                                "[a-z0-9]{64}-other.sqlite.xz$",
                                "[a-z0-9]{64}-comps.xml$",
                                "[a-z0-9]{64}-comps.xml.xz$",
                                ],
                               additional_files_allowed=False)

    def test_08_createrepo_compresstype_bz2(self):
        """--compress-type bz2"""
        res = self.assert_run_cr(self.indir, "--compress-type bz2", c=True)
        self.assert_repo_sanity(res.outdir)
        self.assert_repo_files(res.outdir,
                               ["repomd.xml$",
                                "[a-z0-9]{64}-primary.xml.gz$",
                                "[a-z0-9]{64}-filelists.xml.gz$",
                                "[a-z0-9]{64}-other.xml.gz$",
                                "[a-z0-9]{64}-primary.sqlite.bz2$",
                                "[a-z0-9]{64}-filelists.sqlite.bz2$",
                                "[a-z0-9]{64}-other.sqlite.bz2$",
                                ],
                               additional_files_allowed=False)

    def test_09_createrepo_compresstype_gz(self):
        """--compress-type bz2"""
        res = self.assert_run_cr(self.indir, "--compress-type gz", c=True)
        self.assert_repo_sanity(res.outdir)
        self.assert_repo_files(res.outdir,
                               ["repomd.xml$",
                                "[a-z0-9]{64}-primary.xml.gz$",
                                "[a-z0-9]{64}-filelists.xml.gz$",
                                "[a-z0-9]{64}-other.xml.gz$",
                                "[a-z0-9]{64}-primary.sqlite.gz$",
                                "[a-z0-9]{64}-filelists.sqlite.gz$",
                                "[a-z0-9]{64}-other.sqlite.gz$",
                                ],
                               additional_files_allowed=False)

    def test_10_createrepo_compresstype_xz(self):
        """--compress-type bz2"""
        res = self.assert_run_cr(self.indir, "--compress-type xz", c=True)
        self.assert_repo_sanity(res.outdir)
        self.assert_repo_files(res.outdir,
                               ["repomd.xml$",
                                "[a-z0-9]{64}-primary.xml.gz$",
                                "[a-z0-9]{64}-filelists.xml.gz$",
                                "[a-z0-9]{64}-other.xml.gz$",
                                "[a-z0-9]{64}-primary.sqlite.xz$",
                                "[a-z0-9]{64}-filelists.sqlite.xz$",
                                "[a-z0-9]{64}-other.sqlite.xz$",
                                ],
                               additional_files_allowed=False)

    def test_11_createrepo_repomd_checksum(self):
        """--checksum sha and --groupfile"""
        res = self.assert_run_cr(self.indir,
                                 "--repomd-checksum %(checksum)s --groupfile %(groupfile)s" % {
                                    'checksum': "sha1",
                                    'groupfile': self.fn_comps },
                                 c=True)
        self.assert_repo_sanity(res.outdir)
        self.assert_repo_files(res.outdir,
                               ["repomd.xml$",
                                "[a-z0-9]{40}-primary.xml.gz$",
                                "[a-z0-9]{40}-filelists.xml.gz$",
                                "[a-z0-9]{40}-other.xml.gz$",
                                "[a-z0-9]{40}-primary.sqlite.bz2$",
                                "[a-z0-9]{40}-filelists.sqlite.bz2$",
                                "[a-z0-9]{40}-other.sqlite.bz2$",
                                "[a-z0-9]{40}-comps.xml$",
                                "[a-z0-9]{40}-comps.xml.gz$",
                                ],
                               additional_files_allowed=False)

    def test_12_createrepo_repomd_checksum(self):
        """--checksum sha and --groupfile"""
        res = self.assert_run_cr(self.indir,
                                 "--checksum md5 --repomd-checksum %(checksum)s --groupfile %(groupfile)s" % {
                                    'checksum': "sha1",
                                    'groupfile': self.fn_comps },
                                 c=True)
        self.assert_repo_sanity(res.outdir)
        self.assert_repo_files(res.outdir,
                               ["repomd.xml$",
                                "[a-z0-9]{40}-primary.xml.gz$",
                                "[a-z0-9]{40}-filelists.xml.gz$",
                                "[a-z0-9]{40}-other.xml.gz$",
                                "[a-z0-9]{40}-primary.sqlite.bz2$",
                                "[a-z0-9]{40}-filelists.sqlite.bz2$",
                                "[a-z0-9]{40}-other.sqlite.bz2$",
                                "[a-z0-9]{40}-comps.xml$",
                                "[a-z0-9]{40}-comps.xml.gz$",
                                ],
                               additional_files_allowed=False)

    def test_13_createrepo_general_compress_type(self):
        """--checksum sha and --groupfile"""
        res = self.assert_run_cr(self.indir,
                                 "--general-compress-type %(compress_type)s --groupfile %(groupfile)s" % {
                                    'compress_type': "xz",
                                    'groupfile': self.fn_comps },
                                 c=True)
        self.assert_repo_sanity(res.outdir)
        self.assert_repo_files(res.outdir,
                               ["repomd.xml$",
                                "[a-z0-9]+-primary.xml.xz$",
                                "[a-z0-9]+-filelists.xml.xz$",
                                "[a-z0-9]+-other.xml.xz$",
                                "[a-z0-9]+-primary.sqlite.xz$",
                                "[a-z0-9]+-filelists.sqlite.xz$",
                                "[a-z0-9]+-other.sqlite.xz$",
                                "[a-z0-9]+-comps.xml$",
                                "[a-z0-9]+-comps.xml.xz$",
                                ],
                               additional_files_allowed=False)