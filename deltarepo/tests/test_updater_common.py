import os.path
import logging
import unittest
from deltarepo.updater_common import LocalRepo, OriginRepo, DRMirror, Solver

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

class TestCaseSolver(unittest.TestCase):

    class LinkMock(object):
        def __init__(self, src, dst, type="sha256", mirrorurl="mockedlink", cost=100):
            self.src = src
            self.dst = dst
            self.type = type
            self.mirrorurl = mirrorurl
            self._cost = cost

        def cost(self):
            return self._cost

    def test_solver_graph_build(self):

        links = []
        links.append(TestCaseSolver.LinkMock("aaa", "bbb"))
        links.append(TestCaseSolver.LinkMock("aaa", "ccc"))
        links.append(TestCaseSolver.LinkMock("bbb", "ccc"))

        logger = logging.getLogger("testloger")
        graph = Solver.Graph().graph_from_links(links, logger)

        self.assertTrue(graph)
        self.assertEqual(len(graph.nodes), 3)
        self.assertTrue("aaa" in graph.nodes)
        self.assertTrue("bbb" in graph.nodes)
        self.assertTrue("ccc" in graph.nodes)

        self.assertEqual(len(graph.nodes["aaa"].targets), 2)
        self.assertEqual(len(graph.nodes["bbb"].targets), 1)
        self.assertEqual(len(graph.nodes["ccc"].targets), 0)

        self.assertEqual(len(graph.nodes["aaa"].sources), 0)
        self.assertEqual(len(graph.nodes["bbb"].sources), 1)
        self.assertEqual(len(graph.nodes["ccc"].sources), 2)

    def test_solver(self):

        links = []
        links.append(TestCaseSolver.LinkMock("aaa", "bbb"))
        links.append(TestCaseSolver.LinkMock("aaa", "ccc"))
        links.append(TestCaseSolver.LinkMock("bbb", "ccc"))

        logger = logging.getLogger("testloger")
        solver = Solver(links, "aaa", "bbb", logger=logger)
        solver.solve()