#!/usr/bin/python
# -*- coding: utf-8 -*-


import os
import os.path
import unittest
import run_tests # set sys.path

import xml_dump
from repopackage import RepoPackage
from pprint import pprint

RPM = "info-4.13a-10.fc14.i686.rpm"


OUTPUT = """<package pkgid="2d27dabeda8db51fad63073dc442a859c9dcbf8b2526d9fc4a4b3037df27df39" name="info" arch="i686"><version epoch="0" ver="4.13a" rel="10.fc14"/><changelog author="Vitezslav Crhonek &lt;vcrhonek@redhat.com&gt; - 4.13a-10" date="1283256000">- Fix info crash when using index in help window
  Resolves: #579263</changelog><changelog author="Jan Gorig &lt;jgorig@redhat.com&gt; - 4.13a-9" date="1263211200">- Fix PowerPC return code bug #531349</changelog><changelog author="Vitezslav Crhonek &lt;vcrhonek@redhat.com&gt; - 4.13a-8" date="1260792000">- Fix memory allocation bug when using old-style --section &quot;Foo&quot; arguments</changelog><changelog author="Vitezslav Crhonek &lt;vcrhonek@redhat.com&gt; - 4.13a-7" date="1251892800">- Fix errors installing texinfo/info with --excludedocs
  Resolves: #515909
  Resolves: #515938</changelog><changelog author="Ville Skyttä &lt;ville.skytta@iki.fi&gt; - 4.13a-6" date="1250078400">- Use lzma compressed upstream tarball.</changelog><changelog author="Vitezslav Crhonek &lt;vcrhonek@redhat.com&gt; - 4.13a-5" date="1249473600">- Fix changelog entry and rebuild</changelog><changelog author="Vitezslav Crhonek &lt;vcrhonek@redhat.com&gt; - 4.13a-4" date="1249387200">- Fix data types (fix by Ralf Corsepius)
  Resolves: #515402</changelog><changelog author="Fedora Release Engineering &lt;rel-eng@lists.fedoraproject.org&gt; - 4.13a-3" date="1248609600">- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild</changelog><changelog author="Fedora Release Engineering &lt;rel-eng@lists.fedoraproject.org&gt; - 4.13a-2" date="1235563200">- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild</changelog><changelog author="Vitezslav Crhonek &lt;vcrhonek@redhat.com&gt; - 4.13-1" date="1227182400">- Update to texinfo-4.13a
  Resolves: #471194
  Resolves: #208511</changelog></package>
"""

EMPTY_DICT_OUTPUT = """<package pkgid="" name="" arch=""><version epoch="" ver="" rel=""/></package>
"""

WRONG_NUM_VALUES_OUTPUT = """<package pkgid="" name="" arch=""><version epoch="" ver="" rel=""/></package>
"""

PARTIALY_RIGHT_OUTPUT = """<package pkgid="" name="" arch=""><version epoch="" ver="" rel=""/><changelog author="foo" date="123456">foo</changelog></package>
"""


class TestXmlDumpPrimary(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_dump(self):
        pkg = RepoPackage(RPM)
        pkg_dict = pkg.get_pkg_data()
        xml = xml_dump.xml_dump_other(pkg_dict, None)
        self.assertEqual(OUTPUT, xml)

    def test_dump_with_empty_input(self):
        pkg_dict = {}
        xml = xml_dump.xml_dump_other(pkg_dict, None)
        self.assertEqual(EMPTY_DICT_OUTPUT, xml)

    def test_dump_with_wrong_input_1(self):
        pkg_dict = {'name': 123}
        xml = xml_dump.xml_dump_other(pkg_dict, None)
        self.assertEqual(EMPTY_DICT_OUTPUT, xml)

    def test_dump_with_wrong_input_2(self):
        pkg_dict = {
            'size_package': "foo",
            "size_installed": "foo",
            "size_archive": "foo",
            "time_file": "foo",
            "time_build": "foo",
            "rpm_header_start": "foo",
            "rpm_header_end": "foo",
        }
        xml = xml_dump.xml_dump_other(pkg_dict, None)
        self.assertEqual(WRONG_NUM_VALUES_OUTPUT, xml)

    def test_dump_with_wrong_input_3(self):
        pkg_dict = {
            "requires": 123,
            "provides": 456,
            "conflicts": 789,
            "oboletes": 000,
        }
        xml = xml_dump.xml_dump_other(pkg_dict, None)
        self.assertEqual(EMPTY_DICT_OUTPUT, xml)

    def test_dump_with_wrong_input_4(self):
        pkg_dict = {
            "files": 123,
            "changelogs": 456,
        }
        xml = xml_dump.xml_dump_other(pkg_dict, None)
        self.assertEqual(EMPTY_DICT_OUTPUT, xml)

    def test_dump_with_wrong_input_5(self):
        pkg_dict = {
            "changelogs": [(123, 123, 123)],
        }
        xml = xml_dump.xml_dump_other(pkg_dict, None)
        self.assertEqual(EMPTY_DICT_OUTPUT, xml)

    def test_dump_with_wrong_input_6(self):
        pkg_dict = {
            "changelogs": [("foo", "foo", "foo")],
        }
        xml = xml_dump.xml_dump_other(pkg_dict, None)
        self.assertEqual(EMPTY_DICT_OUTPUT, xml)

    def test_dump_with_wrong_input_7(self):
        pkg_dict = {
            "changelogs": [(123456, "foo", "foo"), ("foo", "foo", "foo")],
        }
        xml = xml_dump.xml_dump_other(pkg_dict, None)
        self.assertEqual(PARTIALY_RIGHT_OUTPUT, xml)

    def test_dump_with_wrong_input_8(self):
        pkg_dict = {
            "changelogs": [(123456L, "foo", "foo"), ("foo", "foo", "foo")],
        }
        xml = xml_dump.xml_dump_other(pkg_dict, None)
        self.assertEqual(PARTIALY_RIGHT_OUTPUT, xml)


if __name__ == "__main__":
    unittest.main()
