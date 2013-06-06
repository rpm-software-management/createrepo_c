import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from fixtures import *

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

        self.assertEqual(cs.size, 2686)
        self.assertEqual(cs.checksum_type, cr.SHA256)
        self.assertEqual(cs.checksum, "05509ef37239eb123d41c077626b788352ef94"\
                                      "5200231a901802ef59e565978f")

