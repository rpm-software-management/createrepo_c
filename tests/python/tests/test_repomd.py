import re
import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from fixtures import *

class TestCaseRepomd(unittest.TestCase):

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="createrepo_ctest-")
        self.FN_00 = "primary.xml.gz"
        self.FN_01 = "primary.xml"
        self.path00 = os.path.join(self.tmpdir, self.FN_00)
        self.path01 = os.path.join(self.tmpdir, self.FN_01)

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def xxx_repomdrecord_fill(self):
        self.assertRaises(TypeError, cr.RepomdRecord)
        self.assertRaises(TypeError, cr.RepomdRecord, None)

        shutil.copyfile(REPO_00_PRIXML, self.path00)
        self.assertTrue(os.path.exists(self.path00))

        rec = cr.RepomdRecord(self.path00)
        self.assertTrue(rec)
        rec.fill(cr.SHA256)
        rec.rename_file()

        # Filename shoud contain a (valid) checksum
        self.assertEqual(os.listdir(self.tmpdir),
            ['dabe2ce5481d23de1f4f52bdcfee0f9af98316c9e0de2ce8123adeefa0dd08b9-primary.xml.gz'])

    def test_repomd(self):
        shutil.copyfile(REPO_00_PRIXML, self.path00)
        self.assertTrue(os.path.exists(self.path00))

        md = cr.Repomd()
        self.assertTrue(md)

        xml = md.xml_dump()
        # Revision shoud be current Unix time
        self.assertTrue(re.search(r"<revision>[0-9]+</revision>", xml))

        md.set_revision("foobar")

        md.add_distro_tag("tag1")
        md.add_distro_tag("tag2", "cpeid1")
        md.add_distro_tag("tag3", cpeid="cpeid2")

        md.add_repo_tag("repotag")

        md.add_content_tag("contenttag")

        xml = md.xml_dump()
        self.assertEqual(xml,
"""<?xml version="1.0" encoding="UTF-8"?>
<repomd xmlns="http://linux.duke.edu/metadata/repo" xmlns:rpm="http://linux.duke.edu/metadata/rpm">
  <revision>foobar</revision>
  <tags>
    <content>contenttag</content>
    <repo>repotag</repo>
    <distro cpeid="cpeid2">tag3</distro>
    <distro cpeid="cpeid1">tag2</distro>
    <distro>tag1</distro>
  </tags>
</repomd>
""")

        rec = cr.RepomdRecord(self.path00)
        rec.fill(cr.SHA256)
        rec.timestamp = 1
        md.set_record(rec, "primary")

        xml = md.xml_dump()
        self.assertEqual(xml,
"""<?xml version="1.0" encoding="UTF-8"?>
<repomd xmlns="http://linux.duke.edu/metadata/repo" xmlns:rpm="http://linux.duke.edu/metadata/rpm">
  <revision>foobar</revision>
  <tags>
    <content>contenttag</content>
    <repo>repotag</repo>
    <distro cpeid="cpeid2">tag3</distro>
    <distro cpeid="cpeid1">tag2</distro>
    <distro>tag1</distro>
  </tags>
  <data type="primary">
    <checksum type="sha256">dabe2ce5481d23de1f4f52bdcfee0f9af98316c9e0de2ce8123adeefa0dd08b9</checksum>
    <open-checksum type="sha256">e1e2ffd2fb1ee76f87b70750d00ca5677a252b397ab6c2389137a0c33e7b359f</open-checksum>
    <location href="repodata/primary.xml.gz"/>
    <timestamp>1</timestamp>
    <size>134</size>
    <open-size>167</open-size>
  </data>
</repomd>
""")
