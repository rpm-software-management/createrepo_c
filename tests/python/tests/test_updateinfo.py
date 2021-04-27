from datetime import datetime
import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from .fixtures import *

class TestCaseUpdateInfo(unittest.TestCase):

    def test_updateinfo_setters(self):
        now = datetime.now()
        # Microseconds are always 0 in updateinfo
        now = datetime(now.year, now.month, now.day, now.hour, now.minute,
                       now.second, 0)

        ui = cr.UpdateInfo()
        self.assertTrue(ui)

        self.assertEqual(ui.updates, [])

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

        ui.append(rec)

        self.assertEqual(len(ui.updates), 1)
        rec = ui.updates[0]

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
        self.assertEqual(rec.description, "description")
        self.assertEqual(rec.solution, "solution")
        self.assertEqual(rec.reboot_suggested, True)
        self.assertEqual(len(rec.references), 0)
        self.assertEqual(len(rec.collections), 0)

        rec = cr.UpdateRecord()
        rec.issued_date = int(now.timestamp())
        ui.append(rec)

        self.assertEqual(len(ui.updates), 2)
        rec = ui.updates[1]
        self.assertEqual(rec.issued_date, int(now.timestamp()))

    def test_updateinfo_getter(self):
        ui = cr.UpdateInfo(TEST_UPDATEINFO_03)
        self.assertTrue(ui)

        self.assertEqual(len(ui.updates), 6)

        rec = ui.updates[2]
        self.assertRaisesRegex(cr.CreaterepoCError, "Unable to parse updateinfo record date: 15mangled2",
                               rec.__getattribute__, "issued_date")

    def test_updateinfo_xml_dump_01(self):
        ui = cr.UpdateInfo()
        xml = ui.xml_dump()

        self.assertEqual(xml,
        """<?xml version="1.0" encoding="UTF-8"?>\n<updates/>\n""")

    def test_updateinfo_xml_dump_02(self):
        now = datetime.now()
        # Microseconds are always 0 in updateinfo
        now = datetime(now.year, now.month, now.day, now.hour, now.minute,
                       now.second, 0)

        ui = cr.UpdateInfo()
        xml = ui.xml_dump()

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

        ui.append(rec)
        xml = ui.xml_dump()

        self.assertEqual(xml,
"""<?xml version="1.0" encoding="UTF-8"?>
<updates>
  <update from="from" status="status" type="type" version="version">
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
</updates>
""" % {"now": now.strftime("%Y-%m-%d %H:%M:%S")})

    def test_updateinfo_xml_dump_03(self):
        now = datetime.now()
        # Microseconds are always 0 in updateinfo
        now = datetime(now.year, now.month, now.day, now.hour, now.minute,
                       now.second, 0)

        mod = cr.UpdateCollectionModule()
        mod.name = "kangaroo"
        mod.stream = "0"
        mod.version = 18446744073709551615
        mod.context = "deadbeef"
        mod.arch = "x86"

        pkg = cr.UpdateCollectionPackage()
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

        col = cr.UpdateCollection()
        col.shortname = "short name"
        col.name = "long name"
        col.module = mod
        col.append(pkg)

        ref = cr.UpdateReference()
        ref.href = "href"
        ref.id = "id"
        ref.type = "type"
        ref.title = "title"

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
        rec.append_collection(col)
        rec.append_reference(ref)

        ui = cr.UpdateInfo()
        ui.append(rec)

        xml = ui.xml_dump()

        self.assertEqual(xml,
"""<?xml version="1.0" encoding="UTF-8"?>
<updates>
  <update from="from" status="status" type="type" version="version">
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
    <references>
      <reference href="href" id="id" type="type" title="title"/>
    </references>
    <pkglist>
      <collection short="short name">
        <name>long name</name>
        <module name="kangaroo" stream="0" version="18446744073709551615" context="deadbeef" arch="x86"/>
        <package name="foo" version="1.2" release="3" epoch="0" arch="x86" src="foo.src.rpm">
          <filename>foo.rpm</filename>
          <sum type="sha256">abcdef</sum>
          <reboot_suggested>True</reboot_suggested>
          <restart_suggested>True</restart_suggested>
          <relogin_suggested>True</relogin_suggested>
        </package>
      </collection>
    </pkglist>
  </update>
</updates>
""" % {"now": now.strftime("%Y-%m-%d %H:%M:%S")})

    def test_updateinfo_xml_dump_04(self):
        now = datetime.now()
        # Microseconds are always 0 in updateinfo
        now = datetime(now.year, now.month, now.day, now.hour, now.minute,
                       now.second, 0)

        pkg = cr.UpdateCollectionPackage()
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

        # Collection without module
        col = cr.UpdateCollection()
        col.shortname = "short name"
        col.name = "long name"
        col.append(pkg)

        ref = cr.UpdateReference()
        ref.href = "href"
        ref.id = "id"
        ref.type = "type"
        ref.title = "title"

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
        rec.append_collection(col)
        rec.append_reference(ref)

        ui = cr.UpdateInfo()
        ui.append(rec)

        xml = ui.xml_dump()

        self.assertEqual(xml,
"""<?xml version="1.0" encoding="UTF-8"?>
<updates>
  <update from="from" status="status" type="type" version="version">
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
    <references>
      <reference href="href" id="id" type="type" title="title"/>
    </references>
    <pkglist>
      <collection short="short name">
        <name>long name</name>
        <package name="foo" version="1.2" release="3" epoch="0" arch="x86" src="foo.src.rpm">
          <filename>foo.rpm</filename>
          <sum type="sha256">abcdef</sum>
          <reboot_suggested>True</reboot_suggested>
        </package>
      </collection>
    </pkglist>
  </update>
</updates>
""" % {"now": now.strftime("%Y-%m-%d %H:%M:%S")})

    def test_updateinfo_xml_dump_05(self):
        now = datetime.now()
        # Microseconds are always 0 in updateinfo
        now = datetime(now.year, now.month, now.day, now.hour, now.minute,
                       now.second, 0)

        # Collection module with unset fields
        mod = cr.UpdateCollectionModule()
        mod.version = 18446744073709551615
        mod.context = "deadbeef"
        mod.arch = "x86"

        pkg = cr.UpdateCollectionPackage()
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

        col = cr.UpdateCollection()
        col.shortname = "short name"
        col.name = "long name"
        col.module = mod
        col.append(pkg)

        ref = cr.UpdateReference()
        ref.href = "href"
        ref.id = "id"
        ref.type = "type"
        ref.title = "title"

        rec = cr.UpdateRecord()
        rec.fromstr = "from"
        rec.status = "status"
        rec.type = "type"
        rec.version = "version"
        rec.id = "id"
        rec.title = "title"
        rec.issued_date = int(now.timestamp())
        rec.updated_date = now
        rec.rights = "rights"
        rec.release = "release"
        rec.pushcount = "pushcount"
        rec.severity = "severity"
        rec.summary = "summary"
        rec.description = "description"
        rec.solution = "solution"
        rec.reboot_suggested = True
        rec.append_collection(col)
        rec.append_reference(ref)

        ui = cr.UpdateInfo()
        ui.append(rec)

        xml = ui.xml_dump()

        self.assertEqual(xml,
"""<?xml version="1.0" encoding="UTF-8"?>
<updates>
  <update from="from" status="status" type="type" version="version">
    <id>id</id>
    <title>title</title>
    <issued date="%(now_epoch)s"/>
    <updated date="%(now)s"/>
    <rights>rights</rights>
    <release>release</release>
    <pushcount>pushcount</pushcount>
    <severity>severity</severity>
    <summary>summary</summary>
    <description>description</description>
    <solution>solution</solution>
    <reboot_suggested>True</reboot_suggested>
    <references>
      <reference href="href" id="id" type="type" title="title"/>
    </references>
    <pkglist>
      <collection short="short name">
        <name>long name</name>
        <module version="18446744073709551615" context="deadbeef" arch="x86"/>
        <package name="foo" version="1.2" release="3" epoch="0" arch="x86" src="foo.src.rpm">
          <filename>foo.rpm</filename>
          <sum type="sha256">abcdef</sum>
          <reboot_suggested>True</reboot_suggested>
          <restart_suggested>True</restart_suggested>
          <relogin_suggested>True</relogin_suggested>
        </package>
      </collection>
    </pkglist>
  </update>
</updates>
""" % {"now": now.strftime("%Y-%m-%d %H:%M:%S"), "now_epoch": now.strftime('%s')})
