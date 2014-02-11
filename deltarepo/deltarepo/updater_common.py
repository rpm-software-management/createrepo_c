import shutil
import os
import os.path
import librepo
import tempfile
import createrepo_c as cr
from .deltarepos import DeltaRepos
from .common import LoggingInterface, calculate_contenthash
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
        self._deltareposrecord = None    # DeltaReposRecord()
        self._drmirror = None            # DRMirror()

    #def __getattr__(self, item):
    #    if hasattr(self.deltareposrecord, item):
    #        return getattr(self.deltareposrecord, item, None)
    #    raise AttributeError("object has no attribute '{0}'".format(item))

    @property
    def src(self):
        """Source content hash"""
        return self._deltareposrecord.contenthash_src

    @property
    def dst(self):
        """Destination content hash."""
        return self._deltareposrecord.contenthash_dst

    @property
    def type(self):
        """Type of content hash (e.g., sha256, etc.) """
        return self._deltareposrecord.contenthash_type

    @property
    def mirrorurl(self):
        """Mirror url"""
        return self._drmirror.url

    def cost(self):
        """Cost (currently just a total size).
        In future maybe just sizes of needed delta metadata."""
        return self._deltareposrecord.size_total

    @classmethod
    def links_from_drmirror(cls, drmirror):
        links = []
        for rec in drmirror.records:
            link = cls()
            link.deltareposrecord = rec
            link.drmirror = drmirror
            links.append(link)
        return links

class Solver(LoggingInterface):

    class ResolvedPath(object):
        def __init__(self):
            self.cost = -1  # Sum of hop sizes
            self.links = [] # List of Link objects

    class Node(object):
        """Single graph node"""
        def __init__(self, value):
            self.value = value   # Content hash
            self.links = []      # List of all links that belong to the node
                                 # All of them must have self.value as a src value
            self.targets = {}    # { Node: Link }
            self.sources = set() # set(Nodes)

        def __repr__(self):
            targets = [x.value for x in self.targets]
            return "<Node {0} \'{1}\' points to: {2}>".format(
                id(self), self.value, targets)

    class Graph(object):
        def __init__(self, contenthash_type="sha256"):
            #self.links = []
            self.nodes = {}  # { 'content_hash': Node }
            self.contenthash_type = contenthash_type

        def get_node(self, contenthash):
            return self.nodes.get(contenthash)

        @classmethod
        def graph_from_links(cls, links, logger, contenthash_type="sha256"):
            already_processed_links = set() # Set of tuples (src, dst)
            nodes = {}  # { 'content_hash': Node }

            for link in links:
                if contenthash_type != link.type.lower():
                    logger.warning("Content hash type mishmash {0} vs {1}"
                                   "".format(contenthash_type, link.type))

                if (link.src, link.dst) in already_processed_links:
                    logger.warning("Duplicated path {0}->{1} from {2} skipped"
                                   "".format(link.src, link.dst, link.mirrorurl))
                    continue

                node = nodes.setdefault(link.src, Solver.Node(link.src))

                if link.dst in node.targets:
                    # Should not happen (the already_processed_links
                    # list should avoid this)
                    logger.warning("Duplicated path {0}->{1} from {2} skipped"
                                   "".format(link.src, link.dst, link.mirrorurl))
                    continue

                #node.links.append(link)    # TODO: Remove (?)
                dst_node = nodes.setdefault(link.dst, Solver.Node(link.dst))
                dst_node.sources.add(node)
                node.targets[dst_node] = link

            g = cls()
            g.links = links
            g.nodes = nodes
            return g

    def __init__(self, links, source, target, contenthash_type="sha256", logger=None):
        LoggingInterface.__init__(self, logger)

        self.links = links      # Links
        self.source_ch = source # Source content hash (str)
        self.target_ch = target # Target content hash (str)
        self.contenthash_type = contenthash_type

    def solve(self):
        # Build the graph
        graph = self.Graph.graph_from_links(self.links,
                                            self.logger,
                                            self.contenthash_type)

        # Find start and end node in the graph
        source_node = graph.get_node(self.source_ch)
        if not source_node:
            raise DeltaRepoError("Source repo ({0}) not available".format(self.source_ch))
        target_node = graph.get_node(self.target_ch)
        if not target_node:
            raise DeltaRepoError("Target repo ({0}) not available".format(self.target_ch))

        # Dijkstra's algorithm
        # http://en.wikipedia.org/wiki/Dijkstra%27s_algorithm
        dist = {}       # Distance
        previous = {}   # Predecessor
        Q = []

        for _, node in graph.nodes.items():
            dist[node] = -1 # -1 Stands for infinity here
            previous[node] = None
            Q.append(node)

        dist[source_node] = 0

        while Q:
            u = None
            val = -1
            # Select node from Q with the smallest distance in dist
            for node in Q:
                if dist[node] == -1:
                    continue
                if val == -1 or dist[node] < val:
                    val = dist[node]
                    u = node

            if u:
                # Remove the u from the queue
                Q.remove(u)
            else:
                # All remaining nodes are inaccessible from source
                break

            if u == target_node:
                # Cool!
                break

            # Iterate over the u neighbors
            for v, link in u.targets.items():
                alt = dist[u] + link.cost()
                if alt < dist[v] or dist[v] == -1:
                    dist[v] = alt
                    previous[v] = u

        # At this point we have previous and dist filled
        import pprint
        print
        pprint.pprint(previous)
        print
        pprint.pprint(dist)

        resolved_path = []
        u = target_node
        while previous[u] is not None:
            resolved_path.append(previous[u])
            u = previous[u]

        resolved_path.reverse()

        print "xxxx"
        pprint.pprint(resolved_path)

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