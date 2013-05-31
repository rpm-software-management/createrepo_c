import re
import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from fixtures import *

class TestCaseXmlParserPrimary(unittest.TestCase):

    def test_xml_parser_primary_repo01(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb():
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_primary(REPO_01_PRIXML, newpkgcb, pkgcb, warningcb, 1)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 1)
        self.assertEqual(userdata["warnings"], [])

        pkg = userdata["pkgs"][0]
        self.assertEqual(pkg.pkgId, "152824bff2aa6d54f429d43e87a3ff3a0286505c6d93ec87692b5e3a9e3b97bf")
        self.assertEqual(pkg.name, "super_kernel")
        self.assertEqual(pkg.arch, "x86_64")
        self.assertEqual(pkg.version, "6.0.1")
        self.assertEqual(pkg.epoch, "0")
        self.assertEqual(pkg.release, "2")
        self.assertEqual(pkg.summary, "Test package")
        self.assertEqual(pkg.description, "This package has provides, requires, obsoletes, conflicts options.")
        self.assertEqual(pkg.url, "http://so_super_kernel.com/it_is_awesome/yep_it_really_is")
        self.assertEqual(pkg.time_file, 1334667003)
        self.assertEqual(pkg.time_build, 1334667003)
        self.assertEqual(pkg.rpm_license, "LGPLv2")
        self.assertEqual(pkg.rpm_vendor, None)
        self.assertEqual(pkg.rpm_group, "Applications/System")
        self.assertEqual(pkg.rpm_buildhost, "localhost.localdomain")
        self.assertEqual(pkg.rpm_sourcerpm, "super_kernel-6.0.1-2.src.rpm")
        self.assertEqual(pkg.rpm_header_start, 280)
        self.assertEqual(pkg.rpm_header_end, 2637)
        self.assertEqual(pkg.rpm_packager, None)
        self.assertEqual(pkg.size_package, 2845)
        self.assertEqual(pkg.size_installed, 0)
        self.assertEqual(pkg.size_archive, 404)
        self.assertEqual(pkg.location_href, "super_kernel-6.0.1-2.x86_64.rpm")
        self.assertEqual(pkg.location_base, None)
        self.assertEqual(pkg.checksum_type, "sha256")
        self.assertEqual(pkg.requires,
                [('bzip2', 'GE', '0', '1.0.0', None, True),
                 ('expat', None, None, None, None, True),
                 ('glib', 'GE', '0', '2.26.0', None, False),
                 ('zlib', None, None, None, None, False)])
        self.assertEqual(pkg.provides,
                [('not_so_super_kernel', 'LT', '0', '5.8.0', None, False),
                 ('super_kernel', 'EQ', '0', '6.0.0', None, False),
                 ('super_kernel', 'EQ', '0', '6.0.1', '2', False),
                 ('super_kernel(x86-64)', 'EQ', '0', '6.0.1', '2', False)])
        self.assertEqual(pkg.conflicts,
                [('kernel', None, None, None, None, False),
                 ('super_kernel', 'EQ', '0', '5.0.0', None, False),
                 ('super_kernel', 'LT', '0', '4.0.0', None, False)])
        self.assertEqual(pkg.obsoletes,
                [('kernel', None, None, None, None, False),
                 ('super_kernel', 'EQ', '0', '5.9.0', None, False)])
        self.assertEqual(pkg.files,
                [(None, '/usr/bin/', 'super_kernel')])
        self.assertEqual(pkg.changelogs, [])


    def test_xml_parser_primary_repo02(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb():
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_primary(REPO_02_PRIXML, newpkgcb, pkgcb, warningcb, 1)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['fake_bash', 'super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 2)
        self.assertEqual(userdata["warnings"], [])

    def test_xml_parser_primary_warnings(self):

        userdata = {
                "pkgs": [],
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_primary(PRIMARY_MULTI_WARN_00_PATH,
                             newpkgcb,
                             None,
                             warningcb,
                             1)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['fake_bash', 'super_kernel'])
        self.assertEqual(userdata["warnings"],
            [(0, 'Unknown element "fooelement"'),
             (1, 'Missing attribute "type" of a package element'),
             (0, 'Unknown element "foo"'),
             (3, 'Conversion of "foobar" to integer failed'),
             (2, 'Unknown file type "xxx"'),
             (0, 'Unknown element "bar"')])

    def test_xml_parser_primary_error(self):

        userdata = { "pkgs": [] }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_primary,
                          PRIMARY_ERROR_00_PATH, newpkgcb, None, None, 1)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]], ['fake_bash'])

    def test_xml_parser_primary_newpkgcb_abort(self):
        def newpkgcb(pkgId, name, arch):
            raise Error("Foo error")
        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_primary,
                          REPO_02_PRIXML, newpkgcb, None, None, 1)

    def test_xml_parser_primary_pkgcb_abort(self):
        def newpkgcb(pkgId, name, arch):
            return cr.Package()
        def pkgcb():
            raise Error("Foo error")
        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_primary,
                          REPO_02_PRIXML, newpkgcb, pkgcb, None, 1)

    def test_xml_parser_primary_warningcb_abort(self):
        def newpkgcb(pkgId, name, arch):
            return cr.Package()
        def warningcb(type, msg):
            raise Error("Foo error")
        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_primary,
                          PRIMARY_MULTI_WARN_00_PATH,
                          newpkgcb, None, warningcb, 1)

class TestCaseXmlParserFilelists(unittest.TestCase):

    def test_xml_parser_filelists_repo01(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb():
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_filelists(REPO_01_FILXML, newpkgcb, pkgcb, warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 1)
        self.assertEqual(userdata["warnings"], [])

        pkg = userdata["pkgs"][0]
        self.assertEqual(pkg.pkgId, "152824bff2aa6d54f429d43e87a3ff3a0286505c6d93ec87692b5e3a9e3b97bf")
        self.assertEqual(pkg.name, "super_kernel")
        self.assertEqual(pkg.arch, "x86_64")
        self.assertEqual(pkg.version, "6.0.1")
        self.assertEqual(pkg.epoch, "0")
        self.assertEqual(pkg.release, "2")
        self.assertEqual(pkg.summary, None)
        self.assertEqual(pkg.description, None)
        self.assertEqual(pkg.url, None)
        self.assertEqual(pkg.time_file, 0)
        self.assertEqual(pkg.time_build, 0)
        self.assertEqual(pkg.rpm_license, None)
        self.assertEqual(pkg.rpm_vendor, None)
        self.assertEqual(pkg.rpm_group, None)
        self.assertEqual(pkg.rpm_buildhost, None)
        self.assertEqual(pkg.rpm_sourcerpm, None)
        self.assertEqual(pkg.rpm_header_start, 0)
        self.assertEqual(pkg.rpm_header_end, 0)
        self.assertEqual(pkg.rpm_packager, None)
        self.assertEqual(pkg.size_package, 0)
        self.assertEqual(pkg.size_installed, 0)
        self.assertEqual(pkg.size_archive, 0)
        self.assertEqual(pkg.location_href, None)
        self.assertEqual(pkg.location_base, None)
        self.assertEqual(pkg.checksum_type, None)
        self.assertEqual(pkg.requires, [])
        self.assertEqual(pkg.provides, [])
        self.assertEqual(pkg.conflicts, [])
        self.assertEqual(pkg.obsoletes, [])
        self.assertEqual(pkg.files,
                [(None, '/usr/bin/', 'super_kernel'),
                 (None, '/usr/share/man/', 'super_kernel.8.gz')])
        self.assertEqual(pkg.changelogs, [])

    def test_xml_parser_filelists_repo02(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb():
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_filelists(REPO_02_FILXML, newpkgcb, pkgcb, warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['fake_bash', 'super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 2)
        self.assertEqual(userdata["warnings"], [])

    def test_xml_parser_filelists_warnings(self):

        userdata = {
                "pkgs": [],
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_filelists(FILELISTS_MULTI_WARN_00_PATH,
                               newpkgcb,
                               None,
                               warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['fake_bash', 'super_kernel'])
        self.assertEqual(userdata["warnings"],
            [(1, 'Missing attribute "arch" of a package element'),
             (2, 'Unknown file type "xxx"'),
             (0, 'Unknown element "bar"')])

    def test_xml_parser_filelists_error(self):

        userdata = { "pkgs": [] }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_filelists,
                          FILELISTS_ERROR_00_PATH, newpkgcb, None, None)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]], [])

    def test_xml_parser_filelists_newpkgcb_abort(self):
        def newpkgcb(pkgId, name, arch):
            raise Error("Foo error")
        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_filelists,
                          REPO_02_FILXML, newpkgcb, None, None)

    def test_xml_parser_filelists_pkgcb_abort(self):
        def newpkgcb(pkgId, name, arch):
            return cr.Package()
        def pkgcb():
            raise Error("Foo error")
        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_filelists,
                          REPO_02_FILXML, newpkgcb, pkgcb, None)

    def test_xml_parser_filelists_warningcb_abort(self):
        def newpkgcb(pkgId, name, arch):
            return cr.Package()
        def warningcb(type, msg):
            raise Error("Foo error")
        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_filelists,
                          FILELISTS_MULTI_WARN_00_PATH,
                          newpkgcb, None, warningcb)

class TestCaseXmlParserOther(unittest.TestCase):

    def test_xml_parser_other_repo01(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb():
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_other(REPO_01_OTHXML, newpkgcb, pkgcb, warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 1)
        self.assertEqual(userdata["warnings"], [])

        pkg = userdata["pkgs"][0]
        self.assertEqual(pkg.pkgId, "152824bff2aa6d54f429d43e87a3ff3a0286505c6d93ec87692b5e3a9e3b97bf")
        self.assertEqual(pkg.name, "super_kernel")
        self.assertEqual(pkg.arch, "x86_64")
        self.assertEqual(pkg.version, "6.0.1")
        self.assertEqual(pkg.epoch, "0")
        self.assertEqual(pkg.release, "2")
        self.assertEqual(pkg.summary, None)
        self.assertEqual(pkg.description, None)
        self.assertEqual(pkg.url, None)
        self.assertEqual(pkg.time_file, 0)
        self.assertEqual(pkg.time_build, 0)
        self.assertEqual(pkg.rpm_license, None)
        self.assertEqual(pkg.rpm_vendor, None)
        self.assertEqual(pkg.rpm_group, None)
        self.assertEqual(pkg.rpm_buildhost, None)
        self.assertEqual(pkg.rpm_sourcerpm, None)
        self.assertEqual(pkg.rpm_header_start, 0)
        self.assertEqual(pkg.rpm_header_end, 0)
        self.assertEqual(pkg.rpm_packager, None)
        self.assertEqual(pkg.size_package, 0)
        self.assertEqual(pkg.size_installed, 0)
        self.assertEqual(pkg.size_archive, 0)
        self.assertEqual(pkg.location_href, None)
        self.assertEqual(pkg.location_base, None)
        self.assertEqual(pkg.checksum_type, None)
        self.assertEqual(pkg.requires, [])
        self.assertEqual(pkg.provides, [])
        self.assertEqual(pkg.conflicts, [])
        self.assertEqual(pkg.obsoletes, [])
        self.assertEqual(pkg.files, [])
        self.assertEqual(pkg.changelogs,
                [('Tomas Mlcoch <tmlcoch@redhat.com> - 6.0.1-1',
                   1334664000L,
                  '- First release'),
                 ('Tomas Mlcoch <tmlcoch@redhat.com> - 6.0.1-2',
                   1334664001L,
                   '- Second release')])

    def test_xml_parser_other_repo02(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb():
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_other(REPO_02_OTHXML, newpkgcb, pkgcb, warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['fake_bash', 'super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 2)
        self.assertEqual(userdata["warnings"], [])

    def test_xml_parser_other_warnings(self):

        userdata = {
                "pkgs": [],
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_other(OTHER_MULTI_WARN_00_PATH,
                           newpkgcb,
                           None,
                           warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            [None, 'super_kernel'])
        self.assertEqual(userdata["warnings"],
            [(1, 'Missing attribute "name" of a package element'),
             (0, 'Unknown element "bar"'),
             (3, 'Conversion of "xxx" to integer failed')])

    def test_xml_parser_other_error(self):

        userdata = { "pkgs": [] }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_other,
                          OTHER_ERROR_00_PATH, newpkgcb, None, None)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]], [])

    def test_xml_parser_other_newpkgcb_abort(self):
        def newpkgcb(pkgId, name, arch):
            raise Error("Foo error")
        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_other,
                          REPO_02_OTHXML, newpkgcb, None, None)

    def test_xml_parser_other_pkgcb_abort(self):
        def newpkgcb(pkgId, name, arch):
            return cr.Package()
        def pkgcb():
            raise Error("Foo error")
        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_other,
                          REPO_02_OTHXML, newpkgcb, pkgcb, None)

    def test_xml_parser_other_warningcb_abort(self):
        def newpkgcb(pkgId, name, arch):
            return cr.Package()
        def warningcb(type, msg):
            raise Error("Foo error")
        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_other,
                          OTHER_MULTI_WARN_00_PATH,
                          newpkgcb, None, warningcb)
