import unittest
import createrepo_c as cr

from .fixtures import *

class TestCaseMetadataLocation(unittest.TestCase):
    def test_metadatalocation(self):
        ml = cr.MetadataLocation(REPO_00_PATH, 1)
        self.assertTrue(ml)
        self.assertTrue(ml["primary"].endswith("/repodata/1cb61ea996355add02b1426ed4c1780ea75ce0c04c5d1107c025c3fbd7d8bcae-primary.xml.gz"))
        self.assertTrue(ml["filelists"].endswith("/repodata/95a4415d859d7120efb6b3cf964c07bebbff9a5275ca673e6e74a97bcbfb2a5f-filelists.xml.gz"))
        self.assertTrue(ml["other"].endswith("/repodata/ef3e20691954c3d1318ec3071a982da339f4ed76967ded668b795c9e070aaab6-other.xml.gz"))
        self.assertTrue(ml["primary_db"] is None)
        self.assertTrue(ml["filelists_db"] is None)
        self.assertTrue(ml["other_db"] is None)
        self.assertTrue(ml["group"] is None)
        self.assertTrue(ml["group_gz"] is None)
        self.assertTrue(ml["updateinfo"] is None)
        self.assertTrue(ml["foobarxyz"] is None)
