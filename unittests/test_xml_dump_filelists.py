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


OUTPUT = """<package pkgid="2d27dabeda8db51fad63073dc442a859c9dcbf8b2526d9fc4a4b3037df27df39" name="info" arch="i686"><version epoch="0" ver="4.13a" rel="10.fc14"/><file>/sbin/install-info</file><file>/usr/bin/info</file><file>/usr/bin/infokey</file><file type="dir">/usr/share/doc/info-4.13a</file><file>/usr/share/doc/info-4.13a/COPYING</file><file>/usr/share/info/dir</file><file>/usr/share/info/info-stnd.info.gz</file><file>/usr/share/info/info.info.gz</file><file>/usr/share/man/man1/info.1.gz</file><file>/usr/share/man/man1/infokey.1.gz</file><file>/usr/share/man/man1/install-info.1.gz</file><file>/usr/share/man/man5/info.5.gz</file></package>
"""

EMPTY_DICT_OUTPUT = """<package pkgid="" name="" arch=""><version epoch="" ver="" rel=""/></package>
"""

PARTIAL_OK_OUTPUT = """<package pkgid="" name="" arch=""><version epoch="" ver="" rel=""/><file>/file1</file><file>/file2</file></package>
"""

WRONG_NUM_VALUES_OUTPUT = """<package pkgid="" name="" arch=""><version epoch="" ver="" rel=""/></package>
"""


class TestXmlDumpFilelists(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_dump(self):
        pkg = RepoPackage(RPM)
        pkg_dict = pkg.get_pkg_data()
        xml = xml_dump.xml_dump_filelists(pkg_dict, None)
        self.assertEqual(OUTPUT, xml)

    def test_dump_with_empty_input(self):
        pkg_dict = {}
        xml = xml_dump.xml_dump_filelists(pkg_dict, None)
        self.assertEqual(EMPTY_DICT_OUTPUT, xml)

    def test_dump_with_wrong_input_1(self):
        pkg_dict = {'name': 123}
        xml = xml_dump.xml_dump_filelists(pkg_dict, None)
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
        xml = xml_dump.xml_dump_filelists(pkg_dict, None)
        self.assertEqual(WRONG_NUM_VALUES_OUTPUT, xml)

    def test_dump_with_wrong_input_3(self):
        pkg_dict = {
            "requires": 123,
            "provides": 456,
            "conflicts": 789,
            "oboletes": 000,
        }
        xml = xml_dump.xml_dump_filelists(pkg_dict, None)
        self.assertEqual(EMPTY_DICT_OUTPUT, xml)

    def test_dump_with_wrong_input_4(self):
        pkg_dict = {
            "files": 123,
            "changelogs": 456,
        }
        xml = xml_dump.xml_dump_filelists(pkg_dict, None)
        self.assertEqual(EMPTY_DICT_OUTPUT, xml)

    def test_dump_with_partialy_ok_input_1(self):
        pkg_dict = {
            "files": [('/file1', 'file'), ('/file2', ''), ('/file3',), (), [], [1,2]],
        }
        xml = xml_dump.xml_dump_filelists(pkg_dict, None)
        self.assertEqual(PARTIAL_OK_OUTPUT, xml)


if __name__ == "__main__":
    unittest.main()
