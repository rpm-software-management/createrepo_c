import unittest
import shutil
import tempfile
from deltarepo.deltametadata import DeltaMetadata, PluginBundle

from fixtures import *

XML_EMPTY = """<?xml version='1.0' encoding='UTF-8'?>
<deltametadata>
  <usedplugins/>
</deltametadata>
"""

XML_01 = """<?xml version='1.0' encoding='UTF-8'?>
<deltametadata>
  <revision src="123" dst="456" />
  <contenthash src="abc" dst="bcd" type="foobar" />
  <timestamp src="120" dst="450" />
  <usedplugins>
    <plugin database="1" name="FooBarPlugin" origincompression="gz" version="1">
      <removedpackage base="ftp://foobaar/" href="Pacakges/foo.rpm"/>
      <removedpackage href="Packages/bar.rpm"/>
      <emptylist/>
    </plugin>
  </usedplugins>
</deltametadata>
"""


class TestCasePluginPersistentConfig(unittest.TestCase):

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="deltarepo-test-")

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_dump_empty_deltametadata(self):
        dm = DeltaMetadata()
        content = dm.xmldump()
        self.assertEqual(content, XML_EMPTY)

    def test_dump_deltametadata_01(self):
        plugin = PluginBundle("FooBarPlugin", 1)
        plugin.set("database", "1")
        plugin.set("origincompression", "gz")
        plugin.append("removedpackage", {"href": "Pacakges/foo.rpm",
                                         "base": "ftp://foobaar/"})
        plugin.append("removedpackage", {"href": "Packages/bar.rpm"})
        plugin.append("emptylist", {})

        dm = DeltaMetadata()
        dm.revision_src = "123"
        dm.revision_dst = "456"
        dm.contenthash_src = "abc"
        dm.contenthash_dst = "bcd"
        dm.contenthash_type = "foobar"
        dm.timestamp_src = 120
        dm.timestamp_dst = 450
        dm.add_pluginbundle(plugin)
        content = dm.xmldump()

        path = os.path.join(self.tmpdir, "01.xml")
        open(path, "w").write(content)

        dm_loaded = DeltaMetadata()
        dm_loaded.xmlparse(path)

        self.assertEqual(dm.revision_src, dm_loaded.revision_src)
        self.assertEqual(dm.revision_dst, dm_loaded.revision_dst)
        self.assertEqual(dm.contenthash_src, dm_loaded.contenthash_src)
        self.assertEqual(dm.contenthash_dst, dm_loaded.contenthash_dst)
        self.assertEqual(dm.contenthash_type, dm_loaded.contenthash_type)
        self.assertEqual(dm.timestamp_src, dm_loaded.timestamp_src)
        self.assertEqual(dm.timestamp_dst, dm_loaded.timestamp_dst)
        self.assertEqual(len(dm.usedplugins), len(dm_loaded.usedplugins))
        self.assertEqual(dm.usedplugins["FooBarPlugin"].__dict__,
                         dm_loaded.usedplugins["FooBarPlugin"].__dict__)

    def test_parse_empty_deltametadata(self):
        path = os.path.join(self.tmpdir, "empty.xml")
        open(path, "w").write(XML_EMPTY)

        dm = DeltaMetadata()
        dm.xmlparse(path)

        self.assertEqual(len(dm.usedplugins), 0)

    def test_parse_deltametadata_01(self):
        path = os.path.join(self.tmpdir, "01.xml")
        open(path, "w").write(XML_01)

        dm = DeltaMetadata()
        dm.xmlparse(path)

        self.assertEqual(dm.revision_src, "123")
        self.assertEqual(dm.revision_dst, "456")
        self.assertEqual(dm.contenthash_src, "abc")
        self.assertEqual(dm.contenthash_dst, "bcd")
        self.assertEqual(dm.contenthash_type, "foobar")
        self.assertEqual(dm.timestamp_src, 120)
        self.assertEqual(dm.timestamp_dst, 450)

        self.assertEqual(len(dm.usedplugins), 1)

        bp = dm.usedplugins["FooBarPlugin"]
        self.assertEqual(bp.name, "FooBarPlugin")
        self.assertEqual(bp.version, 1)
        self.assertEqual(bp.get("database"), "1")
        self.assertEqual(bp.get("origincompression"), "gz")
        self.assertEqual(bp.get_list("removedpackage"),
                         [{"href": "Pacakges/foo.rpm", "base": "ftp://foobaar/"},
                          {"href": "Packages/bar.rpm"}])
        self.assertEqual(bp.get_list("emptylist"), [{}])