import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from .fixtures import *

class TestCaseCrFile(unittest.TestCase):

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="createrepo_ctest-")

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_crfile_basic_operations(self):
        f = cr.CrFile(self.tmpdir+"/foo.gz",
                      cr.MODE_WRITE,
                      cr.GZ_COMPRESSION,
                      None)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(self.tmpdir+"/foo.gz"))

    def test_crfile_operations_on_closed_file(self):
        # Already closed file
        path = os.path.join(self.tmpdir, "primary.xml.gz")
        f = cr.CrFile(path, cr.MODE_WRITE, cr.GZ_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.close()

        self.assertRaises(cr.CreaterepoCError, f.write, "foobar")
        f.close() # No error should be raised
        del(f)    # No error should be raised

    def test_crfile_error_cases(self):
        path = os.path.join(self.tmpdir, "foofile")
        self.assertFalse(os.path.exists(path))

        # Bad open mode
        self.assertRaises(ValueError, cr.CrFile, path, 86,
                          cr.GZ_COMPRESSION, None)
        self.assertFalse(os.path.exists(path))

        # Bad compression type
        self.assertRaises(ValueError, cr.CrFile, path,
                          cr.MODE_READ, 678, None)
        self.assertFalse(os.path.exists(path))

        # Bad contentstat object
        self.assertRaises(TypeError, cr.XmlFile, path,
                          cr.MODE_READ, cr.GZ_COMPRESSION, "foo")
        self.assertFalse(os.path.exists(path))

        # Non existing path
        self.assertRaises(IOError, cr.CrFile,
                          "foobar/foo/xxx/cvydmaticxuiowe")

    def test_crfile_no_compression(self):
        path = os.path.join(self.tmpdir, "foo")
        f = cr.CrFile(path, cr.MODE_WRITE, cr.NO_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.write("foobar")
        f.close()

        with open(path) as foo:
            self.assertEqual(foo.read(), "foobar")

    def test_crfile_gz_compression(self):
        path = os.path.join(self.tmpdir, "foo.gz")
        f = cr.CrFile(path, cr.MODE_WRITE, cr.GZ_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.write("foobar")
        f.close()

        import gzip
        with gzip.open(path) as foo_gz:
            self.assertEqual(foo_gz.read().decode('utf-8'), "foobar")

    def test_crfile_bz2_compression(self):
        path = os.path.join(self.tmpdir, "foo.bz2")
        f = cr.CrFile(path, cr.MODE_WRITE, cr.BZ2_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.write("foobar")
        f.close()

        import bz2
        with open(path, 'rb') as foo_bz2:
            content = bz2.decompress(foo_bz2.read()).decode('utf-8')
            self.assertEqual(content, "foobar")

    def test_crfile_xz_compression(self):
        path = os.path.join(self.tmpdir, "foo.xz")
        f = cr.CrFile(path, cr.MODE_WRITE, cr.XZ_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.write("foobar")
        f.close()

        import subprocess
        with subprocess.Popen(["unxz", "--stdout", path], stdout=subprocess.PIPE, close_fds=False) as p:
            content = p.stdout.read().decode('utf-8')
            self.assertEqual(content, "foobar")

    def test_crfile_zck_compression(self):
        if cr.HAS_ZCK == 0:
            return

        path = os.path.join(self.tmpdir, "foo.zck")
        f = cr.CrFile(path, cr.MODE_WRITE, cr.ZCK_COMPRESSION)
        self.assertTrue(f)
        self.assertTrue(os.path.isfile(path))
        f.write("foobar")
        f.close()

        import subprocess
        with subprocess.Popen(["unzck", "--stdout", path], stdout=subprocess.PIPE, close_fds=False) as p:
            content = p.stdout.read().decode('utf-8')
            self.assertEqual(content, "foobar")
