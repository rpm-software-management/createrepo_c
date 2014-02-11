import os.path
import unittest
from deltarepo.updater_common import LocalRepo, OriginRepo, DRMirror

from .fixtures import *

class TestCaseLocalRepo(unittest.TestCase):
    def localrepo_init(self):
        lr = LocalRepo()
        self.assertTrue(lr)

    def test_localrepo_from_path(self):
        lr = LocalRepo.from_path(REPO_01_PATH)
        self.assertEqual(lr.revision, "1378724582")
        self.assertEqual(lr.timestamp, 1378724581L)
        self.assertEqual(lr.contenthash, "4d1c9f8b7c442adb5f90fda368ec7eb267fa42759a5d125001585bc8928b3967")
        self.assertEqual(lr.contenthash_type, "sha256")

    def test_localrepo_from_path_md5(self):
        lr = LocalRepo.from_path(REPO_01_PATH, "md5")
        self.assertEqual(lr.revision, "1378724582")
        self.assertEqual(lr.timestamp, 1378724581L)
        self.assertEqual(lr.contenthash_type, "md5")
        self.assertEqual(lr.contenthash, "357a4ca1d69f48f2a278158079153211")

class TestCaseOriginRepo(unittest.TestCase):
    def originrepo_init(self):
        lr = OriginRepo()
        self.assertTrue(lr)

    def test_originrepo_from_url(self):
        lr = OriginRepo.from_url(urls=[REPO_01_PATH])
        self.assertEqual(lr.revision, "1378724582")
        self.assertEqual(lr.timestamp, 1378724581L)
        self.assertEqual(lr.contenthash, None)
        self.assertEqual(lr.contenthash_type, None)
        self.assertEqual(lr.urls, [REPO_01_PATH])
        self.assertEqual(lr.mirrorlist, None)
        self.assertEqual(lr.metalink, None)

class TestCaseDRMirror(unittest.TestCase):
    def test_drmirror_init(self):
        drm = DRMirror()
        self.assertTrue(drm)

    def test_drmirror_from_url(self):
        url = "file://" + os.path.abspath(DELTAREPOS_01_PATH)
        drm = DRMirror.from_url(url)
        self.assertTrue(drm)
        self.assertEqual(len(drm.records), 3)
        self.assertTrue(drm.deltarepos)