import os.path
import tempfile
import unittest
import shutil
import createrepo_c as cr

from .fixtures import *

class TestCaseMisc(unittest.TestCase):

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="createrepo_ctest-")
        self.nofile = os.path.join(self.tmpdir, "this_file_should_not_exists")
        self.tmpfile = os.path.join(self.tmpdir, "file")
        self.content = "some\nfoo\ncontent\n"
        with open(self.tmpfile, "w") as tmp_file:
            tmp_file.write(self.content)

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_compress_file(self):
        # Non exist file
        self.assertRaises(IOError, cr.compress_file,
                          self.nofile, None, cr.BZ2)

        # Compression - use the same name+suffix
        cr.compress_file(self.tmpfile, None, cr.BZ2)
        self.assertTrue(os.path.isfile(self.tmpfile+".bz2"))

        # Compression - new name
        new_name = os.path.join(self.tmpdir, "foobar.gz")
        cr.compress_file(self.tmpfile, new_name, cr.GZ)
        self.assertTrue(os.path.isfile(new_name))

        # Compression - with stat
        stat = cr.ContentStat(cr.SHA256)
        cr.compress_file(self.tmpfile, None, cr.XZ, stat)
        self.assertTrue(os.path.isfile(self.tmpfile+".xz"))
        self.assertEqual(stat.checksum, "e61ebaa6241e335c779194ce7af98c590f1"\
                                        "b26a749f219b997a0d7d5a773063b")
        self.assertEqual(stat.checksum_type, cr.SHA256)
        self.assertEqual(stat.size, len(self.content))

        # Check directory for unexpected files
        self.assertEqual(set(os.listdir(self.tmpdir)),
                         set(['file.bz2', 'file.xz', 'file', 'foobar.gz']))

    def test_decompress_file(self):
        # Non exist file
        self.assertRaises(IOError, cr.decompress_file,
                          self.nofile, None, cr.BZ2)

        tmpfile_gz_comp = os.path.join(self.tmpdir, "gzipedfile.gz")
        shutil.copy(FILE_TEXT_GZ, tmpfile_gz_comp)
        tmpfile_gz_comp_ns = os.path.join(self.tmpdir, "gzipedfile_no_suffix")
        shutil.copy(FILE_TEXT_GZ, tmpfile_gz_comp_ns)

        # Decompression - use the same name without suffix
        dest = os.path.join(self.tmpdir, "gzipedfile")
        cr.decompress_file(tmpfile_gz_comp, None, cr.GZ)
        self.assertTrue(os.path.isfile(dest))

        # Decompression - use the specific name
        dest = os.path.join(self.tmpdir, "decompressed.file")
        cr.decompress_file(tmpfile_gz_comp, dest, cr.GZ)
        self.assertTrue(os.path.isfile(dest))

        # Decompression - bad suffix by default
        dest = os.path.join(self.tmpdir, "gzipedfile_no_suffix.decompressed")
        cr.decompress_file(tmpfile_gz_comp_ns, None, cr.GZ)
        self.assertTrue(os.path.isfile(dest))

        # Decompression - with stat
        stat = cr.ContentStat(cr.SHA256)
        dest = os.path.join(self.tmpdir, "gzipedfile")
        cr.decompress_file(tmpfile_gz_comp, None, cr.AUTO_DETECT_COMPRESSION, stat)
        self.assertTrue(os.path.isfile(dest))
        self.assertEqual(stat.checksum, FILE_TEXT_SHA256SUM)
        self.assertEqual(stat.checksum_type, cr.SHA256)
        self.assertEqual(stat.size, 910)

