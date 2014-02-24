import os.path
import logging
import unittest
from deltarepo.updater_common import LocalRepo, OriginRepo, DRMirror, Solver, UpdateSolver
from deltarepo.errors import DeltaRepoError

from .fixtures import *

class LinkMock(object):
    """Mock object"""
    def __init__(self, src, dst, type="sha256", mirrorurl="mockedlink", cost=100):
        self.src = src
        self.dst = dst
        self.type = type
        self.contenthash_src = src
        self.contenthash_dst = dst
        self.contenthash_type = type
        self.mirrorurl = mirrorurl
        self._cost = cost

        # User can set these remaining values by yourself
        self.revision_src = None
        self.revision_dst = None
        self.timestamp_src = None   # Integer expected here
        self.timestamp_dst = None   # Integer expected here

    def __repr__(self):
        return "<LinkMock \'{0}\'->\'{1}\' ({2})>".format(
            self.src, self.dst, self.cost())

    def cost(self, whitelisted_metadata=None):
        return self._cost

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
        lr = LocalRepo.from_path(REPO_01_PATH, contenthash_type="md5")
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

    def test_originrepo_from_local_repomd(self):
        lr = OriginRepo.from_local_repomd(os.path.join(REPO_01_PATH, "repodata/repomd.xml"))
        self.assertEqual(lr.revision, "1378724582")
        self.assertEqual(lr.timestamp, 1378724581L)
        self.assertEqual(lr.contenthash, None)
        self.assertEqual(lr.contenthash_type, None)
        self.assertEqual(lr.urls, [])
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

    def path_to_strlist(self, resolved_path):
        path = [x.src for x in resolved_path]
        path.append(resolved_path[-1].dst)
        return path

    def test_solver_graph_build(self):
        links = []
        links.append(LinkMock("aaa", "bbb"))
        links.append(LinkMock("aaa", "ccc"))
        links.append(LinkMock("bbb", "ccc"))

        logger = logging.getLogger("testloger")
        graph = Solver.Graph()
        graph.graph_from_links(links)

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

    def test_solver_01(self):
        links = []
        links.append(LinkMock("aaa", "bbb"))
        links.append(LinkMock("aaa", "ccc"))
        links.append(LinkMock("bbb", "ccc"))

        logger = logging.getLogger("testloger")
        solver = Solver(links, "aaa", "ccc", logger=logger)
        resolved_path = solver.solve()
        self.assertTrue(resolved_path)
        self.assertTrue(len(resolved_path), 1)
        self.assertEqual(self.path_to_strlist(resolved_path),
                         ["aaa", "ccc"])

    def test_solver_02(self):
        links = []
        links.append(LinkMock("aaa", "bbb"))
        links.append(LinkMock("bbb", "ccc"))

        logger = logging.getLogger("testloger")
        solver = Solver(links, "aaa", "ccc", logger=logger)
        resolved_path = solver.solve()
        self.assertTrue(resolved_path)
        self.assertEqual(self.path_to_strlist(resolved_path),
                         ["aaa", "bbb", "ccc"])

    def test_solver_03(self):
        links = []
        links.append(LinkMock("aaa", "bbb"))
        links.append(LinkMock("bbb", "ccc"))
        links.append(LinkMock("aaa", "ccc", cost=1000))

        logger = logging.getLogger("testloger")
        solver = Solver(links, "aaa", "ccc", logger=logger)
        resolved_path = solver.solve()
        self.assertTrue(resolved_path)
        self.assertEqual(self.path_to_strlist(resolved_path),
                         ["aaa", "bbb", "ccc"])

    def test_solver_04(self):
        links = []
        links.append(LinkMock("aaa", "bbb"))
        links.append(LinkMock("bbb", "aaa"))
        links.append(LinkMock("bbb", "ccc"))
        links.append(LinkMock("ccc", "bbb"))

        logger = logging.getLogger("testloger")
        solver = Solver(links, "aaa", "ccc", logger=logger)
        resolved_path = solver.solve()
        self.assertTrue(resolved_path)
        self.assertEqual(self.path_to_strlist(resolved_path),
                         ["aaa", "bbb", "ccc"])

    def test_solver_shouldfail_01(self):
        links = []
        links.append(LinkMock("aaa", "bbb"))
        links.append(LinkMock("ccc", "ddd"))

        logger = logging.getLogger("testloger")
        solver = Solver(links, "aaa", "ccc", logger=logger)
        resolved_path = solver.solve()
        self.assertFalse(resolved_path)

    def test_solver_shouldfail_02(self):
        links = []
        links.append(LinkMock("aaa", "bbb"))
        logger = logging.getLogger("testloger")
        solver = Solver(links, "aaa", "ccc", logger=logger)
        self.assertRaises(DeltaRepoError, solver.solve)

    def test_solver_shouldfail_03(self):
        links = []
        links.append(LinkMock("aaa", "bbb"))
        logger = logging.getLogger("testloger")
        solver = Solver(links, "ccc", "aaa", logger=logger)
        self.assertRaises(DeltaRepoError, solver.solve)

class TestCaseUpdateSolver(unittest.TestCase):

    def test_updatesolver_resolve_path(self):
        links = []
        links.append(LinkMock("aaa", "bbb"))
        links.append(LinkMock("bbb", "ccc"))

        updatesolver = UpdateSolver([])
        updatesolver._links = links

        resolved_path = updatesolver.resolve_path("aaa", "ccc")
        self.assertTrue(resolved_path)
        self.assertEqual(len(resolved_path), 2)

    def test_updatesolver_find_repo_contenthash(self):
        links = []
        link = LinkMock("aaa", "bbb")
        link.revision_src = "aaa_rev"
        link.revision_dst = "bbb_rev"
        link.timestamp_src = 111
        link.timestamp_dst = 222
        links.append(link)

        updatesolver = UpdateSolver([])
        updatesolver._links = links

        repo = LocalRepo()

        repo.revision = "aaa_rev"
        repo.timestamp = 111
        type, hash = updatesolver.find_repo_contenthash(repo)
        self.assertEqual(type, "sha256")
        self.assertEqual(hash, "aaa")

        repo.revision = "bbb_rev"
        repo.timestamp = 222
        type, hash = updatesolver.find_repo_contenthash(repo)
        self.assertEqual(type, "sha256")
        self.assertEqual(hash, "bbb")

        repo.revision = "aaa_rev"
        repo.timestamp = 222
        type, hash = updatesolver.find_repo_contenthash(repo)
        self.assertEqual(type, "sha256")
        self.assertEqual(hash, None)

        repo.revision = "ccc_rev"
        repo.timestamp = 111
        type, hash = updatesolver.find_repo_contenthash(repo)
        self.assertEqual(type, "sha256")
        self.assertEqual(hash, None)

        repo.revision = "aaa_rev"
        repo.timestamp = 111
        type, hash = updatesolver.find_repo_contenthash(repo, contenthash_type="md5")
        self.assertEqual(type, "md5")
        self.assertEqual(hash, None)
