import unittest
import shutil
import tempfile
from deltarepo.deltarepos import DeltaRepos, DeltaReposRecord

from .fixtures import *

XML_EMPTY = """<?xml version="1.0" encoding="UTF-8"?>
<deltarepos>
</deltarepos>
"""

XML_01 = """<?xml version="1.0" encoding="UTF-8"?>
<deltarepos>
  <deltarepo>
    <!-- <plugin name="MainDeltaPlugin" contenthash_type="sha256" dst_contenthash="043fds4red" src_contenthash="ei7as764ly"/> -->
    <location href="deltarepos/ei7as764ly-043fds4red" />
    <size total="15432" />
    <revision src="1387077123" dst="1387087456" />
    <contenthash src="a" dst="b" type="md5" />
    <timestamp src="1387075111" dst="1387086222" />
    <repomd>
      <timestamp>123456789</timestamp>
      <size>963</size>
      <checksum type="sha256">foobarchecksum</checksum>
    </repomd>
  </deltarepo>
</deltarepos>
"""


class TestCaseDeltaRepos(unittest.TestCase):

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="deltarepo-test-")

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_dump_empty_deltarepos(self):
        dr = DeltaRepos()
        content = dr.xmldump()

        path = os.path.join(self.tmpdir, "01.xml")
        open(path, "w").write(content)

        dr = DeltaRepos()
        dr.xmlparse(path)

        self.assertEqual(len(dr.records), 0)

    def test_dump_deltarepos_01(self):
        rec = DeltaReposRecord()

        rec.location_href = "deltarepos/ei7as764ly-043fds4red"
        rec.size_total = 15432
        rec.revision_src = "1387077123"
        rec.revision_dst = "1387086456"
        rec.contenthash_src = "a"
        rec.contenthash_dst = "b"
        rec.contenthash_type = "md5"
        rec.timestamp_src = 1387075111
        rec.timestamp_dst = 1387086222

        rec.repomd_timestamp = 123456789
        rec.repomd_size = 963
        rec.repomd_checksums = [("sha256", "foobarchecksum")]

        #rec.add_plugin('MainDeltaPlugin', {'src_contenthash': 'ei7as764ly',
        #                                   'dst_contenthash': '043fds4red',
        #                                   'contenthash_type': 'sha256'})

        dr = DeltaRepos()
        dr.add_record(rec)
        content = dr.xmldump()

        path = os.path.join(self.tmpdir, "01.xml")
        open(path, "w").write(content)

        dr = DeltaRepos()
        dr.xmlparse(path)

        self.assertEqual(len(dr.records), 1)
        self.assertEqual(dr.records[0].__dict__, rec.__dict__)

    def test_parse_empty_deltarepos(self):
        path = os.path.join(self.tmpdir, "empty.xml")
        open(path, "w").write(XML_EMPTY)

        dr = DeltaRepos()
        dr.xmlparse(path)

        self.assertEqual(len(dr.records), 0)

    def test_parse_deltarepos_01(self):
        path = os.path.join(self.tmpdir, "01.xml")
        open(path, "w").write(XML_01)

        dr = DeltaRepos()
        dr.xmlparse(path)

        self.assertEqual(len(dr.records), 1)

        rec = dr.records[0]

        self.assertEqual(rec.location_base, None)
        self.assertEqual(rec.location_href, "deltarepos/ei7as764ly-043fds4red")
        self.assertEqual(rec.size_total, 15432)
        self.assertEqual(rec.revision_src, "1387077123")
        self.assertEqual(rec.revision_dst, "1387087456")
        self.assertEqual(rec.timestamp_src, 1387075111)
        self.assertEqual(rec.timestamp_dst, 1387086222)

        #self.assertEqual(len(rec.plugins), 1)
        #self.assertTrue("MainDeltaPlugin" in rec.plugins)
        #plugin = rec.plugins["MainDeltaPlugin"]
        #self.assertEqual(plugin, {'name': 'MainDeltaPlugin',
        #                          'src_contenthash': 'ei7as764ly',
        #                          'dst_contenthash': '043fds4red',
        #                          'contenthash_type': 'sha256'})