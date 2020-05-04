import unittest
import createrepo_c as cr

from .fixtures import *

class TestCaseParsepkg(unittest.TestCase):
    def test_package_from_rpm(self):
        pkg = cr.package_from_rpm(PKG_ARCHER_PATH)
        self.assertTrue(pkg)
        self.assertEqual(pkg.name, "Archer")

        pkg = cr.package_from_rpm(PKG_BALICEK_ISO88591_PATH)
        self.assertTrue(pkg)
        self.assertEqual(pkg.name, "balicek-iso88591")

        pkg = cr.package_from_rpm(PKG_BALICEK_ISO88592_PATH)
        self.assertTrue(pkg)
        self.assertEqual(pkg.name, "balicek-iso88592")

        pkg = cr.package_from_rpm(PKG_BALICEK_UTF8_PATH)
        self.assertTrue(pkg)
        self.assertEqual(pkg.name, "balicek-utf8")

        pkg = cr.package_from_rpm(PKG_EMPTY_PATH)
        self.assertTrue(pkg)
        self.assertEqual(pkg.name, "empty")

        pkg = cr.package_from_rpm(PKG_EMPTY_SRC_PATH)
        self.assertTrue(pkg)
        self.assertEqual(pkg.name, "empty")

        pkg = cr.package_from_rpm(PKG_FAKE_BASH_PATH)
        self.assertTrue(pkg)
        self.assertEqual(pkg.name, "fake_bash")

        pkg = cr.package_from_rpm(PKG_SUPER_KERNEL_PATH)
        self.assertTrue(pkg)
        self.assertEqual(pkg.name, "super_kernel")

        # Test error cases

        # Rpm doesn't exists
        self.assertRaises(IOError, cr.package_from_rpm, "this_foo_pkg_should_not_exists.rpm")

        # Path is a directory, not a file
        self.assertRaises(IOError, cr.package_from_rpm, "./")

        # File is not a rpm
        self.assertRaises(IOError, cr.package_from_rpm, FILE_BINARY_PATH)

    def test_xml_from_rpm(self):
        xml = cr.xml_from_rpm(PKG_ARCHER_PATH)
        self.assertTrue(xml)
        self.assertTrue(len(xml) == 3)
        self.assertTrue("<name>Archer</name>" in xml[0])
        self.assertTrue('<package pkgid="4e0b775220c67f0f2c1fd2177e626b9c863a098130224ff09778ede25cea9a9e" name="Archer" arch="x86_64">' in xml[1])
        self.assertTrue('<package pkgid="4e0b775220c67f0f2c1fd2177e626b9c863a098130224ff09778ede25cea9a9e" name="Archer" arch="x86_64">' in xml[2])

        # Test error cases

        # Rpm doesn't exists
        self.assertRaises(IOError, cr.xml_from_rpm, "this_foo_pkg_should_not_exists.rpm")

        # Path is a directory, not a file
        self.assertRaises(IOError, cr.xml_from_rpm, "./")

        # File is not a rpm
        self.assertRaises(IOError, cr.xml_from_rpm, FILE_BINARY_PATH)
