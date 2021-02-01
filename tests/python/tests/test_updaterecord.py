from datetime import datetime
import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from .fixtures import *

class TestCaseUpdateRecord(unittest.TestCase):

    def test_updaterecord_setters(self):
        now = datetime.now()
        # Microseconds are always 0 in updateinfo
        now = datetime(now.year, now.month, now.day, now.hour, now.minute,
                       now.second, 0)

        rec = cr.UpdateRecord()
        self.assertTrue(rec)

        self.assertEqual(rec.fromstr, None)
        self.assertEqual(rec.status, None)
        self.assertEqual(rec.type, None)
        self.assertEqual(rec.version, None)
        self.assertEqual(rec.id, None)
        self.assertEqual(rec.title, None)
        self.assertEqual(rec.issued_date, None)
        self.assertEqual(rec.updated_date, None)
        self.assertEqual(rec.rights, None)
        self.assertEqual(rec.release, None)
        self.assertEqual(rec.pushcount, None)
        self.assertEqual(rec.severity, None)
        self.assertEqual(rec.summary, None)
        self.assertEqual(rec.description, None)
        self.assertEqual(rec.reboot_suggested, 0)
        self.assertEqual(rec.solution, None)
        self.assertEqual(rec.references, [])
        self.assertEqual(rec.collections, [])

        ref = cr.UpdateReference()
        ref.href = "href"
        ref.id = "id"
        ref.type = "type"
        ref.title = "title"

        col = cr.UpdateCollection()
        col.shortname = "short name"
        col.name = "long name"

        rec.fromstr = "from"
        rec.status = "status"
        rec.type = "type"
        rec.version = "version"
        rec.id = "id"
        rec.title = "title"
        rec.issued_date = now
        rec.updated_date = now
        rec.rights = "rights"
        rec.release = "release"
        rec.pushcount = "pushcount"
        rec.severity = "severity"
        rec.summary = "summary"
        rec.description = "description"
        rec.reboot_suggested = True
        rec.solution = "solution"
        rec.append_reference(ref)
        rec.append_collection(col)

        self.assertEqual(rec.fromstr, "from")
        self.assertEqual(rec.status, "status")
        self.assertEqual(rec.type, "type")
        self.assertEqual(rec.version, "version")
        self.assertEqual(rec.id, "id")
        self.assertEqual(rec.title, "title")
        self.assertEqual(rec.issued_date, now)
        self.assertEqual(rec.updated_date, now)
        self.assertEqual(rec.rights, "rights")
        self.assertEqual(rec.release, "release")
        self.assertEqual(rec.pushcount, "pushcount")
        self.assertEqual(rec.severity, "severity")
        self.assertEqual(rec.summary, "summary")
        self.assertEqual(rec.reboot_suggested, True)
        self.assertEqual(rec.description, "description")
        self.assertEqual(rec.solution, "solution")
        self.assertEqual(len(rec.references), 1)
        self.assertEqual(len(rec.collections), 1)

        ref = rec.references[0]
        self.assertEqual(ref.href, "href")
        self.assertEqual(ref.id, "id")
        self.assertEqual(ref.type, "type")
        self.assertEqual(ref.title, "title")

        col = rec.collections[0]
        self.assertEqual(col.shortname, "short name")
        self.assertEqual(col.name, "long name")
        self.assertEqual(len(col.packages), 0)

    def test_xml_dump_updaterecord(self):
        now = datetime.now()
        # Microseconds are always 0 in updateinfo
        now = datetime(now.year, now.month, now.day, now.hour, now.minute,
                       now.second, 0)

        rec = cr.UpdateRecord()
        rec.fromstr = "from"
        rec.status = "status"
        rec.type = "type"
        rec.version = "version"
        rec.id = "id"
        rec.title = "title"
        rec.issued_date = now
        rec.updated_date = now
        rec.rights = "rights"
        rec.release = "release"
        rec.pushcount = "pushcount"
        rec.severity = "severity"
        rec.summary = "summary"
        rec.description = "description"
        rec.solution = "solution"
        rec.reboot_suggested = True

        xml = cr.xml_dump_updaterecord(rec)
        self.assertEqual(xml,
"""  <update from="from" status="status" type="type" version="version">
    <id>id</id>
    <title>title</title>
    <issued date="%(now)s"/>
    <updated date="%(now)s"/>
    <rights>rights</rights>
    <release>release</release>
    <pushcount>pushcount</pushcount>
    <severity>severity</severity>
    <summary>summary</summary>
    <description>description</description>
    <solution>solution</solution>
    <reboot_suggested>True</reboot_suggested>
    <references/>
    <pkglist/>
  </update>
""" % {"now": now.strftime("%Y-%m-%d %H:%M:%S")})

    def test_xml_dump_updaterecord_no_updated_date(self):
        now = datetime.now()
        # Microseconds are always 0 in updateinfo
        now = datetime(now.year, now.month, now.day, now.hour, now.minute, now.second, 0)

        rec = cr.UpdateRecord()
        rec.fromstr = "from"
        rec.status = "status"
        rec.type = "type"
        rec.version = "version"
        rec.id = "id"
        rec.title = "title"
        rec.issued_date = now
        rec.rights = "rights"
        rec.release = "release"
        rec.pushcount = "pushcount"
        rec.severity = "severity"
        rec.summary = "summary"
        rec.description = "description"
        rec.solution = "solution"
        rec.reboot_suggested = True

        target_xml = \
  """  <update from="from" status="status" type="type" version="version">
    <id>id</id>
    <title>title</title>
    <issued date="%(now)s"/>
    <rights>rights</rights>
    <release>release</release>
    <pushcount>pushcount</pushcount>
    <severity>severity</severity>
    <summary>summary</summary>
    <description>description</description>
    <solution>solution</solution>
    <reboot_suggested>True</reboot_suggested>
    <references/>
    <pkglist/>
  </update>
""" % {"now": now.strftime("%Y-%m-%d %H:%M:%S")}

        xml = cr.xml_dump_updaterecord(rec)
        self.assertEqual(xml, target_xml)

        # Setting it to None is the same as not setting it at all
        rec.updated_date = None

        xml = cr.xml_dump_updaterecord(rec)
        self.assertEqual(xml, target_xml)
