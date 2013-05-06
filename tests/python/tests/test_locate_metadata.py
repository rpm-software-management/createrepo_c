import unittest
import createrepo_c as cr

from fixtures import *

class TestCaseMetadataLocation(unittest.TestCase):
    def test_metadatalocation(self):
        ml = cr.MetadataLocation(REPO_00_PATH, 1)
        self.assertTrue(ml)
        self.assertTrue(ml["primary"].endswith("/repodata/dabe2ce5481d23de1f4f52bdcfee0f9af98316c9e0de2ce8123adeefa0dd08b9-primary.xml.gz"))
        self.assertTrue(ml["filelists"].endswith("/repodata/401dc19bda88c82c403423fb835844d64345f7e95f5b9835888189c03834cc93-filelists.xml.gz"))
        self.assertTrue(ml["other"].endswith("/repodata/6bf9672d0862e8ef8b8ff05a2fd0208a922b1f5978e6589d87944c88259cb670-other.xml.gz"))
        self.assertTrue(ml["primary_db"] is None)
        self.assertTrue(ml["filelists_db"] is None)
        self.assertTrue(ml["other_db"] is None)
        self.assertTrue(ml["group"] is None)
        self.assertTrue(ml["group_gz"] is None)
        self.assertTrue(ml["updateinfo"] is None)
        self.assertTrue(ml["foobarxyz"] is None)
