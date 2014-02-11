import shutil
import os
import os.path
import librepo
import tempfile
import createrepo_c as cr
from .deltarepos import DeltaRepos
from .common import calculate_contenthash
from .errors import DeltaRepoError

class _Repo(object):
    """Base class for LocalRepo and OriginRepo classes."""

    def __init__ (self):
        self.path = None
        self.timestamp = None
        self.revision = None
        self.contenthash = None
        self.contenthash_type = None
        self.present_metadata = []  # ["primary", "filelists", ...]
        #self.repomd = None # createrepo_c.Repomd() object

    def _fill_from_path(self, path, contenthash=True, contenthash_type="sha256"):
        """Fill the repo attributes from a repository specified by path.
        @param path         path to repository (a dir that contains
                            repodata/ subdirectory)
        @param contenthash  calculate content hash? (primary metadata must
                            be available in the repo)
        @param contenthash_type type of the calculated content hash
        """

        if not os.path.isdir(path) or \
           not os.path.isdir(os.path.join(path, "repodata/")) or \
           not os.path.isfile(os.path.join(path, "repodata/repomd.xml")):
            raise DeltaRepoError("Not a repository: {0}".format(path))

        repomd_path = os.path.join(path, "repodata/repomd.xml")
        repomd = cr.Repomd(repomd_path)

        timestamp = -1
        primary_path = None
        present_metadata = []
        for rec in repomd.records:
            present_metadata.append(rec.type)
            if rec.timestamp:
                timestamp = max(timestamp, rec.timestamp)
            if rec.type == "primary":
                primary_path = rec.location_href

        if not primary_path:
            raise DeltaRepoError("{0} - primary metadata are missing"
                                 "".format(primary_path))

        primary_path = os.path.join(path, primary_path)
        if contenthash and os.path.isfile(primary_path):
            self.contenthash = calculate_contenthash(primary_path, contenthash_type)
            self.contenthash_type = contenthash_type

        self.path = path
        self.revision = repomd.revision
        self.timestamp = timestamp
        self.present_metadata = present_metadata

class LocalRepo(_Repo):
    def __init__ (self):
        _Repo.__init__(self)

    @classmethod
    def from_path(cls, path, contenthash_type="sha256"):
        """Create a LocalRepo object from a path to the repo."""
        lr = cls()
        lr._fill_from_path(path, contenthash_type=contenthash_type)
        return lr

class OriginRepo(_Repo):
    def __init__ (self):
        _Repo.__init__(self)

        self.urls = []
        self.mirrorlist = None
        self.metalink = None

    @classmethod
    def from_url(cls, urls=None, mirrorlist=None, metalink=None):
        if not urls and not mirrorlist and not metalink:
            raise AttributeError("At least one argument must be specified")

        tmpdir = tempfile.mkdtemp(prefix="deltarepo-updater-", dir="/tmp")

        h = librepo.Handle()
        h.repotype = librepo.YUMREPO
        h.urls = urls
        h.mirrorlisturl = mirrorlist
        h.metalinkurl = metalink
        h.yumdlist = []
        h.destdir = tmpdir

        try:
            r = h.perform()
        except librepo.LibrepoException as e:
            shutil.rmtree(tmpdir)
            raise DeltaRepoError("Cannot download ({0}, {1}, {2}): {3}".format(
                urls, mirrorlist, metalink, e))

        repo = cls()
        repo._fill_from_path(tmpdir, contenthash=False)

        repo.path = None
        repo.urls = urls
        repo.mirrorlist = mirrorlist
        repo.metalink = metalink

        shutil.rmtree(tmpdir)
        return repo

class DRMirror(object):
    def __init__(self):
        self.url = None
        self.records = []       # list of DeltaReposRecord
        self.deltarepos = None  # DeltaRepos object

    @classmethod
    def from_url(cls, url):
        # TODO: support for metalink and mirrorlist
        fd, fn = tempfile.mkstemp(prefix="deltarepos.xml.xz-", dir="/tmp")

        # Download deltarepos.xml
        deltarepos_xml_url = os.path.join(url, "deltarepos.xml.xz")
        try:
            librepo.download_url(deltarepos_xml_url, fd)
        except librepo.LibrepoException as e:
            os.remove(fn)
            raise DeltaRepoError("Cannot download {0}: {1}".format(
                deltarepos_xml_url, e))

        # Parse deltarepos.xml
        dr = DeltaRepos()
        try:
            dr.xmlparse(fn)
        except DeltaRepoError as e:
            raise DeltaRepoError("Error while parsing deltarepos.xml "
                                 "from {0}: {1}".format(deltarepos_xml_url, e))
        finally:
            os.remove(fn)

        # Fill and return DRMirror object
        drm = cls()
        drm.url = url               # Url of the mirror
        drm.records = dr.records    # List of DeltaReposRecords
        drm.deltarepos = dr         # DeltaRepos object
        return drm

class Link(object):
    """Graph's link (path) = a delta repository
    from one point of history (version) to another.
    """

    def __init__(self):
        self.deltareposrecord = None    # DeltaReposRecord()
        self.drmirror = None            # DRMirror()

    #def __getattr__(self, item):
    #    if hasattr(self.deltareposrecord, item):
    #        return getattr(self.deltareposrecord, item, None)
    #    raise AttributeError("object has no attribute '{0}'".format(item))

    @property
    def src(self):
        """Source content hash"""
        return self.deltareposrecord.contenthash_src

    @property
    def dst(self):
        """Destination content hash."""
        return self.deltareposrecord.contenthash_dst

    @property
    def type(self):
        """Type of content hash (e.g., sha256, etc.) """
        return self.deltareposrecord.contenthash_type

    @classmethod
    def links_from_drmirror(cls, drmirror):
        links = []
        for rec in drmirror.records:
            link = cls()
            link.deltareposrecord = rec
            link.drmirror = drmirror
            links.append(link)
        return links

class Solver(object):

    class ResolvedPath(object):
        def __init__(self):
            self.cost = -1  # Sum of hop sizes
            self.links = [] # List of Link objects

    class Node(object):
        """Single graph node"""
        def __init__(self):
            self.sources = []
            self.targets = []

    class Graph(object):
        def __init__(self):
            self.links = []
            self.nodes = []

        @classmethod
        def graph_from_links(cls, links):
            nodes = []
            # TODO
            g = cls()
            g.links = links
            g.nodes = nodes
            return g

    def __init__(self, links, start, target, contenthash_type="sha256"):
        self.links = links      # List of hops
        self.start = start      # Start hop
        self.target = target    # Target hop
        self.contenthash_type = contenthash_type

        # TODO: process (split) DRMirror objects to PathHops

    def get_paths(self):
        """Get all available paths"""
        # TODO: Raise warning when multiple hops with the same src
        # and destination exists
        pass

    def solve(self):
        # TODO: Implement Dijkstra's algorithm here
        pass

class DRUpdater(object):

    def __init__(self, localrepo, drmirrors=None, originrepo=None,
                 target_contenthash=None):
        self.localrepo = localrepo
        self.drmirrors = drmirrors or []
        self.originrepo = originrepo

        # TODO: Make hops from the drmirrors

        self.target_contenthash = target_contenthash

    def _find_dr_path(self):
        pass

    def _use_originrepo(self):
        """Replace the local repo with the origin one"""
        pass

    def _use_dr_path(self):
        """Update the local repo by the selected path of delta repos"""
        pass

    def update(self):
        pass