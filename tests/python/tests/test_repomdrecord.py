import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from fixtures import *

class TestCaseRepomdRecord(unittest.TestCase):

    # TODO:
    #  - Test rename_file() for files with checksum

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="createrepo_ctest-")
        self.FN_00 = "primary.xml.gz"
        self.FN_01 = "primary.xml"
        self.path00 = os.path.join(self.tmpdir, self.FN_00)
        self.path01 = os.path.join(self.tmpdir, self.FN_01)

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_repomdrecord_fill(self):
        shutil.copyfile(REPO_00_PRIXML, self.path00)
        self.assertTrue(os.path.exists(self.path00))

        rec = cr.RepomdRecord("primary", self.path00)
        self.assertTrue(rec)

        self.assertEqual(rec.location_real, self.path00)
        self.assertEqual(rec.location_href, "repodata/primary.xml.gz")
        self.assertEqual(rec.location_base, None)
        self.assertEqual(rec.checksum, None)
        self.assertEqual(rec.checksum_type, None)
        self.assertEqual(rec.checksum_open, None)
        self.assertEqual(rec.checksum_open_type, None)
        self.assertEqual(rec.timestamp, 0)
        self.assertEqual(rec.size, 0)
        self.assertEqual(rec.size_open, 0)
        self.assertEqual(rec.db_ver, 0)

        rec.fill(cr.SHA256)

        self.assertEqual(rec.location_real, self.path00)
        self.assertEqual(rec.location_href, "repodata/primary.xml.gz")
        self.assertEqual(rec.location_base, None)
        self.assertEqual(rec.checksum, "dabe2ce5481d23de1f4f52bdcfee0f9af98316c9e0de2ce8123adeefa0dd08b9")
        self.assertEqual(rec.checksum_type, "sha256")
        self.assertEqual(rec.checksum_open, "e1e2ffd2fb1ee76f87b70750d00ca5677a252b397ab6c2389137a0c33e7b359f")
        self.assertEqual(rec.checksum_open_type, "sha256")
        self.assertTrue(rec.timestamp > 0)
        self.assertEqual(rec.size, 134)
        self.assertEqual(rec.size_open, 167)
        self.assertEqual(rec.db_ver, 10)

        rec.rename_file()

        # Filename shoud contain a (valid) checksum
        self.assertEqual(os.listdir(self.tmpdir),
            ['dabe2ce5481d23de1f4f52bdcfee0f9af98316c9e0de2ce8123adeefa0dd08b9-primary.xml.gz'])

    def test_repomdrecord_setters(self):
        shutil.copyfile(REPO_00_PRIXML, self.path00)
        self.assertTrue(os.path.exists(self.path00))

        rec = cr.RepomdRecord("primary", self.path00)
        self.assertTrue(rec)

        rec.fill(cr.SHA256)

        self.assertEqual(rec.type, "primary")
        self.assertEqual(rec.location_real, self.path00)
        self.assertEqual(rec.location_href, "repodata/primary.xml.gz")
        self.assertEqual(rec.checksum, "dabe2ce5481d23de1f4f52bdcfee0f9af98316c9e0de2ce8123adeefa0dd08b9")
        self.assertEqual(rec.checksum_type, "sha256")
        self.assertEqual(rec.checksum_open, "e1e2ffd2fb1ee76f87b70750d00ca5677a252b397ab6c2389137a0c33e7b359f")
        self.assertEqual(rec.checksum_open_type, "sha256")
        self.assertTrue(rec.timestamp > 0)
        self.assertEqual(rec.size, 134)
        self.assertEqual(rec.size_open, 167)
        self.assertEqual(rec.db_ver, 10)

        # Set new values

        rec.type = "foo"
        rec.location_href = "repodata/foo.xml.gz"
        rec.checksum = "foobar11"
        rec.checksum_type = "foo1"
        rec.checksum_open = "foobar22"
        rec.checksum_open_type = "foo2"
        rec.timestamp = 123
        rec.size = 456
        rec.size_open = 789
        rec.db_ver = 11

        # Check

        self.assertEqual(rec.type, "foo")
        self.assertEqual(rec.location_real, self.path00)
        self.assertEqual(rec.location_href, "repodata/foo.xml.gz")
        self.assertEqual(rec.checksum, "foobar11")
        self.assertEqual(rec.checksum_type, "foo1")
        self.assertEqual(rec.checksum_open, "foobar22")
        self.assertEqual(rec.checksum_open_type, "foo2")
        self.assertEqual(rec.timestamp, 123)
        self.assertEqual(rec.size, 456)
        self.assertEqual(rec.size_open, 789)
        self.assertEqual(rec.db_ver, 11)

    def test_repomdrecord_compress_and_fill(self):
        open(self.path01, "w").write("foobar\ncontent\nhh\n")
        self.assertTrue(os.path.exists(self.path01))

        rec = cr.RepomdRecord("primary", self.path01)
        self.assertTrue(rec)
        rec_compressed = rec.compress_and_fill(cr.SHA256, cr.GZ_COMPRESSION)

        # A new compressed file shoud be created
        self.assertEqual(sorted(os.listdir(self.tmpdir)),
            sorted(['primary.xml.gz', 'primary.xml']))

        rec.rename_file()
        rec_compressed.rename_file()

        # Filename shoud contain a (valid) checksum
        self.assertEqual(sorted(os.listdir(self.tmpdir)),
            sorted(['10091f8e2e235ae875cb18c91c443891c7f1a599d41f44d518e8af759a6c8109-primary.xml.gz',
                    'b33fc63178d852333a826385bc15d9b72cb6658be7fb927ec28c4e40b5d426fb-primary.xml']))

    def test_repomdrecord_load_contentstat(self):
        rec = cr.RepomdRecord("primary", None)
        self.assertTrue(rec)

        stat = cr.ContentStat(cr.SHA256)
        stat.checksum = "foobar"
        stat.checksum_type = cr.SHA256
        stat.size = 123

        self.assertEqual(rec.checksum_open, None)
        self.assertEqual(rec.checksum_open_type, None)
        self.assertEqual(rec.size, 0)

        rec.load_contentstat(stat);

        self.assertEqual(rec.checksum_open, "foobar")
        self.assertEqual(rec.checksum_open_type, "sha256")
        self.assertEqual(rec.size_open, 123)

