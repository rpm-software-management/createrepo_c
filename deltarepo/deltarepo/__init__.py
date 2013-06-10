"""
DeltaRepo package for Python.
This is the library for generation, application and handling of
DeltaRepositories.
The library is builded on the Createrepo_c library and its a part of it.

Copyright (C) 2013   Tomas Mlcoch

"""

import os
import shutil
import hashlib
import logging
from lxml import etree
import createrepo_c as cr

__all__ = ['VERSION', 'VERBOSE_VERSION', 'DeltaRepoGenerator']

VERSION = "0.0.1"
VERBOSE_VERSION = "%s (createrepo_c: %s)" % (VERSION, cr.VERSION)

class DeltaRepoError(Exception):
    pass

class DeltaModule(object):

    def _path(self, path, record):
        return os.path.join(path, record.location_href)

class PrimaryDeltaModule(DeltaModule):
    def do(old_path, old_rec, new_path, new_rec, delta_path, data):

        old_fn = self._path(old_path, old_rec)

        old_packages = set()

        def pkgcb(pkg):
            old_packages.add(pkg.pkgId, pkg.location_href, location_base)

        cr.xml_parse_primary(old_fn, pkgcb=pkgcb)

        print old_packages
        print "DONE"


_DELTA_MODULES = {
        "primary": PrimaryDeltaModule,
#        "filelists": FilelistsDeltaModule,
#        "other": OtherDeltaModule,
    }

class RemovedXml(object):
    def __init__(self):
        self.packages = {}  # { location_href: location_base }
        self.files = {}     # { location_href: location_base or Null }

    def __str__(self):
        print self.packages
        print self.files

    def add_pkg(self, pkg):
        self.packages[pkg.location_href] = pkg.location_base

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
        # TODO: parsing for RemovedXml
        pass

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

class DeltaRepoGenerator(LoggingInterface):
    """Object for generating of DeltaRepositories."""

    def __init__(self, id_type=None, logger=None):
        LoggingInterface.__init__(self, logger)

        if id_type is None:
            id_type = "sha256"
        self.id_type = id_type

    def _fn_without_checksum(self, path):
        """Strip checksum from a record filename"""
        path = os.path.basename(path)
        return path.rsplit('-')[-1]

    def gendelta(self, old_path, new_path, out_path=None,
                 do_only=None, skip=None):
        removedxml = RemovedXml()

        # Prepare variables with paths
        new_repodata_path = os.path.join(new_path, "repodata/")
        old_repodata_path = os.path.join(old_path, "repodata/")

        if not os.path.isdir(new_repodata_path):
            raise IOError("Directory %s doesn't exists" % new_repodata_path)

        if not os.path.isdir(old_repodata_path):
            raise IOError("Directory %s doesn't exists" % old_repodata_path)

        old_repomd_path = os.path.join(old_repodata_path, "repomd.xml")
        new_repomd_path = os.path.join(new_repodata_path, "repomd.xml")

        # Prepare Repomd objects
        old_repomd = cr.Repomd(old_repomd_path)
        new_repomd = cr.Repomd(new_repomd_path)
        delta_repomd = cr.Repomd()

        # Prepare output path
        delta_path = os.path.join(out_path, ".deltarepo/")
        delta_repodata_path = os.path.join(delta_path, "repodata/")
        os.mkdir(delta_path)
        os.mkdir(delta_repodata_path)

        # Do repomd delta
        delta_repomd.set_revision(new_repomd.revision)
        for tag in new_repomd.distro_tags:
            delta_repomd.add_distro_tag(tag[1], tag[0])
        for tag in new_repomd.repo_tags:
            delta_repomd.add_repo_tag(tag)
        for tag in new_repomd.content_tags:
            delta_repomd.add_content_tag(tag)

        old_records = dict([(record.type, record) for record in old_repomd.records ])
        new_records = dict([(record.type, record) for record in new_repomd.records ])
        old_record_types = set(old_records.keys())
        new_record_types = set(new_records.keys())
        deleted_repomd_record_types = old_record_types - new_record_types
        added_repomd_record_types = new_record_types - old_record_types

        delta_data = {  # Data shared between delta modules
                "removedxml": removedxml,
            }

        # Do deltas for the "primary
        if not "primary" in old_records or not "primary" in new_records:
            raise DeltaRepoError("Missing primary metadata")

        delta_fn = os.path.join(delta_repodata_path, "primary.xml")
        deltamodule = _DELTA_MODULES["primary"]()
        deltamodule.do(old_path, old_records["primary"],
                       new_path, new_records["primary"],
                       delta_fn, delta_data)

        # Do deltas for the rest of the metadata
        for record_type in added_repomd_record_types:
            # Added records
            self._debug("Added: %s" % record_type)
            rec = new_records[record_type]
            rec_path = os.path.join(new_path, rec.location_href)
            shutil.copy2(rec_path, delta_repodata_path)
            delta_repomd.set_record(rec)

        # Do deltas for individual records
        for record in old_repomd.records:
            if record.type == "primary":
                # primary record is already done
                continue

            if record.type in deleted_repomd_record_types:
                # Removed record
                removedxml.add_record(record)
                self._debug("Removed: %s" % record.type)
                continue

            old_rec = old_records[record.type]
            new_rec = new_records[record.type]
            if old_rec.checksum == new_rec.checksum and \
               old_rec.checksum_open == new_rec.checksum_open:
                # File unchanged
                self._debug("Unchanged: %s" % record.type)
                continue

            if (skip and record.type in skip) or \
               (do_only and record.type not in do_only) or \
               (record.type not in _DELTA_MODULES):
                # Do not do delta of this file, just copy it
                self._debug("No delta for: %s" % record.type)
                rec = new_records[record.type]
                rec_path = os.path.join(new_path, rec.location_href)
                shutil.copy2(rec_path, delta_repodata_path)
                delta_repomd.set_record(record)
                continue

            # TODO: Do delta
            delta_fn = os.path.join(delta_repodata_path,
                            self._fn_without_checksum(record.location_href))
            print delta_fn
            deltamodule = _DELTA_MODULES[record.type]()
            deltamodule.do(old_rec, new_rec, delta_fn, delta_data)
            # TODO

        # Write out removed.xml
        # TODO: Compressed!!
        removedxml_path = os.path.join(delta_repodata_path, "removed.xml")
        removedxml_xml = removedxml.xml_dump()
        open(removedxml_path, "w").write(removedxml_xml)
        removedxml_rec = cr.RepomdRecord("removed", removedxml_path)
        removedxml_rec.fill(cr.SHA256)
        delta_repomd.set_record(removedxml_rec)

        # Write out repomd.xml
        #deltarepoid = "%s-%s" % (old_repomd.repoid, new_repomd.repoid)
        # RepoId must be calculated during primary delta calculation
        deltarepoid = "xxx"
        delta_repomd.set_repoid(deltarepoid, self.id_type)
        delta_repomd_path = os.path.join(delta_repodata_path, "repomd.xml")
        delta_repomd_xml = delta_repomd.xml_dump()
        open(delta_repomd_path, "w").write(delta_repomd_xml)

