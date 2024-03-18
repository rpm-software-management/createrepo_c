import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from .fixtures import *

class TestCaseContentStat(unittest.TestCase):

    # TODO:
    #  - Test rename_file() for files with checksum

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="createrepo_ctest-")

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_contentstat(self):
        pkg = cr.package_from_rpm(PKG_ARCHER_PATH)
        self.assertTrue(pkg)

        pkg.time_file = 1
        pkg.time_build = 1

        cs = cr.ContentStat(cr.SHA256)
        self.assertEqual(cs.size, 0)
        self.assertEqual(cs.checksum_type, cr.SHA256)
        self.assertEqual(cs.checksum, None)

        path = os.path.join(self.tmpdir, "primary.xml.gz")
        f = cr.PrimaryXmlFile(path, cr.GZ_COMPRESSION, cs)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.add_pkg(pkg)
        f.close()

        self.assertTrue(os.path.isfile(path))

        self.assertEqual(cs.size, 2805)
        self.assertEqual(cs.checksum_type, cr.SHA256)
        self.assertEqual(cs.checksum, "d447983b39363a6519067ce477a7fc64409b4"\
                                      "900e0160da68d66b25207a0408d")

    def test_contentstat_ref_in_xmlfile(self):
        """Test if reference is saved properly"""

        pkg = cr.package_from_rpm(PKG_ARCHER_PATH)
        self.assertTrue(pkg)

        pkg.time_file = 1
        pkg.time_build = 1

        cs = cr.ContentStat(cr.SHA256)
        self.assertEqual(cs.size, 0)
        self.assertEqual(cs.checksum_type, cr.SHA256)
        self.assertEqual(cs.checksum, None)

        path = os.path.join(self.tmpdir, "primary.xml.gz")
        f = cr.PrimaryXmlFile(path, cr.GZ_COMPRESSION, cs)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        del cs
        f.add_pkg(pkg)
        f.close()

        self.assertTrue(os.path.isfile(path))

    def test_contentstat_ref_in_crfile(self):
        """Test if reference is saved properly"""

        cs = cr.ContentStat(cr.SHA256)
        self.assertEqual(cs.size, 0)
        self.assertEqual(cs.checksum_type, cr.SHA256)
        self.assertEqual(cs.checksum, None)

        path = os.path.join(self.tmpdir, "foofile.gz")
        f = cr.CrFile(path, cr.MODE_WRITE, cr.GZ_COMPRESSION, cs)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        del cs
        f.write("foobar")
        f.close()

        self.assertTrue(os.path.isfile(path))
