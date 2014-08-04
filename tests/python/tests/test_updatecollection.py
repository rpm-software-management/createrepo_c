import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from fixtures import *

class TestCaseUpdateCollection(unittest.TestCase):

    def test_updatecollection_setters(self):
        col = cr.UpdateCollection()
        self.assertTrue(col)

        self.assertEqual(col.shortname, None)
        self.assertEqual(col.name, None)
        self.assertEqual(col.packages, [])

        pkg = cr.UpdateCollectionPackage()
        pkg.name = "foo"
        pkg.version = "1.2"
        pkg.release = "3"
        pkg.epoch = "0"
        pkg.arch = "x86"
        pkg.src = "foo.src.rpm"
        pkg.filename = "foo.rpm"
        pkg.sum = "abcdef"
        pkg.sum_type = cr.SHA1
        pkg.reboot_suggested = True

        col.shortname = "short name"
        col.name = "long name"
        col.append(pkg)

        self.assertEqual(col.shortname, "short name")
        self.assertEqual(col.name, "long name")
        self.assertEqual(len(col.packages), 1)

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
        self.assertEqual(pkg.sum_type, cr.SHA1)
        self.assertEqual(pkg.reboot_suggested, True)
