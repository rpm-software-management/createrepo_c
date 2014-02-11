"""
DeltaRepo package for Python.
This is the library for generation, application and handling of
DeltaRepositories.
The library is builded on the Createrepo_c library and its a part of it.

Copyright (C) 2013   Tomas Mlcoch

"""

import os
import shutil
import createrepo_c as cr
from .common import LoggingInterface
from .plugins_common import GlobalBundle, Metadata
from .deltametadata import DeltaMetadata, PluginBundle
from .common import DEFAULT_CHECKSUM_TYPE, DEFAULT_COMPRESSION_TYPE
from .plugins import GlobalBundle, PLUGINS, GENERAL_PLUGIN
from .util import calculate_content_hash, pkg_id_str
from .errors import DeltaRepoError

__all__ = ['DeltaRepoGenerator']

class DeltaRepoGenerator(LoggingInterface):

    def __init__(self,
                 old_repo_path,
                 new_repo_path,
                 out_path=None,
                 logger=None,
                 contenthash_type="sha256",
                 compression_type="xz",
                 force_database=False,
                 ignore_missing=False):

        # Initialization

        self.ignore_missing = ignore_missing

        LoggingInterface.__init__(self, logger)

        self.out_path = out_path or "./"

        self.final_path = os.path.join(self.out_path, "repodata")

        self.new_repo_path = new_repo_path
        self.new_repodata_path = os.path.join(self.new_repo_path, "repodata/")
        self.new_repomd_path = os.path.join(self.new_repodata_path, "repomd.xml")

        self.old_repo_path = old_repo_path
        self.old_repodata_path = os.path.join(self.old_repo_path, "repodata/")
        self.old_repomd_path = os.path.join(self.old_repodata_path, "repomd.xml")

        self.delta_repo_path = out_path
        self.delta_repodata_path = os.path.join(self.delta_repo_path, ".repodata/")
        self.delta_repomd_path = os.path.join(self.delta_repodata_path, "repomd.xml")

        # contenthash type
        self.contenthash_type_str = contenthash_type or "sha256"
        self.compression_type_str = compression_type or "xz"
        self.compression_type = cr.compression_type(self.compression_type_str)

        # Prepare Repomd objects
        self.old_repomd = cr.Repomd(self.old_repomd_path)
        self.new_repomd = cr.Repomd(self.new_repomd_path)
        self.delta_repomd = cr.Repomd()

        # Use revision and tags
        self.delta_repomd.set_revision(self.new_repomd.revision)
        for tag in self.new_repomd.distro_tags:
            self.delta_repomd.add_distro_tag(tag[1], tag[0])
        for tag in self.new_repomd.repo_tags:
            self.delta_repomd.add_repo_tag(tag)
        for tag in self.new_repomd.content_tags:
            self.delta_repomd.add_content_tag(tag)

        # Load records
        self.old_records = {}
        self.new_records = {}
        for record in self.old_repomd.records:
            self.old_records[record.type] = record
        for record in self.new_repomd.records:
            self.new_records[record.type] = record

        old_record_types = set(self.old_records.keys())
        new_record_types = set(self.new_records.keys())

        self.deleted_repomd_record_types = old_record_types - new_record_types
        self.added_repomd_record_types = new_record_types - old_record_types

        # Important sanity checks (repo without primary is definitely bad)
        if not "primary" in self.old_records:
            raise DeltaRepoError("Missing \"primary\" metadata in old repo")

        if not "primary" in self.new_records:
            raise DeltaRepoError("Missing \"primary\" metadata in new repo")

        # Detect type of checksum in the new repomd.xml (global)
        self.checksum_type = cr.checksum_type(self.new_records["primary"].checksum_type)
        if self.checksum_type == cr.UNKNOWN_CHECKSUM:
            raise DeltaRepoError("Unknown checksum type used in new repo: %s" % \
                    self.new_records["primary"].checksum_type)

        # TODO: Je treba detekovat typ checksumu, kdyz se stejne pro kazdej
        # record nakonec detekuje znova???

        # Detection if use unique md filenames
        if self.new_records["primary"].location_href.split("primary")[0] != "":
            self.unique_md_filenames = True

        self.old_contenthash = self.old_repomd.contenthash
        self.new_contenthash = self.new_repomd.contenthash

        self.deltametadata = DeltaMetadata()

        # Prepare global bundle
        self.globalbundle = GlobalBundle()
        self.globalbundle.contenthash_type_str = self.contenthash_type_str
        self.globalbundle.unique_md_filenames = self.unique_md_filenames
        self.globalbundle.force_database = force_database
        self.globalbundle.ignore_missing = ignore_missing

    def fill_deltametadata(self):
        if not self.deltametadata:
            return

        # Set revisions
        self.deltametadata.revision_src = self.old_repomd.revision
        self.deltametadata.revision_dst = self.new_repomd.revision

        # Set contenthashes
        self.deltametadata.contenthash_type = \
                    self.globalbundle.contenthash_type_str
        self.deltametadata.contenthash_src = \
                    self.globalbundle.calculated_old_contenthash
        self.deltametadata.contenthash_dst = \
                    self.globalbundle.calculated_new_contenthash

        # Set timestamps
        timestamp_src = 0
        timestamp_dst = 0
        for rec in self.old_repomd.records:
            timestamp_src = max(rec.timestamp, timestamp_src)
        for rec in self.new_repomd.records:
            timestamp_dst = max(rec.timestamp, timestamp_dst)
        self.deltametadata.timestamp_src = timestamp_src
        self.deltametadata.timestamp_dst = timestamp_dst

    def _new_metadata(self, metadata_type):
        """Return Metadata Object for the metadata_type"""

        metadata = Metadata(metadata_type)

        metadata.checksum_type = DEFAULT_CHECKSUM_TYPE
        metadata.compression_type = DEFAULT_COMPRESSION_TYPE

        # Output directory
        metadata.out_dir = self.delta_repodata_path

        # Properties related to the first (old) repository
        old_rec = self.old_records.get(metadata_type)
        metadata.old_rec = old_rec
        if old_rec:
            metadata.old_fn = os.path.join(self.old_repo_path, old_rec.location_href)
            if os.path.isfile(metadata.old_fn):
                metadata.old_fn_exists = True
            else:
                msg = "File {0} doesn't exist in the old " \
                      "repository!".format(metadata.old_fn)
                self._warning(msg)
                if not self.ignore_missing:
                    raise DeltaRepoError(msg + " Use --ignore-missing option "
                                               "to ignore this error")

        # Properties related to the second (new) repository
        new_rec = self.new_records.get(metadata_type)
        metadata.new_rec = new_rec
        if new_rec:
            metadata.new_fn = os.path.join(self.new_repo_path, new_rec.location_href)
            if os.path.isfile(metadata.new_fn):
                metadata.new_fn_exists = True

                # Determine compression type
                detected_compression_type = cr.detect_compression(metadata.new_fn)
                if (detected_compression_type != cr.UNKNOWN_COMPRESSION):
                    metadata.compression_type = detected_compression_type
                else:
                    self._warning("Cannot detect compression type for "
                                         "{0}".format(metadata.new_fn))
            else:
                msg = ("The file {0} doesn't exist in the new"
                       "repository!".format(metadata.new_fn))
                self._warning(msg)
                if not self.ignore_missing:
                    raise DeltaRepoError(msg + " Use --ignore-missing option "
                                               "to ignore this error")

            metadata.checksum_type = cr.checksum_type(new_rec.checksum_type)

        return metadata

    def check_content_hashes(self):
        self._debug("Checking expected content hashes")

        c_old_contenthash = self.globalbundle.calculated_old_contenthash
        c_new_contenthash = self.globalbundle.calculated_new_contenthash

        if not c_old_contenthash or not c_new_contenthash:

            pri_md = self._new_metadata("primary")

            if not c_old_contenthash:
                if not pri_md.old_fn_exists:
                    raise DeltaRepoError("Old repository doesn't have "
                                         "a primary metadata!")
                c_old_contenthash = calculate_content_hash(pri_md.old_fn,
                                                          self.contenthash_type_str,
                                                          self._get_logger())
            if not c_new_contenthash:
                if not pri_md.new_fn_exists:
                    raise DeltaRepoError("New repository doesn't have "
                                         "a primary metadata!")
                c_new_contenthash = calculate_content_hash(pri_md.new_fn,
                                                          self.contenthash_type_str,
                                                          self._get_logger())

            self.globalbundle.calculated_old_contenthash = c_old_contenthash
            self.globalbundle.calculated_new_contenthash = c_new_contenthash

        self._debug("Calculated content hash of the old repo: {0}".format(
                    c_old_contenthash))
        self._debug("Calculated content hash of the new repo: {0}".format(
                    c_new_contenthash))

        if self.old_contenthash:
            if self.old_contenthash != c_old_contenthash:
                message = "Content hash of the \"{0}\" repository doesn't match "\
                          "the real one ({1} != {2}).".format(
                           self.old_repo_path, self.old_contenthash,
                           self.globalbundle.calculated_old_contenthash)
                self._error(message)
                raise DeltaRepoError(message)
            else:
                self._debug("Content hash of the old repo matches ({0})".format(
                            self.old_contenthash))
        else:
            self._debug("Content hash of the \"{0}\" is not part of its "\
                        "repomd".format(self.old_repo_path))

        if self.new_contenthash:
            if self.new_contenthash != c_new_contenthash:
                message = "Content hash of the \"{0}\" repository doesn't match "\
                          "the real one ({1} != {2}).".format(
                           self.new_repo_path, self.new_contenthash,
                           self.globalbundle.calculated_new_contenthash)
                self._error(message)
                raise DeltaRepoError(message)
            else:
                self._debug("Content hash of the new repo matches ({0})".format(
                            self.new_contenthash))
        else:
            self._debug("Content hash of the \"{0}\" is not part of its "\
                        "repomd".format(self.new_repo_path))

    def gen(self):

        # Prepare output path
        os.mkdir(self.delta_repodata_path)

        # Set of types of processed metadata records ("primary", "primary_db"...)
        processed_metadata = set()

        for plugin in PLUGINS:

            # Prepare metadata for the plugin
            metadata_objects = {}
            for metadata_name in plugin.METADATA:
                metadata_object = self._new_metadata(metadata_name)
                if metadata_object is not None:
                    metadata_objects[metadata_name] = metadata_object

            # Skip plugin if no supported metadata available
            if not metadata_objects:
                self._debug("Plugin {0}: Skipped - None of supported " \
                            "metadata {1} available".format(
                            plugin.NAME, plugin.METADATA))
                continue

            # Prepare plugin bundle
            pluginbundle = PluginBundle(plugin.NAME, plugin.VERSION)
            self.deltametadata.add_pluginbundle(pluginbundle)

            # Use the plugin
            self._debug("Plugin {0}: Active".format(plugin.NAME))
            plugin_instance = plugin(pluginbundle, self.globalbundle,
                                     logger=self._get_logger())
            repomd_records = plugin_instance.gen(metadata_objects)

            # Put repomd records from processed metadatas to repomd
            self._debug("Plugin {0}: Processed {1} record(s) " \
                "and produced:".format(plugin.NAME, metadata_objects.keys()))
            for rec in repomd_records:
                self._debug(" - {0}".format(rec.type))
                self.delta_repomd.set_record(rec)

            # Organization stuff
            for md in metadata_objects.keys():
                processed_metadata.add(md)

        # Process rest of the metadata files
        metadata_objects = {}
        for rectype, rec in self.new_records.items():
            if rectype not in processed_metadata:
                metadata_object = self._new_metadata(rectype)
                if metadata_object is not None:
                    self._debug("To be processed by general delta plugin: " \
                                "{0}".format(rectype))
                    metadata_objects[rectype] = metadata_object
                else:
                    self._debug("Not processed - even by general delta " \
                                "plugin: {0}".format(rectype))

        if metadata_objects:
            # Use the plugin
            pluginbundle = PluginBundle(GENERAL_PLUGIN.NAME, GENERAL_PLUGIN.VERSION)
            self.deltametadata.add_pluginbundle(pluginbundle)
            self._debug("Plugin {0}: Active".format(GENERAL_PLUGIN.NAME))
            plugin_instance = GENERAL_PLUGIN(pluginbundle, self.globalbundle,
                                             logger=self._get_logger())
            repomd_records = plugin_instance.gen(metadata_objects)

            # Put repomd records from processed metadatas to repomd
            self._debug("Plugin {0}: Processed {1} record(s) " \
                "and produced:".format(GENERAL_PLUGIN.NAME, metadata_objects.keys()))
            for rec in repomd_records:
                self._debug(" - {0}".format(rec.type))
                self.delta_repomd.set_record(rec)

        # Write out deltametadata.xml
        self.fill_deltametadata()
        deltametadata_xml = self.deltametadata.xmldump()
        deltametadata_path = os.path.join(self.delta_repodata_path, "deltametadata.xml")

        if (self.compression_type != cr.UNKNOWN_COMPRESSION):
            deltametadata_path += cr.compression_suffix(self.compression_type)
            stat = cr.ContentStat(self.checksum_type)
            f = cr.CrFile(deltametadata_path, cr.MODE_WRITE,
                          self.compression_type, stat)
            f.write(deltametadata_xml)
            f.close()
        else:
            open(deltametadata_path, "w").write(deltametadata_xml)

        deltametadata_rec = cr.RepomdRecord("deltametadata", deltametadata_path)
        deltametadata_rec.load_contentstat(stat)
        deltametadata_rec.fill(self.checksum_type)
        if self.unique_md_filenames:
            deltametadata_rec.rename_file()
        self.delta_repomd.set_record(deltametadata_rec)

        # Check if calculated contenthashes match
        self.check_content_hashes()

        # Prepare and write out the new repomd.xml
        self._debug("Preparing repomd.xml ...")
        deltacontenthash = "{0}-{1}".format(self.globalbundle.calculated_old_contenthash,
                                       self.globalbundle.calculated_new_contenthash)
        self.delta_repomd.set_contenthash(deltacontenthash, self.contenthash_type_str)
        self.delta_repomd.sort_records()
        delta_repomd_xml = self.delta_repomd.xml_dump()

        self._debug("Writing repomd.xml ...")
        open(self.delta_repomd_path, "w").write(delta_repomd_xml)

        # Final move
        if os.path.exists(self.final_path):
            self._warning("Destination dir already exists! Removing %s" % \
                          self.final_path)
            shutil.rmtree(self.final_path)
        self._debug("Moving %s -> %s" % (self.delta_repodata_path, self.final_path))
        os.rename(self.delta_repodata_path, self.final_path)

