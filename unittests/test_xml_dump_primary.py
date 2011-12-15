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


OUTPUT = """<package type="rpm"><name>info</name><arch>i686</arch><version epoch="0" ver="4.13a" rel="10.fc14"/><checksum pkgid="YES" type="sha256">2d27dabeda8db51fad63073dc442a859c9dcbf8b2526d9fc4a4b3037df27df39</checksum><summary>A stand-alone TTY-based reader for GNU texinfo documentation</summary><description>The GNU project uses the texinfo file format for much of its
documentation. The info package provides a standalone TTY-based
browser program for viewing texinfo files.</description><packager>Fedora Project</packager><url>http://www.gnu.org/software/texinfo/</url><time file="1315916291" build="1283257696"/><size package="176520" installed="314734" archive="316556"/><location href="info-4.13a-10.fc14.i686.rpm"/><format><rpm:license>GPLv3+</rpm:license><rpm:vendor>Fedora Project</rpm:vendor><rpm:group>System Environment/Base</rpm:group><rpm:buildhost>x86-04.phx2.fedoraproject.org</rpm:buildhost><rpm:sourcerpm>texinfo-4.13a-10.fc14.src.rpm</rpm:sourcerpm><rpm:header-range start="1384" end="16796"/><rpm:provides><rpm:entry name="config(info)" flags="EQ" epoch="0" ver="4.13a" rel="10.fc14"/><rpm:entry name="info" flags="EQ" epoch="0" ver="4.13a" rel="10.fc14"/><rpm:entry name="info(x86-32)" flags="EQ" epoch="0" ver="4.13a" rel="10.fc14"/></rpm:provides><rpm:requires><rpm:entry name="rpmlib(CompressedFileNames)" flags="LE" epoch="0" ver="3.0.4" rel="1" pre="0"/><rpm:entry name="rpmlib(PayloadFilesHavePrefix)" flags="LE" epoch="0" ver="4.0" rel="1" pre="0"/><rpm:entry name="rpmlib(FileDigests)" flags="LE" epoch="0" ver="4.6.0" rel="1" pre="0"/><rpm:entry name="rpmlib(PayloadIsXz)" flags="LE" epoch="0" ver="5.2" rel="1" pre="0"/></rpm:requires><file>/sbin/install-info</file><file>/usr/bin/info</file><file>/usr/bin/infokey</file></format></package>
"""

EMPTY_DICT_OUTPUT = """<package type="rpm"><name></name><arch></arch><version epoch="" ver="" rel=""/><checksum pkgid="YES" type=""></checksum><summary></summary><description></description><packager></packager><url></url><time file="0" build="0"/><size package="0" installed="0" archive="0"/><location href=""/><format><rpm:license></rpm:license><rpm:vendor></rpm:vendor><rpm:group></rpm:group><rpm:buildhost></rpm:buildhost><rpm:sourcerpm></rpm:sourcerpm><rpm:header-range start="0" end="0"/></format></package>
"""

PARTIALY_OK_OUTPUT = """<package type="rpm"><name></name><arch></arch><version epoch="" ver="" rel=""/><checksum pkgid="YES" type=""></checksum><summary></summary><description></description><packager></packager><url></url><time file="123" build="456"/><size package="139" installed="146" archive="143"/><location href=""/><format><rpm:license></rpm:license><rpm:vendor></rpm:vendor><rpm:group></rpm:group><rpm:buildhost></rpm:buildhost><rpm:sourcerpm></rpm:sourcerpm><rpm:header-range start="789" end="0"/></format></package>
"""

WRONG_NUM_VALUES_OUTPUT = """<package type="rpm"><name></name><arch></arch><version epoch="" ver="" rel=""/><checksum pkgid="YES" type=""></checksum><summary></summary><description></description><packager></packager><url></url><time file="-1" build="-1"/><size package="-1" installed="-1" archive="-1"/><location href=""/><format><rpm:license></rpm:license><rpm:vendor></rpm:vendor><rpm:group></rpm:group><rpm:buildhost></rpm:buildhost><rpm:sourcerpm></rpm:sourcerpm><rpm:header-range start="-1" end="-1"/></format></package>
"""


class TestXmlDumpPrimary(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_dump(self):
        pkg = RepoPackage(RPM)
        pkg_dict = pkg.get_pkg_data()
        xml = xml_dump.xml_dump_primary(pkg_dict, None)
        self.assertEqual(OUTPUT, xml)

    def test_dump_with_empty_input(self):
        pkg_dict = {}
        xml = xml_dump.xml_dump_primary(pkg_dict, None)
        self.assertEqual(EMPTY_DICT_OUTPUT, xml)

    def test_dump_with_partialy_ok_input_1(self):
        pkg_dict = {
            'size_package': 139,
            "size_installed": 146,
            "size_archive": 143,
            "time_file": 123,
            "time_build": 456,
            "rpm_header_start": 789,
            "rpm_header_end": 000,
        }
        xml = xml_dump.xml_dump_primary(pkg_dict, None)
        self.assertEqual(PARTIALY_OK_OUTPUT, xml)

    def test_dump_with_partialy_ok_input_2(self):
        pkg_dict = {
            'size_package': 139L,
            "size_installed": 146L,
            "size_archive": 143L,
            "time_file": 123L,
            "time_build": 456L,
            "rpm_header_start": 789L,
            "rpm_header_end": 000L,
        }
        xml = xml_dump.xml_dump_primary(pkg_dict, None)
        self.assertEqual(PARTIALY_OK_OUTPUT, xml)

    def test_dump_with_wrong_input_1(self):
        pkg_dict = {'name': 123}
        xml = xml_dump.xml_dump_primary(pkg_dict, None)
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
        xml = xml_dump.xml_dump_primary(pkg_dict, None)
        self.assertEqual(WRONG_NUM_VALUES_OUTPUT, xml)

    def test_dump_with_wrong_input_3(self):
        pkg_dict = {
            "requires": 123,
            "provides": 456,
            "conflicts": 789,
            "oboletes": 000,
        }
        xml = xml_dump.xml_dump_primary(pkg_dict, None)
        self.assertEqual(EMPTY_DICT_OUTPUT, xml)

    def test_dump_with_wrong_input_4(self):
        pkg_dict = {
            "files": 123,
            "changelogs": 456,
        }
        xml = xml_dump.xml_dump_primary(pkg_dict, None)
        self.assertEqual(EMPTY_DICT_OUTPUT, xml)


if __name__ == "__main__":
    unittest.main()
