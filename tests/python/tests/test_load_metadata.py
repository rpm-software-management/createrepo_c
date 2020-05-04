import unittest
import createrepo_c as cr

from .fixtures import *

class TestCaseLoadMetadata(unittest.TestCase):
    def test_load_metadata_repo00(self):
        md = cr.Metadata()
        md.locate_and_load_xml(REPO_00_PATH)
        self.assertTrue(md)

        self.assertEqual(md.key, cr.HT_KEY_DEFAULT)

        self.assertEqual(md.len(), 0)
        self.assertEqual(md.keys(), [])
        self.assertFalse(md.has_key("foo"))
        self.assertFalse(md.has_key(""))
        self.assertFalse(md.remove("foo"))
        self.assertFalse(md.get("xxx"))

    def test_load_metadata_repo01(self):
        md = cr.Metadata()
        md.locate_and_load_xml(REPO_01_PATH)
        self.assertTrue(md)

        self.assertEqual(md.key, cr.HT_KEY_DEFAULT)

        self.assertEqual(md.len(), 1)
        self.assertEqual(md.keys(), ['152824bff2aa6d54f429d43e87a3ff3a0286505c6d93ec87692b5e3a9e3b97bf'])
        self.assertFalse(md.has_key("foo"))
        self.assertFalse(md.has_key(""))
        self.assertFalse(md.remove("foo"))

        pkg = md.get('152824bff2aa6d54f429d43e87a3ff3a0286505c6d93ec87692b5e3a9e3b97bf')
        self.assertTrue(pkg)

        self.assertEqual(pkg.name, "super_kernel")

    def test_load_metadata_repo02(self):
        md = cr.Metadata()
        md.locate_and_load_xml(REPO_02_PATH)
        self.assertTrue(md)

        self.assertEqual(md.key, cr.HT_KEY_DEFAULT)

        self.assertEqual(md.len(), 2)
        self.assertEqual(md.keys(),
            ['6d43a638af70ef899933b1fd86a866f18f65b0e0e17dcbf2e42bfd0cdd7c63c3',
             '90f61e546938a11449b710160ad294618a5bd3062e46f8cf851fd0088af184b7'])
        self.assertFalse(md.has_key("foo"))
        self.assertFalse(md.has_key(""))
        self.assertFalse(md.remove("foo"))

        pkg = md.get('152824bff2aa6d54f429d43e87a3ff3a0286505c6d93ec87692b5e3a9e3b97bf')
        self.assertEqual(pkg, None)

        pkg = md.get('90f61e546938a11449b710160ad294618a5bd3062e46f8cf851fd0088af184b7')
        self.assertEqual(pkg.name, "fake_bash")

    def test_load_metadata_repo02_destructor(self):
        md = cr.Metadata(use_single_chunk=True)
        md.locate_and_load_xml(REPO_02_PATH)
        pkg = md.get('90f61e546938a11449b710160ad294618a5bd3062e46f8cf851fd0088af184b7')
        del(md)  # in fact, md should not be destroyed yet, because it is
                 # referenced from pkg!
        self.assertEqual(pkg.name, "fake_bash")
