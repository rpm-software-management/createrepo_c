import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from .fixtures import *

class TestCaseUpdateCollectionModule(unittest.TestCase):

    def test_updatecollectionmodule_setters(self):
        module = cr.UpdateCollectionModule()
        self.assertTrue(module)

        self.assertEqual(module.name, None)
        self.assertEqual(module.stream, None)
        self.assertEqual(module.version, 0)
        self.assertEqual(module.context, None)
        self.assertEqual(module.arch, None)

        module.name = "foo"
        module.stream = "0"
        module.version = 20180730223407
        module.context = "deadbeef"
        module.arch = "noarch"

        self.assertEqual(module.name, "foo")
        self.assertEqual(module.stream, "0")
        self.assertEqual(module.version, 20180730223407)
        self.assertEqual(module.context, "deadbeef")
        self.assertEqual(module.arch, "noarch")
