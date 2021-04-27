import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from .fixtures import *

class TestCaseUpdateCollection(unittest.TestCase):

    def test_updatecollection_setters(self):
        col = cr.UpdateCollection()
        self.assertTrue(col)

        self.assertEqual(col.shortname, None)
        self.assertEqual(col.name, None)
        self.assertEqual(col.packages, [])

        module = cr.UpdateCollectionModule()
        module.name = "kangaroo"
        module.stream = "0"
        module.version = 20180730223407
        module.context = "deadbeef"
        module.arch = "noarch"

        pkg = cr.UpdateCollectionPackage()
        pkg.name = "foo"
        pkg.version = "1.2"
        pkg.release = "3"
        pkg.epoch = "0"
        pkg.arch = "x86"
        pkg.src = "foo.src.rpm"
        pkg.filename = "foo.rpm"
        pkg.sum = "abcdef"
        pkg.sum_type = cr.SHA256
        pkg.reboot_suggested = True

        col.shortname = "short name"
        col.name = "long name"
        col.module = module
        col.append(pkg)

        self.assertEqual(col.shortname, "short name")
        self.assertEqual(col.name, "long name")
        self.assertEqual(len(col.packages), 1)

        # Check if the appended module was appended properly
        module = col.module
        self.assertEqual(module.name, "kangaroo")
        self.assertEqual(module.stream, "0")
        self.assertEqual(module.version, 20180730223407)
        self.assertEqual(module.context, "deadbeef")
        self.assertEqual(module.arch, "noarch")

        # Also check if the appended package was appended properly
        pkg = col.packages[0]
        self.assertEqual(pkg.name, "foo")
        self.assertEqual(pkg.version, "1.2")
        self.assertEqual(pkg.release, "3")
        self.assertEqual(pkg.epoch, "0")
        self.assertEqual(pkg.arch, "x86")
        self.assertEqual(pkg.src, "foo.src.rpm")
        self.assertEqual(pkg.filename, "foo.rpm")
        self.assertEqual(pkg.sum, "abcdef")
        self.assertEqual(pkg.sum_type, cr.SHA256)
        self.assertEqual(pkg.reboot_suggested, True)

    def test_updatecollection_setters_when_module_going_out_of_scope(self):
        def create_collection_scope():
            col = cr.UpdateCollection()
            col.name = "name"

            module = cr.UpdateCollectionModule()
            module.name = "kangaroo"
            module.stream = "0"
            module.version = 20180730223407
            module.context = "deadbeef"
            module.arch = "noarch"

            col.module = module

            return col

        col = create_collection_scope()
        self.assertTrue(col)

        self.assertEqual(col.name, "name")

        # Check if the appended module was appended properly
        module = col.module
        self.assertEqual(module.name, "kangaroo")
        self.assertEqual(module.stream, "0")
        self.assertEqual(module.version, 20180730223407)
        self.assertEqual(module.context, "deadbeef")
        self.assertEqual(module.arch, "noarch")

