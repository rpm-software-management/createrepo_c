import re
import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from .fixtures import *

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

        def pkgcb(pkg):
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

    def test_xml_parser_primary_snippet01(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb(pkg):
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        with open(PRIMARY_SNIPPET_01) as primary_snip:
            cr.xml_parse_primary_snippet(primary_snip.read(), newpkgcb, pkgcb, warningcb, 1)

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

        def pkgcb(pkg):
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        with open(PRIMARY_SNIPPET_02) as primary_snip:
            cr.xml_parse_primary_snippet(primary_snip.read(), newpkgcb, pkgcb, warningcb, 1)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['fake_bash', 'super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 2)
        self.assertEqual(userdata["warnings"], [])

    def test_xml_parser_primary_snippet02(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb(pkg):
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_primary(REPO_02_PRIXML, newpkgcb, pkgcb, warningcb, 1)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['fake_bash', 'super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 2)
        self.assertEqual(userdata["warnings"], [])

    def test_xml_parser_primary_repo02_only_pkgcb(self):

        pkgs = []

        def pkgcb(pkg):
            pkgs.append(pkg)

        cr.xml_parse_primary(REPO_02_PRIXML, None, pkgcb, None, 1)

        self.assertEqual([pkg.name for pkg in pkgs],
            ['fake_bash', 'super_kernel'])

    def test_xml_parser_primary_repo02_no_cbs(self):
        self.assertRaises(ValueError,
                          cr.xml_parse_primary,
                          REPO_02_PRIXML, None, None, None, 1)

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
        def pkgcb(pkg):
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

        def pkgcb(pkg):
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

    def test_xml_parser_filelists_snippet01(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb(pkg):
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        with open(FILELISTS_SNIPPET_01) as filelists_snip:
            cr.xml_parse_filelists_snippet(filelists_snip.read(), newpkgcb, pkgcb, warningcb)

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

        def pkgcb(pkg):
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_filelists(REPO_02_FILXML, newpkgcb, pkgcb, warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['fake_bash', 'super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 2)
        self.assertEqual(userdata["warnings"], [])

    def test_xml_parser_filelists_snippet02(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb(pkg):
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        with open(FILELISTS_SNIPPET_02) as filelists_snip:
            cr.xml_parse_filelists_snippet(filelists_snip.read(), newpkgcb, pkgcb, warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['fake_bash', 'super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 2)
        self.assertEqual(userdata["warnings"], [])

    def test_xml_parser_filelists_snippet_huge(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb(pkg):
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        # generete huge filelists snippet
        content = """
                  <package pkgid="68743563000b2a85e7d9d7ce318719217f3bfee6167cd862efd201ff96c1ecbb" name="flat-remix-icon-theme" arch="noarch">
                  <version epoch="0" ver="0.0.20200511" rel="1.fc33"/>
                  """
        for i in range(145951):
            content += "<file>/usr/share/icons/Flat-Remix-Yellow/status/symbolic/user-available-symbolic.svg</file>"
        content += "</package>"

        cr.xml_parse_filelists_snippet(content, newpkgcb, pkgcb, warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['flat-remix-icon-theme'])
        self.assertEqual(userdata["pkgcb_calls"], 1)
        self.assertEqual(userdata["warnings"], [])



    def test_xml_parser_filelists_repo02_only_pkgcb(self):

        pkgs = []

        def pkgcb(pkg):
            pkgs.append(pkg)

        cr.xml_parse_filelists(REPO_02_FILXML, None, pkgcb, None)

        self.assertEqual([pkg.name for pkg in pkgs],
            ['fake_bash', 'super_kernel'])

    def test_xml_parser_filelists_repo02_no_cbs(self):
        self.assertRaises(ValueError,
                          cr.xml_parse_filelists,
                          REPO_02_FILXML, None, None, None)

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
        def pkgcb(pkg):
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

        def pkgcb(pkg):
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
                   1334664000,
                  '- First release'),
                 ('Tomas Mlcoch <tmlcoch@redhat.com> - 6.0.1-2',
                   1334664001,
                   '- Second release')])

    def test_xml_parser_other_snippet01(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb(pkg):
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        with open(OTHER_SNIPPET_01) as other_snip:
            cr.xml_parse_other_snippet(other_snip.read(), newpkgcb, pkgcb, warningcb)

        self.assertEqual(userdata["warnings"], [])
        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 1)

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
                   1334664000,
                  '- First release'),
                 ('Tomas Mlcoch <tmlcoch@redhat.com> - 6.0.1-2',
                   1334664001,
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

        def pkgcb(pkg):
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        with open(OTHER_SNIPPET_02) as other_snip:
            cr.xml_parse_other_snippet(other_snip.read(), newpkgcb, pkgcb, warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['fake_bash', 'super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 2)
        self.assertEqual(userdata["warnings"], [])

    def test_xml_parser_other_snippet02(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb(pkg):
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_other(REPO_02_OTHXML, newpkgcb, pkgcb, warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['fake_bash', 'super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 2)
        self.assertEqual(userdata["warnings"], [])

    def test_xml_parser_other_repo02_only_pkgcb(self):

        pkgs = []

        def pkgcb(pkg):
            pkgs.append(pkg)

        cr.xml_parse_other(REPO_02_OTHXML, None, pkgcb, None)

        self.assertEqual([pkg.name for pkg in pkgs],
            ['fake_bash', 'super_kernel'])

    def test_xml_parser_other_repo02_no_cbs(self):
        self.assertRaises(ValueError,
                          cr.xml_parse_other,
                          REPO_02_OTHXML, None, None, None)

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
        def pkgcb(pkg):
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

class TestCaseXmlParserRepomd(unittest.TestCase):

    def test_xml_parser_repomd_bad_repomd_object(self):
        self.assertRaises(TypeError,
                          cr.xml_parse_repomd,
                          REPO_01_REPOMD, "foo", None)

    def test_xml_parser_repomd_repo01(self):

        warnings = []

        def warningcb(warn_type, msg):
            warnings.append((warn_type, msg))

        repomd = cr.Repomd()

        cr.xml_parse_repomd(REPO_01_REPOMD, repomd, warningcb)

        self.assertEqual(warnings, [])

        self.assertEqual(repomd.revision, "1334667230")
        self.assertEqual(repomd.repo_tags, [])
        self.assertEqual(repomd.distro_tags, [])
        self.assertEqual(repomd.content_tags, [])
        self.assertEqual(len(repomd.records), 3)

        self.assertEqual(repomd.records[0].type, "filelists")
        self.assertEqual(repomd.records[0].location_real, None)
        self.assertEqual(repomd.records[0].location_href,
            "repodata/c7db035d0e6f1b2e883a7fa3229e2d2be70c05a8b8d2b57dbb5f9c1a67483b6c-filelists.xml.gz")
        self.assertEqual(repomd.records[0].checksum,
            "c7db035d0e6f1b2e883a7fa3229e2d2be70c05a8b8d2b57dbb5f9c1a67483b6c")
        self.assertEqual(repomd.records[0].checksum_type, "sha256")
        self.assertEqual(repomd.records[0].checksum_open,
            "85bc611be5d81ac8da2fe01e98ef741d243d1518fcc46ada70660020803fbf09")
        self.assertEqual(repomd.records[0].checksum_open_type, "sha256")
        self.assertEqual(repomd.records[0].timestamp, 1334667230)
        self.assertEqual(repomd.records[0].size, 273)
        self.assertEqual(repomd.records[0].size_open, 389)
        self.assertEqual(repomd.records[0].db_ver, 0)

        self.assertEqual(repomd.records[1].type, "other")
        self.assertEqual(repomd.records[1].location_real, None)
        self.assertEqual(repomd.records[1].location_href,
            "repodata/b752a73d9efd4006d740f943db5fb7c2dd77a8324bd99da92e86bd55a2c126ef-other.xml.gz")
        self.assertEqual(repomd.records[1].checksum,
            "b752a73d9efd4006d740f943db5fb7c2dd77a8324bd99da92e86bd55a2c126ef")
        self.assertEqual(repomd.records[1].checksum_type, "sha256")
        self.assertEqual(repomd.records[1].checksum_open,
            "da6096c924349af0c326224a33be0cdb26897fbe3d25477ac217261652449445")
        self.assertEqual(repomd.records[1].checksum_open_type, "sha256")
        self.assertEqual(repomd.records[1].timestamp, 1334667230)
        self.assertEqual(repomd.records[1].size, 332)
        self.assertEqual(repomd.records[1].size_open, 530)
        self.assertEqual(repomd.records[1].db_ver, 0)

        self.assertEqual(repomd.records[2].type, "primary")
        self.assertEqual(repomd.records[2].location_real, None)
        self.assertEqual(repomd.records[2].location_href,
            "repodata/6c662d665c24de9a0f62c17d8fa50622307739d7376f0d19097ca96c6d7f5e3e-primary.xml.gz")
        self.assertEqual(repomd.records[2].checksum,
            "6c662d665c24de9a0f62c17d8fa50622307739d7376f0d19097ca96c6d7f5e3e")
        self.assertEqual(repomd.records[2].checksum_type, "sha256")
        self.assertEqual(repomd.records[2].checksum_open,
            "0fc6cadf97d515e87491d24dc9712d8ddaf2226a21ae7f131ff42d71a877c496")
        self.assertEqual(repomd.records[2].checksum_open_type, "sha256")
        self.assertEqual(repomd.records[2].timestamp, 1334667230)
        self.assertEqual(repomd.records[2].size, 782)
        self.assertEqual(repomd.records[2].size_open, 2085)
        self.assertEqual(repomd.records[2].db_ver, 0)

    def test_xml_parser_repomd_repo01_nowarningcb(self):

        repomd = cr.Repomd()
        cr.xml_parse_repomd(REPO_01_REPOMD, repomd)
        self.assertEqual(repomd.revision, "1334667230")
        self.assertEqual(repomd.repo_tags, [])
        self.assertEqual(repomd.distro_tags, [])
        self.assertEqual(repomd.content_tags, [])
        self.assertEqual(len(repomd.records), 3)

class TestCaseXmlParserMainMetadataTogether(unittest.TestCase):
    def test_xml_parser_main_metadata_together_repo01(self):
        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb(pkg):
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_main_metadata_together(primary=REPO_01_PRIXML, filelists=REPO_01_FILXML, other=REPO_01_OTHXML,
                                            newpkgcb=newpkgcb, pkgcb=pkgcb, warningcb=warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]], ['super_kernel'])
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
                [(None, '/usr/bin/', 'super_kernel'),
                 (None, '/usr/share/man/', 'super_kernel.8.gz')])
        self.assertEqual(pkg.changelogs,
                [('Tomas Mlcoch <tmlcoch@redhat.com> - 6.0.1-1',
                   1334664000,
                  '- First release'),
                 ('Tomas Mlcoch <tmlcoch@redhat.com> - 6.0.1-2',
                   1334664001,
                   '- Second release')])

    def test_xml_parser_main_metadata_together_repo02(self):

        userdata = {
                "pkgs": [],
                "pkgcb_calls": 0,
                "warnings": []
            }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        def pkgcb(pkg):
            userdata["pkgcb_calls"] += 1

        def warningcb(warn_type, msg):
            userdata["warnings"].append((warn_type, msg))

        cr.xml_parse_main_metadata_together(REPO_02_PRIXML, REPO_02_FILXML, REPO_02_OTHXML, newpkgcb, pkgcb, warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['fake_bash', 'super_kernel'])
        self.assertEqual(userdata["pkgcb_calls"], 2)
        self.assertEqual(userdata["warnings"], [])

    def test_xml_parser_main_metadata_together_repo02_only_pkgcb(self):

        pkgs = []

        def pkgcb(pkg):
            pkgs.append(pkg)

        cr.xml_parse_main_metadata_together(REPO_02_PRIXML, REPO_02_FILXML, REPO_02_OTHXML,
                                            None, pkgcb, None)

        self.assertEqual([pkg.name for pkg in pkgs],
            ['fake_bash', 'super_kernel'])

    def test_xml_parser_main_metadata_together_repo02_no_cbs(self):
        self.assertRaises(ValueError,
                          cr.xml_parse_main_metadata_together,
                          REPO_02_PRIXML, REPO_02_FILXML, REPO_02_OTHXML,
                          None, None, None)

    def test_xml_parser_main_metadata_together_warnings(self):
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

        cr.xml_parse_main_metadata_together(PRIMARY_MULTI_WARN_00_PATH,
                                            FILELISTS_MULTI_WARN_00_PATH,
                                            OTHER_MULTI_WARN_00_PATH,
                                            newpkgcb, None, warningcb)

        self.assertEqual([pkg.name for pkg in userdata["pkgs"]],
            ['fake_bash', 'super_kernel'])
        self.assertEqual(userdata["warnings"],
            [(0, 'Unknown element "fooelement"'),
             (1, 'Missing attribute "type" of a package element'),
             (0, 'Unknown element "foo"'),
             (3, 'Conversion of "foobar" to integer failed'),
             (0, 'Unknown element "bar"'),
             (1, 'Missing attribute "arch" of a package element'),
             (2, 'Unknown file type "xxx"'),
             (0, 'Unknown element "bar"'),
             (1, 'Missing attribute "name" of a package element'),
             (0, 'Unknown element "bar"'),
             (3, 'Conversion of "xxx" to integer failed')])


    def test_xml_parser_main_metadata_together_error(self):
        userdata = { "pkgs": [] }

        def newpkgcb(pkgId, name, arch):
            pkg = cr.Package()
            userdata["pkgs"].append(pkg)
            return pkg

        self.assertRaises(cr.CreaterepoCError, cr.xml_parse_main_metadata_together,
                          PRIMARY_ERROR_00_PATH, REPO_02_FILXML,
                          REPO_02_OTHXML, newpkgcb, None, None)

        # unlike parsing just primary, filelists or other separately when parsing together primary is parsed first fully
        # before newpkgcb is called so the error is detected before any user package is created
        self.assertEqual(len(userdata["pkgs"]), 0)

        # test when the error is filelists
        self.assertRaises(cr.CreaterepoCError, cr.xml_parse_main_metadata_together,
                          REPO_02_PRIXML, FILELISTS_ERROR_00_PATH,
                          REPO_02_OTHXML, newpkgcb, None, None)

        self.assertEqual(len(userdata["pkgs"]), 2)

        # test when the error is other
        userdata = { "pkgs": [] }
        self.assertRaises(cr.CreaterepoCError, cr.xml_parse_main_metadata_together,
                          REPO_02_PRIXML, REPO_02_FILXML,
                          OTHER_ERROR_00_PATH, newpkgcb, None, None)

        self.assertEqual(len(userdata["pkgs"]), 2)

    def test_xml_parser_main_metadata_together_newpkgcb_abort(self):
        def newpkgcb(pkgId, name, arch):
            raise Error("Foo error")
        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_main_metadata_together,
                          REPO_02_PRIXML, REPO_02_FILXML, REPO_02_OTHXML,
                          newpkgcb, None, None)

    def test_xml_parser_main_metadata_together_pkgcb_abort(self):
       def newpkgcb(pkgId, name, arch):
           return cr.Package()
       def pkgcb(pkg):
           raise Error("Foo error")
       self.assertRaises(cr.CreaterepoCError,
                         cr.xml_parse_main_metadata_together,
                         REPO_02_PRIXML, REPO_02_FILXML, REPO_02_OTHXML,
                         newpkgcb, pkgcb, None)

    def test_xml_parser_main_metadata_together_warningcb_abort(self):
        def newpkgcb(pkgId, name, arch):
            return cr.Package()
        def warningcb(type, msg):
            raise Error("Foo error")
        self.assertRaises(cr.CreaterepoCError,
                          cr.xml_parse_main_metadata_together,
                          PRIMARY_MULTI_WARN_00_PATH,
                          FILELISTS_MULTI_WARN_00_PATH,
                          OTHER_MULTI_WARN_00_PATH,
                          newpkgcb, None, warningcb)
