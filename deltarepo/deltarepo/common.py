import os
import tempfile
import logging
import xml.dom.minidom
import createrepo_c as cr
from lxml import etree

class LoggingInterface(object):
    def __init__(self, logger=None):
        if logger is None:
            logger = logging.getLogger()
            logger.disabled = True
        self.logger = logger

    def _debug(self, msg):
        self.logger.debug(msg)

    def _info(self, msg):
        self.logger.info(msg)

    def _warning(self, msg):
        self.logger.warning(msg)

    def _error(self, msg):
        self.logger.error(msg)

    def _critical(self, msg):
        self.logger.critical(msg)

class RemovedXml(object):
    def __init__(self):
        self.packages = {}  # { location_href: location_base }
        self.files = {}     # { location_href: location_base or Null }

    def __str__(self):
        print self.packages
        print self.files

    def add_pkg(self, pkg):
        self.packages[pkg.location_href] = pkg.location_base

    def add_pkg_locations(self, location_href, location_base):
        self.packages[location_href] = location_base

    def add_record(self, rec):
        self.files[rec.location_href] = rec.location_base

    def xml_dump(self):
        xmltree = etree.Element("removed")
        packages = etree.SubElement(xmltree, "packages")
        for href, base in self.packages.iteritems():
            attrs = {}
            if href: attrs['href'] = href
            if base: attrs['base'] = base
            if not attrs: continue
            etree.SubElement(packages, "location", attrs)
        files = etree.SubElement(xmltree, "files")
        for href, base in self.files.iteritems():
            attrs = {}
            if href: attrs['href'] = href
            if base: attrs['base'] = base
            if not attrs: continue
            etree.SubElement(files, "location", attrs)
        return etree.tostring(xmltree,
                              pretty_print=True,
                              encoding="UTF-8",
                              xml_declaration=True)

    def xml_parse(self, path):

        _, tmp_path = tempfile.mkstemp()
        cr.decompress_file(path, tmp_path, cr.AUTO_DETECT_COMPRESSION)
        dom = xml.dom.minidom.parse(tmp_path)
        os.remove(tmp_path)

        packages = dom.getElementsByTagName("packages")
        if packages:
            for loc in packages[0].getElementsByTagName("location"):
                href = loc.getAttribute("href")
                base = loc.getAttribute("base")
                if not href:
                    continue
                if not base:
                    base = None
                self.packages[href] = base

        files = dom.getElementsByTagName("files")
        if files:
            for loc in files[0].getElementsByTagName("location"):
                href = loc.getAttribute("href")
                base = loc.getAttribute("base")
                if not href:
                    continue
                if not base:
                    base = None
                self.files[href] = base

class Metadata(object):
    """Metadata file"""

    def __init__(self, metadata_type):

        self.metadata_type = metadata_type

        # Paths
        self.old_fn = None
        self.delta_fn = None
        self.new_fn = None

        self.out_dir = None

        # Settings
        self.checksum_type = None
        self.compression_type = None

        # Records

        # List of new repomd records related to this delta file
        # List because some deltafiles could lead to more output files.
        # E.g.: delta of primary.xml generate new primary.xml and
        # primary.sqlite as well.
        self.generated_repomd_records = []
