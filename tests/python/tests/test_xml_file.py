import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from .fixtures import *

class TestCaseXmlFile(unittest.TestCase):

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="createrepo_ctest-")

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_xmlfile_basic_operations(self):
        pri = cr.XmlFile(self.tmpdir+"/primary.xml.gz",
                         cr.XMLFILE_PRIMARY,
                         cr.GZ_COMPRESSION,
                         None)
        self.assertTrue(pri)
        self.assertTrue(os.path.isfile(self.tmpdir+"/primary.xml.gz"))

        fil = cr.XmlFile(self.tmpdir+"/filelists.xml.gz",
                         cr.XMLFILE_FILELISTS,
                         cr.GZ_COMPRESSION,
                         None)
        self.assertTrue(fil)
        self.assertTrue(os.path.isfile(self.tmpdir+"/filelists.xml.gz"))

        oth = cr.XmlFile(self.tmpdir+"/other.xml.gz",
                         cr.XMLFILE_OTHER,
                         cr.GZ_COMPRESSION,
                         None)
        self.assertTrue(oth)
        self.assertTrue(os.path.isfile(self.tmpdir+"/other.xml.gz"))

    def test_xmlfile_operations_on_closed_file(self):
        # Already closed file
        path = os.path.join(self.tmpdir, "primary.xml.gz")
        pkg = cr.package_from_rpm(PKG_ARCHER_PATH)
        self.assertTrue(pkg)

        f = cr.PrimaryXmlFile(path, cr.GZ_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.close()
        self.assertRaises(cr.CreaterepoCError, f.set_num_of_pkgs, 1)
        self.assertRaises(cr.CreaterepoCError, f.add_pkg, pkg)
        self.assertRaises(cr.CreaterepoCError, f.add_chunk, "<chunk>text</chunk>")
        self.assertEqual("<createrepo_c.XmlFile Closed object>", f.__str__())
        f.close() # No error should be raised
        del(f)    # No error should be raised

    def test_xmlfile_error_cases(self):
        path = os.path.join(self.tmpdir, "foofile")
        self.assertFalse(os.path.exists(path))

        # Bad file type
        self.assertRaises(ValueError, cr.XmlFile, path, 86,
                          cr.GZ_COMPRESSION, None)
        self.assertFalse(os.path.exists(path))

        # Bad compression type
        self.assertRaises(ValueError, cr.XmlFile, path,
                          cr.XMLFILE_PRIMARY, 678, None)
        self.assertFalse(os.path.exists(path))

        # Bad contentstat object
        self.assertRaises(TypeError, cr.XmlFile, path,
                          cr.XMLFILE_PRIMARY, cr.GZ_COMPRESSION, "foo")
        self.assertFalse(os.path.exists(path))

        # Non existing path
        self.assertRaises(cr.CreaterepoCError, cr.PrimaryXmlFile,
                          "foobar/foo/xxx/cvydmaticxuiowe")

        # Already existing file
        with open(path, "w") as foofile:
            foofile.write("foobar")
        self.assertRaises(IOError, cr.PrimaryXmlFile, path)

    def test_xmlfile_no_compression(self):
        path = os.path.join(self.tmpdir, "primary.xml")
        f = cr.PrimaryXmlFile(path, cr.NO_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.close()

        with open(path) as primary:
            self.assertEqual(primary.read(),
"""<?xml version="1.0" encoding="UTF-8"?>
<metadata xmlns="http://linux.duke.edu/metadata/common" xmlns:rpm="http://linux.duke.edu/metadata/rpm" packages="0">
</metadata>""")

    def test_xmlfile_gz_compression(self):
        path = os.path.join(self.tmpdir, "primary.xml.gz")
        f = cr.PrimaryXmlFile(path, cr.GZ_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.close()

        import gzip
        with gzip.open(path) as primary:
            self.assertEqual(primary.read().decode('utf-8'),
"""<?xml version="1.0" encoding="UTF-8"?>
<metadata xmlns="http://linux.duke.edu/metadata/common" xmlns:rpm="http://linux.duke.edu/metadata/rpm" packages="0">
</metadata>""")

    def test_xmlfile_bz2_compression(self):
        path = os.path.join(self.tmpdir, "primary.xml.bz2")
        f = cr.PrimaryXmlFile(path, cr.BZ2_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.close()

        import bz2
        with open(path, 'rb') as primary_bz2:
            content = bz2.decompress(primary_bz2.read()).decode('utf-8')
            self.assertEqual(content,
"""<?xml version="1.0" encoding="UTF-8"?>
<metadata xmlns="http://linux.duke.edu/metadata/common" xmlns:rpm="http://linux.duke.edu/metadata/rpm" packages="0">
</metadata>""")

    def test_xmlfile_xz_compression(self):
        path = os.path.join(self.tmpdir, "primary.xml.xz")
        f = cr.PrimaryXmlFile(path, cr.XZ_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.close()

        import subprocess
        with subprocess.Popen(["unxz", "--stdout", path], stdout=subprocess.PIPE) as p:
            content = p.stdout.read().decode('utf-8')
            self.assertEqual(content,
"""<?xml version="1.0" encoding="UTF-8"?>
<metadata xmlns="http://linux.duke.edu/metadata/common" xmlns:rpm="http://linux.duke.edu/metadata/rpm" packages="0">
</metadata>""")

    def test_xmlfile_zck_compression(self):
        if cr.HAS_ZCK == 0:
            return

        path = os.path.join(self.tmpdir, "primary.xml.zck")
        f = cr.PrimaryXmlFile(path, cr.ZCK_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.close()

        import subprocess
        with subprocess.Popen(["unzck", "--stdout", path], stdout=subprocess.PIPE) as p:
            content = p.stdout.read().decode('utf-8')
            self.assertEqual(content,
"""<?xml version="1.0" encoding="UTF-8"?>
<metadata xmlns="http://linux.duke.edu/metadata/common" xmlns:rpm="http://linux.duke.edu/metadata/rpm" packages="0">
</metadata>""")

    def test_xmlfile_set_num_of_pkgs(self):
        path = os.path.join(self.tmpdir, "primary.xml")
        f = cr.PrimaryXmlFile(path, cr.NO_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.set_num_of_pkgs(22)
        f.close()

        with open(path) as primary:
            self.assertEqual(primary.read(),
"""<?xml version="1.0" encoding="UTF-8"?>
<metadata xmlns="http://linux.duke.edu/metadata/common" xmlns:rpm="http://linux.duke.edu/metadata/rpm" packages="22">
</metadata>""")

    def test_xmlfile_add_pkg(self):
        pkg = cr.package_from_rpm(PKG_ARCHER_PATH)
        self.assertTrue(pkg)
        pkg.time_file = 111

        # Primary
        path = os.path.join(self.tmpdir, "primary.xml")
        f = cr.PrimaryXmlFile(path, cr.NO_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.add_pkg(pkg)
        self.assertRaises(TypeError, f.add_pkg, None)
        self.assertRaises(TypeError, f.add_pkg, 123)
        self.assertRaises(TypeError, f.add_pkg, "foo")
        self.assertRaises(TypeError, f.add_pkg, [456])
        f.close()

        self.assertTrue(os.path.isfile(path))
        with open(path) as primary:
            self.assertEqual(primary.read(),
"""<?xml version="1.0" encoding="UTF-8"?>
<metadata xmlns="http://linux.duke.edu/metadata/common" xmlns:rpm="http://linux.duke.edu/metadata/rpm" packages="0">
<package type="rpm">
  <name>Archer</name>
  <arch>x86_64</arch>
  <version epoch="2" ver="3.4.5" rel="6"/>
  <checksum type="sha256" pkgid="YES">4e0b775220c67f0f2c1fd2177e626b9c863a098130224ff09778ede25cea9a9e</checksum>
  <summary>Complex package.</summary>
  <description>Archer package</description>
  <packager>Sterling Archer</packager>
  <url>http://soo_complex_package.eu/</url>
  <time file="111" build="1365416480"/>
  <size package="3101" installed="0" archive="544"/>
  <location href=""/>
  <format>
    <rpm:license>GPL</rpm:license>
    <rpm:vendor>ISIS</rpm:vendor>
    <rpm:group>Development/Tools</rpm:group>
    <rpm:buildhost>localhost.localdomain</rpm:buildhost>
    <rpm:sourcerpm>Archer-3.4.5-6.src.rpm</rpm:sourcerpm>
    <rpm:header-range start="280" end="2865"/>
    <rpm:provides>
      <rpm:entry name="bara" flags="LE" epoch="0" ver="22"/>
      <rpm:entry name="barb" flags="GE" epoch="0" ver="11.22.33" rel="44"/>
      <rpm:entry name="barc" flags="EQ" epoch="0" ver="33"/>
      <rpm:entry name="bard" flags="LT" epoch="0" ver="44"/>
      <rpm:entry name="bare" flags="GT" epoch="0" ver="55"/>
      <rpm:entry name="Archer" flags="EQ" epoch="2" ver="3.4.5" rel="6"/>
      <rpm:entry name="Archer(x86-64)" flags="EQ" epoch="2" ver="3.4.5" rel="6"/>
    </rpm:provides>
    <rpm:requires>
      <rpm:entry name="fooa" flags="LE" epoch="0" ver="2"/>
      <rpm:entry name="foob" flags="GE" epoch="0" ver="1.0.0" rel="1"/>
      <rpm:entry name="fooc" flags="EQ" epoch="0" ver="3"/>
      <rpm:entry name="food" flags="LT" epoch="0" ver="4"/>
      <rpm:entry name="fooe" flags="GT" epoch="0" ver="5"/>
      <rpm:entry name="foof" flags="EQ" epoch="0" ver="6" pre="1"/>
    </rpm:requires>
    <rpm:conflicts>
      <rpm:entry name="bba" flags="LE" epoch="0" ver="2222"/>
      <rpm:entry name="bbb" flags="GE" epoch="0" ver="1111.2222.3333" rel="4444"/>
      <rpm:entry name="bbc" flags="EQ" epoch="0" ver="3333"/>
      <rpm:entry name="bbd" flags="LT" epoch="0" ver="4444"/>
      <rpm:entry name="bbe" flags="GT" epoch="0" ver="5555"/>
    </rpm:conflicts>
    <rpm:obsoletes>
      <rpm:entry name="aaa" flags="LE" epoch="0" ver="222"/>
      <rpm:entry name="aab" flags="GE" epoch="0" ver="111.2.3" rel="4"/>
      <rpm:entry name="aac" flags="EQ" epoch="0" ver="333"/>
      <rpm:entry name="aad" flags="LT" epoch="0" ver="444"/>
      <rpm:entry name="aae" flags="GT" epoch="0" ver="555"/>
    </rpm:obsoletes>
    <file>/usr/bin/complex_a</file>
  </format>
</package>
</metadata>""")

        # Filelists
        path = os.path.join(self.tmpdir, "filelists.xml")
        f = cr.FilelistsXmlFile(path, cr.NO_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.add_pkg(pkg)
        self.assertRaises(TypeError, f.add_pkg, None)
        self.assertRaises(TypeError, f.add_pkg, 123)
        self.assertRaises(TypeError, f.add_pkg, "foo")
        self.assertRaises(TypeError, f.add_pkg, [456])
        f.close()

        self.assertTrue(os.path.isfile(path))
        with open(path) as filelists:
            self.assertEqual(filelists.read(),
"""<?xml version="1.0" encoding="UTF-8"?>
<filelists xmlns="http://linux.duke.edu/metadata/filelists" packages="0">
<package pkgid="4e0b775220c67f0f2c1fd2177e626b9c863a098130224ff09778ede25cea9a9e" name="Archer" arch="x86_64">
  <version epoch="2" ver="3.4.5" rel="6"/>
  <file>/usr/bin/complex_a</file>
  <file type="dir">/usr/share/doc/Archer-3.4.5</file>
  <file>/usr/share/doc/Archer-3.4.5/README</file>
</package>
</filelists>""")

        # Other
        path = os.path.join(self.tmpdir, "other.xml")
        f = cr.OtherXmlFile(path, cr.NO_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.add_pkg(pkg)
        self.assertRaises(TypeError, f.add_pkg, None)
        self.assertRaises(TypeError, f.add_pkg, 123)
        self.assertRaises(TypeError, f.add_pkg, "foo")
        self.assertRaises(TypeError, f.add_pkg, [456])
        f.close()

        self.assertTrue(os.path.isfile(path))
        with open(path) as other:
            self.assertEqual(other.read(),
"""<?xml version="1.0" encoding="UTF-8"?>
<otherdata xmlns="http://linux.duke.edu/metadata/other" packages="0">
<package pkgid="4e0b775220c67f0f2c1fd2177e626b9c863a098130224ff09778ede25cea9a9e" name="Archer" arch="x86_64">
  <version epoch="2" ver="3.4.5" rel="6"/>
  <changelog author="Tomas Mlcoch &lt;tmlcoch@redhat.com&gt; - 1.1.1-1" date="1334664000">- First changelog.</changelog>
  <changelog author="Tomas Mlcoch &lt;tmlcoch@redhat.com&gt; - 2.2.2-2" date="1334750400">- That was totally ninja!</changelog>
  <changelog author="Tomas Mlcoch &lt;tmlcoch@redhat.com&gt; - 3.3.3-3" date="1365422400">- 3. changelog.</changelog>
</package>
</otherdata>""")

    def test_xmlfile_add_chunk(self):
        chunk = "  <chunk>Some XML chunk</chunk>\n"

        # Primary
        path = os.path.join(self.tmpdir, "primary.xml")
        f = cr.PrimaryXmlFile(path, cr.NO_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.add_chunk(chunk)
        self.assertRaises(TypeError, f.add_chunk, None)
        self.assertRaises(TypeError, f.add_chunk, 123)
        self.assertRaises(TypeError, f.add_chunk, [1])
        self.assertRaises(TypeError, f.add_chunk, ["foo"])
        f.close()

        self.assertTrue(os.path.isfile(path))
        with open(path) as primary:
            self.assertEqual(primary.read(),
"""<?xml version="1.0" encoding="UTF-8"?>
<metadata xmlns="http://linux.duke.edu/metadata/common" xmlns:rpm="http://linux.duke.edu/metadata/rpm" packages="0">
  <chunk>Some XML chunk</chunk>
</metadata>""")

        # Filelists
        path = os.path.join(self.tmpdir, "filelists.xml")
        f = cr.FilelistsXmlFile(path, cr.NO_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.add_chunk(chunk)
        self.assertRaises(TypeError, f.add_chunk, None)
        self.assertRaises(TypeError, f.add_chunk, 123)
        self.assertRaises(TypeError, f.add_chunk, [1])
        self.assertRaises(TypeError, f.add_chunk, ["foo"])
        f.close()

        self.assertTrue(os.path.isfile(path))
        with open(path) as filelists:
            self.assertEqual(filelists.read(),
"""<?xml version="1.0" encoding="UTF-8"?>
<filelists xmlns="http://linux.duke.edu/metadata/filelists" packages="0">
  <chunk>Some XML chunk</chunk>
</filelists>""")

        # Other
        path = os.path.join(self.tmpdir, "other.xml")
        f = cr.OtherXmlFile(path, cr.NO_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.add_chunk(chunk)
        self.assertRaises(TypeError, f.add_chunk, None)
        self.assertRaises(TypeError, f.add_chunk, 123)
        self.assertRaises(TypeError, f.add_chunk, [1])
        self.assertRaises(TypeError, f.add_chunk, ["foo"])
        f.close()

        self.assertTrue(os.path.isfile(path))
        with open(path) as other:
            self.assertEqual(other.read(),
"""<?xml version="1.0" encoding="UTF-8"?>
<otherdata xmlns="http://linux.duke.edu/metadata/other" packages="0">
  <chunk>Some XML chunk</chunk>
</otherdata>""")
