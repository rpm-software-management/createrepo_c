import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from .fixtures import *

class TestCaseUpdateCollectionPackage(unittest.TestCase):

    def test_updatecollectionpackage_setters(self):
        pkg = cr.UpdateCollectionPackage()
        self.assertTrue(pkg)

        self.assertEqual(pkg.name, None)
        self.assertEqual(pkg.version, None)
        self.assertEqual(pkg.release, None)
        self.assertEqual(pkg.epoch, None)
        self.assertEqual(pkg.arch, None)
        self.assertEqual(pkg.src, None)
        self.assertEqual(pkg.filename, None)
        self.assertEqual(pkg.sum, None)
        self.assertEqual(pkg.sum_type, 0)
        self.assertEqual(pkg.reboot_suggested, 0)
        self.assertEqual(pkg.restart_suggested, 0)
        self.assertEqual(pkg.relogin_suggested, 0)

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
        pkg.restart_suggested = True
        pkg.relogin_suggested = True

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
        self.assertEqual(pkg.restart_suggested, True)
        self.assertEqual(pkg.relogin_suggested, True)
