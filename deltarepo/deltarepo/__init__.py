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

class MainDeltaModule(DeltaModule):
    def do(self, pri_old_fn, pri_new_fn, pri_f,
           fil_new_fn, fil_f, oth_new_fn, oth_f, removed):

        old_packages = set()
        added_packages = {}         # dict { 'pkgId': pkg }
        added_packages_ids = []     # list of package ids

        def old_pkgcb(pkg):
            old_packages.add((pkg.pkgId, pkg.location_href, pkg.location_base))

        def new_pkgcb(pkg):
            pkg_tuple = (pkg.pkgId, pkg.location_href, pkg.location_base)
            if not pkg_tuple in old_packages:
                # This package is only in new repodata
                added_packages[pkg.pkgId] = pkg
                added_packages_ids.append(pkg.pkgId)
            else:
                # This package is also in the old repodata
                old_packages.remove(pkg_tuple)

        cr.xml_parse_primary(pri_old_fn, pkgcb=old_pkgcb)
        cr.xml_parse_primary(pri_new_fn, pkgcb=new_pkgcb)

        removed_pkgs = sorted(old_packages)
        for _, location_href, location_base in removed_pkgs:
            removed.add_pkg_locations(location_href, location_base)

        num_of_packages = len(added_packages)

        # Write out primary delta
        pri_f.set_num_of_pkgs(num_of_packages)
        for pkgid in added_packages_ids:
            pri_f.add_pkg(added_packages[pkgid])
        pri_f.close()

        ",".join(("a", "b"))

        # Filelists and Other cb
        def newpkgcb(pkgId, name, arch):
            return added_packages.get(pkgId, None)

        # Write out filelists delta
        if fil_f and fil_new_fn:
            cr.xml_parse_filelists(fil_new_fn, newpkgcb=newpkgcb)
            fil_f.set_num_of_pkgs(num_of_packages)
            for pkgid in added_packages_ids:
                fil_f.add_pkg(added_packages[pkgid])
            fil_f.close()

        # Write out other delta
        if oth_f and oth_new_fn:
            cr.xml_parse_other(oth_new_fn, newpkgcb=newpkgcb)
            oth_f.set_num_of_pkgs(num_of_packages)
            for pkgid in added_packages_ids:
                oth_f.add_pkg(added_packages[pkgid])
            oth_f.close()

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

        # Do deltas for the "primary", "filelists" and "other"
        if not "primary" in old_records or not "primary" in new_records:
            raise DeltaRepoError("Missing primary metadata")

        pri_old_fn = os.path.join(old_path, old_records["primary"].location_href)
        pri_new_fn = os.path.join(new_path, new_records["primary"].location_href)
        pri_out_fn = os.path.join(delta_repodata_path, "primary.xml.gz")
        pri_out_f_stat = cr.ContentStat(cr.SHA256)
        pri_out_f = cr.PrimaryXmlFile(pri_out_fn, cr.GZ_COMPRESSION)

        fil_new_fn = None
        fil_out_fn = None
        fil_out_f_stat = None
        fil_out_f = None
        if ("filelists" in new_records):
            fil_new_fn = os.path.join(new_path, new_records["filelists"].location_href)
            fil_out_fn = os.path.join(delta_repodata_path, "filelists.xml.gz")
            fil_out_f_stat = cr.ContentStat(cr.SHA256)
            fil_out_f = cr.FilelistsXmlFile(fil_out_fn, cr.GZ_COMPRESSION)

        oth_new_fn = None
        out_out_fn = None
        oth_out_f_stat = None
        oth_out_f = None
        if ("other" in new_records):
            oth_new_fn = os.path.join(new_path, new_records["other"].location_href)
            oth_out_fn = os.path.join(delta_repodata_path, "other.xml.gz")
            oth_out_f_stat = cr.ContentStat(cr.SHA256)
            oth_out_f = cr.OtherXmlFile(oth_out_fn, cr.GZ_COMPRESSION)

        deltamodule = MainDeltaModule()
        deltamodule.do(pri_old_fn, pri_new_fn, pri_out_f, fil_new_fn,
                       fil_out_f, oth_new_fn, oth_out_f, removedxml)

        pri_rec = cr.RepomdRecord("primary", pri_out_fn)
        # TODO: Function for this
        pri_rec.size_open = pri_out_f_stat.size
        pri_rec.checksum_open = pri_out_f_stat.checksum
        pri_rec.checksum_open_type = cr.checksum_name_str(pri_out_f_stat.checksum_type)
        pri_rec.fill(cr.SHA256)
        delta_repomd.set_record(pri_rec)

        if fil_out_fn:
            fil_rec = cr.RepomdRecord("filelists", fil_out_fn)
            fil_rec.fill(cr.SHA256)
            delta_repomd.set_record(fil_rec)

        if oth_out_fn:
            oth_rec = cr.RepomdRecord("other", oth_out_fn)
            oth_rec.fill(cr.SHA256)
            delta_repomd.set_record(oth_rec)

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

