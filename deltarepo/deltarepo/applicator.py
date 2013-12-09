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
from .common import LoggingInterface, Metadata, DeltaMetadata
from .delta_plugins import GlobalBundle, PLUGINS, GENERAL_PLUGIN
from .errors import DeltaRepoError

__all__ = ['DeltaRepoApplicator']

class DeltaRepoApplicator(LoggingInterface):

    def __init__(self,
                 old_repo_path,
                 delta_repo_path,
                 out_path=None,
                 logger=None,
                 force_database=False,
                 ignore_missing=False):

        # Initialization

        LoggingInterface.__init__(self, logger)

        self.repoid_type = None
        self.unique_md_filenames = False
        self.force_database = force_database
        self.ignore_missing = ignore_missing
        self.deltametadata = DeltaMetadata()

        self.out_path = out_path or "./"

        self.final_path = os.path.join(self.out_path, "repodata")

        self.new_repo_path = out_path
        self.new_repodata_path = os.path.join(self.new_repo_path, ".repodata/")
        self.new_repomd_path = os.path.join(self.new_repodata_path, "repomd.xml")

        self.old_repo_path = old_repo_path
        self.old_repodata_path = os.path.join(self.old_repo_path, "repodata/")
        self.old_repomd_path = os.path.join(self.old_repodata_path, "repomd.xml")

        self.delta_repo_path = delta_repo_path
        self.delta_repodata_path = os.path.join(self.delta_repo_path, "repodata/")
        self.delta_repomd_path = os.path.join(self.delta_repodata_path, "repomd.xml")

        # Prepare repomd objects
        self.old_repomd = cr.Repomd(self.old_repomd_path)
        self.delta_repomd = cr.Repomd(self.delta_repomd_path)
        self.new_repomd = cr.Repomd()

        # Check if delta repo id correspond with the old repo id
        if not self.delta_repomd.repoid or \
                len(self.delta_repomd.repoid.split('-')) != 2:
            raise DeltaRepoError("Bad DeltaRepoId")

        self.repoid_type_str = self.delta_repomd.repoid_type
        self.old_id, self.new_id = self.delta_repomd.repoid.split('-')
        self._debug("Delta %s -> %s" % (self.old_id, self.new_id))

        if self.old_repomd.repoid_type == self.delta_repomd.repoid_type:
            if self.old_repomd.repoid and self.old_repomd.repoid != self.old_id:
                raise DeltaRepoError("Not suitable delta for current repo " \
                        "(Expected: {0} Real: {1})".format(
                            self.old_id, self.old_repomd.repoid))
        else:
            self._debug("Different repoid types repo: {0} vs delta: {1}".format(
                    self.old_repomd.repoid_type, self.delta_repomd.repoid_type))

        # Use revision and tags
        self.new_repomd.set_revision(self.delta_repomd.revision)
        for tag in self.delta_repomd.distro_tags:
            self.new_repomd.add_distro_tag(tag[1], tag[0])
        for tag in self.delta_repomd.repo_tags:
            self.new_repomd.add_repo_tag(tag)
        for tag in self.delta_repomd.content_tags:
            self.new_repomd.add_content_tag(tag)

        # Load records
        self.old_records = {}
        self.delta_records = {}
        for record in self.old_repomd.records:
            self.old_records[record.type] = record
        for record in self.delta_repomd.records:
            self.delta_records[record.type] = record

        old_record_types = set(self.old_records.keys())
        delta_record_types = set(self.delta_records.keys())

        self.deleted_repomd_record_types = old_record_types - delta_record_types
        self.added_repomd_record_types = delta_record_types - old_record_types

        # Important sanity checks (repo without primary is definitely bad)
        if not "primary" in self.old_records:
            raise DeltaRepoError("Missing \"primary\" metadata in old repo")

        if not "primary" in self.delta_records:
            raise DeltaRepoError("Missing \"primary\" metadata in delta repo")

        # Detect type of checksum in the delta repomd.xml
        self.checksum_type = cr.checksum_type(self.delta_records["primary"].checksum_type)
        if self.checksum_type == cr.UNKNOWN_CHECKSUM:
            raise DeltaRepoError("Unknown checksum type used in delta repo: %s" % \
                    self.delta_records["primary"].checksum_type)

        # Detection if use unique md filenames
        if self.delta_records["primary"].location_href.split("primary")[0] != "":
            self.unique_md_filenames = True

        # Load removedxml
        self.removedxml_path = None
        if "deltametadata" in self.delta_records:
            self.deltametadata_path = os.path.join(self.delta_repo_path,
                                   self.delta_records["deltametadata"].location_href)
            self.deltametadata.xmlparse(self.deltametadata_path)
        else:
            self._warning("\"deltametadata\" record is missing in repomd.xml "\
                          "of delta repo")

        # Prepare global bundle
        self.globalbundle = GlobalBundle()
        self.globalbundle.repoid_type_str = self.repoid_type_str
        self.globalbundle.unique_md_filenames = self.unique_md_filenames
        self.globalbundle.force_database = self.force_database
        self.globalbundle.ignore_missing = self.ignore_missing

    def _new_metadata(self, metadata_type):
        """Return Metadata Object for the metadata_type or None"""

        if metadata_type not in self.delta_records:
            return None

        metadata = Metadata(metadata_type)

        # Build delta filename
        metadata.delta_fn = os.path.join(self.delta_repo_path,
                            self.delta_records[metadata_type].location_href)
        if not os.path.isfile(metadata.delta_fn):
            self._warning("Delta file {0} doesn't exist!".format(metadata.delta_fn))
            return None

        metadata.delta_checksum = self.delta_records[metadata_type].checksum

        if metadata_type in self.old_records:
            # Build old filename
            metadata.old_fn = os.path.join(self.old_repo_path,
                            self.old_records[metadata_type].location_href)
            if not os.path.isfile(metadata.old_fn):
                msg = "File {0} doesn't exist in the old repository" \
                      " (but it should - delta may rely on " \
                      "it)!".format(metadata.old_fn)
                self._warning(msg)
                if not self.ignore_missing:
                    raise DeltaRepoError(msg + " Use --ignore-missing option "
                                               "to ignore this error")
                #self._warning("Metadata of the type \"{0}\" maybe won't be " \
                #              "available in the generated repository".format(metadata_type))
                #self._warning("Output repository maybe WON'T be a 1:1 identical " \
                #              "to the target repository!")
                metadata.old_fn = None

        # Set output directory
        metadata.out_dir = self.new_repodata_path

        # Determine checksum type
        metadata.checksum_type = cr.checksum_type(
                self.delta_records[metadata_type].checksum_type)

        # Determine compression type
        metadata.compression_type = cr.detect_compression(metadata.delta_fn)
        if (metadata.compression_type == cr.UNKNOWN_COMPRESSION):
            raise DeltaRepoError("Cannot detect compression type for {0}".format(
                    metadata.delta_fn))

        return metadata

    def apply(self):

        # Prepare output path
        os.mkdir(self.new_repodata_path)

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
            pluginbundle = self.deltametadata.get_pluginbundle(plugin.NAME)
            if pluginbundle.version > plugin.VERSION:
                raise DeltaRepoError("Delta of {0} metadata is generated by "
                    "plugin {1} with version: {2}, but locally available "
                    "is only version: {3}".format(metadata_objects.keys(),
                    plugin.NAME, pluginbundle.version, plugin.VERSION))

            # Use the plugin
            self._debug("Plugin {0}: Active".format(plugin.NAME))
            plugin_instance = plugin(pluginbundle, self.globalbundle,
                                     logger=self._get_logger())
            plugin_instance.apply(metadata_objects)

            # Put repomd records from processed metadatas to repomd
            for md in metadata_objects.values():
                self._debug("Plugin {0}: Processed \"{1}\" delta record "\
                            "which produced:".format(
                            plugin.NAME, md.metadata_type))
                for repomd_record in md.generated_repomd_records:
                    self._debug(" - {0}".format(repomd_record.type))
                    self.new_repomd.set_record(repomd_record)

            # Organization stuff
            for md in metadata_objects.keys():
                processed_metadata.add(md)

        # Process rest of the metadata files
        metadata_objects = {}
        for rectype, rec in self.delta_records.items():
            if rectype == "deltametadata":
                continue
            if rectype not in processed_metadata:
                metadata_object = self._new_metadata(rectype)
                if metadata_object is not None:
                    self._debug("To be processed by general delta plugin: " \
                                "{0}".format(rectype))
                    metadata_objects[rectype] = metadata_object
                else:
                    self._debug("Not processed: {0} - SKIP".format(rectype))

        if metadata_objects:
            # Use the plugin
            pluginbundle = self.deltametadata.get_pluginbundle(GENERAL_PLUGIN.NAME)
            if pluginbundle.version > GENERAL_PLUGIN.VERSION:
                raise DeltaRepoError("Delta of {0} metadata is generated by "
                    "plugin {1} with version: {2}, but locally available "
                    "is only version: {3}".format(metadata_objects.keys(),
                    GENERAL_PLUGIN.NAME, pluginbundle.version, GENERAL_PLUGIN.VERSION))
            self._debug("Plugin {0}: Active".format(GENERAL_PLUGIN.NAME))
            plugin_instance = GENERAL_PLUGIN(pluginbundle, self.globalbundle,
                                             logger=self._get_logger())
            plugin_instance.apply(metadata_objects)

            for md in metadata_objects.values():
                self._debug("Plugin {0}: Processed \"{1}\" delta record "\
                            "which produced:".format(
                            GENERAL_PLUGIN.NAME, md.metadata_type))
                for repomd_record in md.generated_repomd_records:
                    self._debug(" - {0}".format(repomd_record.type))
                    self.new_repomd.set_record(repomd_record)

        # Check if calculated repoids match
        self._debug("Checking expected repoids")

        self._debug("Calculated repoid of the old repo: {0}".format(
            self.globalbundle.calculated_old_repoid))
        self._debug("Calculated repoid of the new repo: {0}".format(
            self.globalbundle.calculated_new_repoid))

        if self.globalbundle.calculated_old_repoid:
            if self.old_id != self.globalbundle.calculated_old_repoid:
                message = "Repoid of the \"{0}\" repository doesn't match "\
                          "the real repoid ({1} != {2}).".format(
                           self.old_repo_path, self.old_id,
                           self.globalbundle.calculate_old_repoid)
                self._error(message)
                raise DeltaRepoError(message)
            else:
                self._debug("Repoid of the old repo matches ({0})".format(
                            self.old_id))
        else:
            self._warning("\"old_repoid\" item was not calculated.")

        if self.globalbundle.calculated_new_repoid:
            if self.new_id != self.globalbundle.calculated_new_repoid:
                message = "Repoid of the \"{0}\" repository doesn't match "\
                          "the real repoid ({1} != {2}).".format(
                           self.new_repo_path, self.new_id,
                           self.globalbundle.calculated_new_repoid)
                self._error(message)
                raise DeltaRepoError(message)
            else:
                self._debug("Repoid of the new repo matches ({0})".format(
                            self.new_id))
        else:
            self._warning("\"new_repoid\" item was not calculated.")

        # Prepare and write out the new repomd.xml
        self._debug("Preparing repomd.xml ...")
        self.new_repomd.set_repoid(self.new_id, self.repoid_type_str)
        self.new_repomd.sort_records()
        new_repomd_xml = self.new_repomd.xml_dump()

        self._debug("Writing repomd.xml ...")
        open(self.new_repomd_path, "w").write(new_repomd_xml)

        # Final move
        if os.path.exists(self.final_path):
            self._warning("Destination dir already exists! Removing %s" % \
                          self.final_path)
            shutil.rmtree(self.final_path)
        self._debug("Moving %s -> %s" % (self.new_repodata_path, self.final_path))
        os.rename(self.new_repodata_path, self.final_path)

