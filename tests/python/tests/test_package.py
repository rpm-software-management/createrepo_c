import unittest
import createrepo_c as cr

from .fixtures import *

class TestCasePackage(unittest.TestCase):
    def test_package_empty(self):
        pkg = cr.package_from_rpm(PKG_EMPTY_PATH)
        self.assertTrue(pkg)
        self.assertEqual(pkg.pkgId, "91afc5e3a124eedfc5bc52737940950b42a37c611dccecad4692a4eb317f9810")
        self.assertEqual(pkg.name, "empty")
        self.assertEqual(pkg.arch, "x86_64")
        self.assertEqual(pkg.version, "0")
        self.assertEqual(pkg.epoch, "0")
        self.assertEqual(pkg.release, "0")
        self.assertEqual(pkg.summary, '""')
        self.assertEqual(pkg.description, None)
        self.assertEqual(pkg.url, None)
        #self.assertEqual(pkg.time_file, 1340709886)
        self.assertEqual(pkg.time_build, 1340696582)
        self.assertEqual(pkg.rpm_license, "LGPL")
        self.assertEqual(pkg.rpm_vendor, None)
        self.assertEqual(pkg.rpm_group, "Unspecified")
        self.assertEqual(pkg.rpm_buildhost, "localhost.localdomain")
        self.assertEqual(pkg.rpm_sourcerpm, "empty-0-0.src.rpm")
        self.assertEqual(pkg.rpm_header_start, 280)
        self.assertEqual(pkg.rpm_header_end, 1285)
        self.assertEqual(pkg.rpm_packager, None)
        self.assertEqual(pkg.size_package, 1401)
        self.assertEqual(pkg.size_installed, 0)
        self.assertEqual(pkg.size_archive, 124)
        self.assertEqual(pkg.location_href, None)
        self.assertEqual(pkg.location_base, None)
        self.assertEqual(pkg.checksum_type, "sha256")
        self.assertEqual(pkg.requires, [])
        self.assertEqual(pkg.provides, [
            ('empty', 'EQ', '0', '0', '0', False),
            ('empty(x86-64)', 'EQ', '0', '0', '0', False)
        ])
        self.assertEqual(pkg.conflicts, [])
        self.assertEqual(pkg.obsoletes, [])
        self.assertEqual(pkg.suggests, [])
        self.assertEqual(pkg.enhances, [])
        self.assertEqual(pkg.recommends, [])
        self.assertEqual(pkg.supplements, [])
        self.assertEqual(pkg.files, [])
        self.assertEqual(pkg.changelogs, [])

        self.assertEqual(pkg.nvra(), "empty-0-0.x86_64")
        self.assertEqual(pkg.nevra(), "empty-0:0-0.x86_64")

    def test_package_archer(self):
        pkg = cr.package_from_rpm(PKG_ARCHER_PATH)
        self.assertTrue(pkg)
        self.assertEqual(pkg.pkgId, "4e0b775220c67f0f2c1fd2177e626b9c863a098130224ff09778ede25cea9a9e")
        self.assertEqual(pkg.name, "Archer")
        self.assertEqual(pkg.arch, "x86_64")
        self.assertEqual(pkg.version, "3.4.5")
        self.assertEqual(pkg.epoch, "2")
        self.assertEqual(pkg.release, "6")
        self.assertEqual(pkg.summary, "Complex package.")
        self.assertEqual(pkg.description, "Archer package")
        self.assertEqual(pkg.url, "http://soo_complex_package.eu/")
        #self.assertEqual(pkg.time_file, 1365416502)
        self.assertEqual(pkg.time_build, 1365416480)
        self.assertEqual(pkg.rpm_license, "GPL")
        self.assertEqual(pkg.rpm_vendor, "ISIS")
        self.assertEqual(pkg.rpm_group, "Development/Tools")
        self.assertEqual(pkg.rpm_buildhost, "localhost.localdomain")
        self.assertEqual(pkg.rpm_sourcerpm, "Archer-3.4.5-6.src.rpm")
        self.assertEqual(pkg.rpm_header_start, 280)
        self.assertEqual(pkg.rpm_header_end, 2865)
        self.assertEqual(pkg.rpm_packager, "Sterling Archer")
        self.assertEqual(pkg.size_package, 3101)
        self.assertEqual(pkg.size_installed, 0)
        self.assertEqual(pkg.size_archive, 544)
        self.assertEqual(pkg.location_href, None)
        self.assertEqual(pkg.location_base, None)
        self.assertEqual(pkg.checksum_type, "sha256")
        self.assertEqual(pkg.requires, [
            ('fooa', 'LE', '0', '2', None, False),
            ('foob', 'GE', '0', '1.0.0', '1', False),
            ('fooc', 'EQ', '0', '3', None, False),
            ('food', 'LT', '0', '4', None, False),
            ('fooe', 'GT', '0', '5', None, False),
            ('foof', 'EQ', '0', '6', None, True)
            ])
        self.assertEqual(pkg.provides, [
            ('bara', 'LE', '0', '22', None, False),
            ('barb', 'GE', '0', '11.22.33', '44', False),
            ('barc', 'EQ', '0', '33', None, False),
            ('bard', 'LT', '0', '44', None, False),
            ('bare', 'GT', '0', '55', None, False),
            ('Archer', 'EQ', '2', '3.4.5', '6', False),
            ('Archer(x86-64)', 'EQ', '2', '3.4.5', '6', False)
            ])
        self.assertEqual(pkg.conflicts, [
            ('bba', 'LE', '0', '2222', None, False),
            ('bbb', 'GE', '0', '1111.2222.3333', '4444', False),
            ('bbc', 'EQ', '0', '3333', None, False),
            ('bbd', 'LT', '0', '4444', None, False),
            ('bbe', 'GT', '0', '5555', None, False)
            ])
        self.assertEqual(pkg.obsoletes, [
            ('aaa', 'LE', '0', '222', None, False),
            ('aab', 'GE', '0', '111.2.3', '4', False),
            ('aac', 'EQ', '0', '333', None, False),
            ('aad', 'LT', '0', '444', None, False),
            ('aae', 'GT', '0', '555', None, False)
            ])
        self.assertEqual(pkg.suggests, [])
        self.assertEqual(pkg.enhances, [])
        self.assertEqual(pkg.recommends, [])
        self.assertEqual(pkg.supplements, [])
        self.assertEqual(pkg.files, [
            ('', '/usr/bin/', 'complex_a'),
            ('dir', '/usr/share/doc/', 'Archer-3.4.5'),
            ('', '/usr/share/doc/Archer-3.4.5/', 'README')
            ])
        self.assertEqual(pkg.changelogs, [
            ('Tomas Mlcoch <tmlcoch@redhat.com> - 1.1.1-1', 1334664000,
                '- First changelog.'),
            ('Tomas Mlcoch <tmlcoch@redhat.com> - 2.2.2-2', 1334750400,
                '- That was totally ninja!'),
            ('Tomas Mlcoch <tmlcoch@redhat.com> - 3.3.3-3', 1365422400,
                '- 3. changelog.')
            ])

        self.assertEqual(pkg.nvra(), "Archer-3.4.5-6.x86_64")
        self.assertEqual(pkg.nevra(), "Archer-2:3.4.5-6.x86_64")

    def test_package_setters(self):
        pkg = cr.Package()
        self.assertTrue(pkg)

        pkg.pkgId = "foo"
        self.assertEqual(pkg.pkgId, "foo")
        pkg.name = "bar"
        self.assertEqual(pkg.name, "bar")
        pkg.arch = "quantum"
        self.assertEqual(pkg.arch, "quantum")
        pkg.version = "11"
        self.assertEqual(pkg.version, "11")
        pkg.epoch = "22"
        self.assertEqual(pkg.epoch, "22")
        pkg.release = "33"
        self.assertEqual(pkg.release, "33")
        pkg.summary = "sum"
        self.assertEqual(pkg.summary, "sum")
        pkg.description = "desc"
        self.assertEqual(pkg.description, "desc")
        pkg.url = "http://foo"
        self.assertEqual(pkg.url, "http://foo")
        pkg.time_file = 111
        self.assertEqual(pkg.time_file, 111)
        pkg.time_build = 112
        self.assertEqual(pkg.time_build, 112)
        pkg.rpm_license = "EULA"
        self.assertEqual(pkg.rpm_license, "EULA")
        pkg.rpm_vendor = "Me"
        self.assertEqual(pkg.rpm_vendor, "Me")
        pkg.rpm_group = "a-team"
        self.assertEqual(pkg.rpm_group, "a-team")
        pkg.rpm_buildhost = "hal3000.space"
        self.assertEqual(pkg.rpm_buildhost, "hal3000.space")
        pkg.rpm_sourcerpm = "source.src.rpm"
        self.assertEqual(pkg.rpm_sourcerpm, "source.src.rpm")
        pkg.rpm_header_start = 1
        self.assertEqual(pkg.rpm_header_start, 1)
        pkg.rpm_header_end = 2
        self.assertEqual(pkg.rpm_header_end, 2)
        pkg.rpm_packager = "Arnold Rimmer"
        self.assertEqual(pkg.rpm_packager, "Arnold Rimmer")
        pkg.size_package = 33
        self.assertEqual(pkg.size_package, 33)
        pkg.size_installed = 44
        self.assertEqual(pkg.size_installed, 44)
        pkg.size_archive = 55
        self.assertEqual(pkg.size_archive, 55)
        pkg.location_href = "package/foo.rpm"
        self.assertEqual(pkg.location_href, "package/foo.rpm")
        pkg.location_base = "file://dir/"
        self.assertEqual(pkg.location_base, "file://dir/")
        pkg.checksum_type = "crc"
        self.assertEqual(pkg.checksum_type, "crc")

        pkg.requires = [('bar', 'GE', '1', '3.2.1', None, True)]
        self.assertEqual(pkg.requires, [('bar', 'GE', '1', '3.2.1', None, True)])
        pkg.provides = [('foo', None, None, None, None, False)]
        self.assertEqual(pkg.provides, [('foo', None, None, None, None, False)])
        pkg.conflicts = [('foobar', 'LT', '0', '1.0.0', None, False)]
        self.assertEqual(pkg.conflicts, [('foobar', 'LT', '0', '1.0.0', None, False)])
        pkg.obsoletes = [('foobar', 'GE', '0', '1.1.0', None, False)]
        self.assertEqual(pkg.obsoletes, [('foobar', 'GE', '0', '1.1.0', None, False)])
        pkg.suggests = [('foo_sug', 'GE', '0', '1.1.0', None, False)]
        self.assertEqual(pkg.suggests, [('foo_sug', 'GE', '0', '1.1.0', None, False)])
        pkg.enhances = [('foo_enh', 'GE', '0', '1.1.0', None, False)]
        self.assertEqual(pkg.enhances, [('foo_enh', 'GE', '0', '1.1.0', None, False)])
        pkg.recommends = [('foo_rec', 'GE', '0', '1.1.0', None, False)]
        self.assertEqual(pkg.recommends, [('foo_rec', 'GE', '0', '1.1.0', None, False)])
        pkg.supplements = [('foo_sup', 'GE', '0', '1.1.0', None, False)]
        self.assertEqual(pkg.supplements, [('foo_sup', 'GE', '0', '1.1.0', None, False)])
        pkg.files = [(None, '/foo/', 'bar')]
        self.assertEqual(pkg.files, [(None, '/foo/', 'bar')])
        pkg.changelogs = [('me', 123456, 'first commit')]
        self.assertEqual(pkg.changelogs, [('me', 123456, 'first commit')])

        self.assertEqual(pkg.nvra(), "bar-11-33.quantum")
        self.assertEqual(pkg.nevra(), "bar-22:11-33.quantum")

    def test_package_copying(self):
        import copy

        pkg_a = cr.Package()
        pkg_a.name = "FooPackage"
        pkg_b = pkg_a
        self.assertEqual(id(pkg_a), id(pkg_b))
        pkg_c = copy.copy(pkg_a)
        self.assertFalse(id(pkg_a) == id(pkg_c))
        pkg_d = copy.deepcopy(pkg_a)
        self.assertFalse(id(pkg_a) == id(pkg_d))
        self.assertFalse(id(pkg_c) == id(pkg_d))

        # Next lines should not fail (if copy really works)
        del(pkg_a)
        del(pkg_b)
        self.assertEqual(pkg_c.name, "FooPackage")
        del(pkg_c)
        self.assertEqual(pkg_d.name, "FooPackage")
        del(pkg_d)

