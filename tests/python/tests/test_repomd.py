import re
import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from .fixtures import *

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
        shutil.copyfile(REPO_00_PRIXML, self.path00)
        self.assertTrue(os.path.exists(self.path00))

        rec = cr.RepomdRecord("primary", self.path00)
        self.assertTrue(rec)
        rec.fill(cr.SHA256)
        rec.rename_file()

        # Filename should contain a (valid) checksum
        self.assertEqual(os.listdir(self.tmpdir),
            ['1cb61ea996355add02b1426ed4c1780ea75ce0c04c5d1107c025c3fbd7d8bcae-primary.xml.gz'])

    def test_repomd(self):
        shutil.copyfile(REPO_00_PRIXML, self.path00)
        self.assertTrue(os.path.exists(self.path00))

        md = cr.Repomd()
        self.assertTrue(md)

        xml = md.xml_dump()
        # Revision should be current Unix time
        self.assertTrue(re.search(r"<revision>[0-9]+</revision>", xml))

        self.assertEqual(md.revision, None)
        md.set_revision("foobar")
        self.assertEqual(md.revision, "foobar")

        self.assertEqual(md.repoid, None);
        md.set_repoid("barid", "sha256")
        self.assertEqual(md.repoid, "barid")

        self.assertEqual(md.contenthash, None);
        md.set_contenthash("fooid", "sha256")
        self.assertEqual(md.contenthash, "fooid")

        self.assertEqual(md.distro_tags, [])
        md.add_distro_tag("tag1")
        md.add_distro_tag("tag2", "cpeid1")
        md.add_distro_tag("tag3", cpeid="cpeid2")
        self.assertEqual(md.distro_tags,
                [(None, 'tag1'),
                 ('cpeid1', 'tag2'),
                 ('cpeid2', 'tag3')])

        self.assertEqual(md.repo_tags, [])
        md.add_repo_tag("repotag")
        self.assertEqual(md.repo_tags, ['repotag'])

        self.assertEqual(md.content_tags, [])
        md.add_content_tag("contenttag")
        self.assertEqual(md.content_tags, ['contenttag'])

        self.assertEqual(md.records, [])

        xml = md.xml_dump()
        self.assertEqual(xml,
"""<?xml version="1.0" encoding="UTF-8"?>
<repomd xmlns="http://linux.duke.edu/metadata/repo" xmlns:rpm="http://linux.duke.edu/metadata/rpm">
  <revision>foobar</revision>
  <repoid type="sha256">barid</repoid>
  <contenthash type="sha256">fooid</contenthash>
  <tags>
    <content>contenttag</content>
    <repo>repotag</repo>
    <distro>tag1</distro>
    <distro cpeid="cpeid1">tag2</distro>
    <distro cpeid="cpeid2">tag3</distro>
  </tags>
</repomd>
""")

        rec = cr.RepomdRecord("primary", self.path00)
        rec.fill(cr.SHA256)
        rec.timestamp = 1
        rec.location_base = "http://foo/"
        md.set_record(rec)

        self.assertEqual(len(md.records), 1)

        md.set_record(rec)

        self.assertEqual(len(md.records), 1)

        md.repoid = None
        md.contenthash = None

        xml = md.xml_dump()
        self.assertEqual(xml,
"""<?xml version="1.0" encoding="UTF-8"?>
<repomd xmlns="http://linux.duke.edu/metadata/repo" xmlns:rpm="http://linux.duke.edu/metadata/rpm">
  <revision>foobar</revision>
  <tags>
    <content>contenttag</content>
    <repo>repotag</repo>
    <distro>tag1</distro>
    <distro cpeid="cpeid1">tag2</distro>
    <distro cpeid="cpeid2">tag3</distro>
  </tags>
  <data type="primary">
    <checksum type="sha256">1cb61ea996355add02b1426ed4c1780ea75ce0c04c5d1107c025c3fbd7d8bcae</checksum>
    <open-checksum type="sha256">e1e2ffd2fb1ee76f87b70750d00ca5677a252b397ab6c2389137a0c33e7b359f</open-checksum>
    <location href="repodata/primary.xml.gz" xml:base="http://foo/"/>
    <timestamp>1</timestamp>
    <size>134</size>
    <open-size>167</open-size>
  </data>
</repomd>
""")

    def test_repomd_with_path_in_constructor_repo01(self):

        repomd = cr.Repomd(REPO_01_REPOMD)
        self.assertEqual(repomd.revision, "1334667230")
        self.assertEqual(repomd.repo_tags, [])
        self.assertEqual(repomd.distro_tags, [])
        self.assertEqual(repomd.content_tags, [])
        self.assertEqual(len(repomd.records), 3)

    def test_repomd_indexing_and_iteration_repo01(self):
        repomd = cr.Repomd(REPO_01_REPOMD)

        types = []
        for rec in repomd:
            types.append(rec.type)
        self.assertEqual(types, ['filelists', 'other', 'primary'])

        rec = repomd["primary"]
        self.assertEqual(rec.type, "primary")

        self.assertRaises(KeyError, repomd.__getitem__, "foobar")

        self.assertTrue("primary" in repomd)
