import unittest
import createrepo_c as cr

from .fixtures import *

def list_has_str_ending_with(l, s):
    for e in l:
        if e.endswith(s):
            return True
    return False


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

        if os.environ.get("WITH_LIBMODULEMD", "ON").upper() != "OFF":
            ml = cr.MetadataLocation(REPO_WITH_ADDITIONAL_METADATA, 0)
            self.assertTrue(ml)
            self.assertTrue(ml["primary"].endswith("/repodata/490a2a494a3827b8a356f728ac36bc02fb009b0eaea173c890e727bb54219037-primary.xml.gz"))
            self.assertTrue(ml["filelists"].endswith("/repodata/ba5a4fdbb20e7b9b70d9a9abd974bcab1065b1e81d711f80e06ad8cae30c4183-filelists.xml.gz"))
            self.assertTrue(ml["other"].endswith("/repodata/fd458a424a3f3e0dadc95b806674b79055c24e73637e47ad5a6e57926aa1b9d1-other.xml.gz"))
            self.assertTrue(ml["primary_db"].endswith("/repodata/1e12239bf5cb07ec73c74482c35e80dabe30dbe2fdd57bd9e557d987cbacc8c2-primary.sqlite.bz2"))
            self.assertTrue(ml["filelists_db"].endswith("/repodata/4f4de7d3254a033b84626f330bc6adb8a3c1a4a20f0ddbe30a5692a041318c81-filelists.sqlite.bz2"))
            self.assertTrue(ml["other_db"].endswith("/repodata/8b13cba732c1a02b841f43d6791ca68788d45f376787d9f3ccf68e75f01af499-other.sqlite.bz2"))
            self.assertTrue(ml["group"].endswith("/repodata/04460bfaf6cb5af6b0925d8c99401a44e5192d287796aed4cced5f7ce881761f-comps.f20.xml"))
            self.assertTrue(ml["group_gz"].endswith("/repodata/f9d860ddcb64fbdc88a9b71a14ddb9f5670968d5dd3430412565c13d42b6804d-comps.f20.xml.gz"))
            self.assertTrue(ml["updateinfo"].endswith("/repodata/88514679cb03d8f51e850ad3639c089f899e83407a2380ef9e62873a8eb1db13-updateinfo_01.xml.gz"))
            additional_metadata = ml["additional_metadata"] 
            self.assertTrue(len(additional_metadata) == 8)
            self.assertTrue(list_has_str_ending_with(additional_metadata, "4fbad65c641f4f8fb3cec9b1672fcec2357443e1ea6e93541a0bb559c7dc9238-modules.yaml.gz"))
            self.assertTrue(list_has_str_ending_with(additional_metadata, "cb0f4b5df8268f248158e50d66ee1565591bca23ee2dbd84ae9c457962fa3122-modules.yaml.gz.zck"))
            self.assertTrue(list_has_str_ending_with(additional_metadata, "04460bfaf6cb5af6b0925d8c99401a44e5192d287796aed4cced5f7ce881761f-comps.f20.xml"))
            self.assertTrue(list_has_str_ending_with(additional_metadata, "2bbdf70c4394e71c2d3905c143d460009d04359de5a90b72b47cdb9dbdcc079d-comps.f20.xml.zck"))
            self.assertTrue(list_has_str_ending_with(additional_metadata, "2bbdf70c4394e71c2d3905c143d460009d04359de5a90b72b47cdb9dbdcc079d-comps.f20.xml.gz.zck"))
            self.assertTrue(list_has_str_ending_with(additional_metadata, "f9d860ddcb64fbdc88a9b71a14ddb9f5670968d5dd3430412565c13d42b6804d-comps.f20.xml.gz"))
            self.assertTrue(list_has_str_ending_with(additional_metadata, "88514679cb03d8f51e850ad3639c089f899e83407a2380ef9e62873a8eb1db13-updateinfo_01.xml.gz"))
            self.assertTrue(list_has_str_ending_with(additional_metadata, "0219a2f1f9f32af6b7873905269ac1bc27b03e0caf3968c929a49e5a939e8935-updateinfo_01.xml.gz.zck"))
            self.assertTrue(ml["foobarxyz"] is None)
        else:
            with self.assertRaises(Exception):
                ml = cr.MetadataLocation(REPO_WITH_ADDITIONAL_METADATA, 0)

