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
from deltarepo.common import LoggingInterface, Metadata, RemovedXml
from deltarepo.delta_plugins import PLUGINS, GENERAL_PLUGIN
from deltarepo.errors import DeltaRepoError, DeltaRepoPluginError

__all__ = ['DeltaRepoApplicator']

class DeltaRepoApplicator(LoggingInterface):

    def __init__(self,
                 old_repo_path,
                 delta_repo_path,
                 out_path=None,
                 logger=None):

        # Initialization

        LoggingInterface.__init__(self, logger)

        self.repoid_type = None
        self.unique_md_filenames = False
        self.databases = False
        self.removedxmlobj = RemovedXml()

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
        if "removed" in self.delta_records:
            self.removedxml_path = os.path.join(self.delta_repo_path,
                                   self.delta_records["removed"].location_href)
            self.removedxmlobj.xml_parse(self.removedxml_path)
        else:
            self._warning("\"removed\" record is missing in repomd.xml "\
                          "of delta repo")

        # Prepare bundle
        self.bundle = {}
        self.bundle["repoid_type_str"] = self.repoid_type_str
        self.bundle["removed_obj"] = self.removedxmlobj
        self.bundle["unique_md_filenames"] = self.unique_md_filenames

    def _new_metadata(self, metadata_type):
        """Return Metadata Object for the metadata_type or None"""

        if metadata_type not in self.delta_records:
            return None

        metadata = Metadata(metadata_type)

        # Build delta filename
        metadata.delta_fn = os.path.join(self.delta_repo_path,
                            self.delta_records[metadata_type].location_href)

        # Build old filename
        if metadata_type in self.old_records:
            metadata.old_fn = os.path.join(self.old_repo_path,
                            self.old_records[metadata_type].location_href)

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

        processed_metadata = set()
        used_plugins = set()
        plugin_used = True

        while plugin_used:
            # Iterate on plugins until any of them was used
            plugin_used = False

            for plugin in PLUGINS:

                # Use only plugins that haven't been already used
                if plugin in used_plugins:
                    continue

                # Check which metadata this plugin want to process
                conflicting_metadata = set(plugin.METADATA) & processed_metadata
                if conflicting_metadata:
                    message = "Plugin {0}: Error - Plugin want to process " \
                              "already processed metadata {1}".format(
                               plugin.NAME, conflicting_metadata)
                    self._error(message)
                    raise DeltaRepoError(message)

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
                    used_plugins.add(plugin)
                    continue

                # Check if bundle contains all what plugin need
                required_bundle_keys = set(plugin.APPLY_REQUIRED_BUNDLE_KEYS)
                bundle_keys = set(self.bundle.keys())
                if not required_bundle_keys.issubset(bundle_keys):
                    self._debug("Plugin {0}: Skipped - Bundle keys {1} "\
                                "are not available".format(plugin.NAME,
                                (required_bundle_keys - bundle_keys)))
                    continue

                # Use the plugin
                self._debug("Plugin {0}: Active".format(plugin.NAME))
                plugin_instance = plugin()
                plugin_instance.apply(metadata_objects, self.bundle)

                # Check what bundle keys was added by the plugin
                new_bundle_keys = set(self.bundle.keys())
                diff = new_bundle_keys - bundle_keys
                if diff != set(plugin.APPLY_BUNDLE_CONTRIBUTION):
                    message = "Plugin {0}: Error - Plugin should add: {1} " \
                               "bundle items but add: {2}".format(
                               plugin.NAME, plugin.APPLY_BUNDLE_CONTRIBUTION,
                               list(diff))
                    self._error(message)
                    raise DeltaRepoError(message)

                # Put repomd records from processed metadatas to repomd
                for md in metadata_objects.values():
                    self._debug("Plugin {0}: Processed \"{1}\" delta record "\
                                "which produced:".format(
                                plugin.NAME, md.metadata_type))
                    for repomd_record in md.generated_repomd_records:
                        self._debug(" - {0}".format(repomd_record.type))
                        self.new_repomd.set_record(repomd_record)

                # Organization stuff
                processed_metadata.update(set(metadata_objects.keys()))
                used_plugins.add(plugin)
                plugin_used = True

        # Process rest of the metadata files
        metadata_objects = {}
        for rectype, rec in self.delta_records.items():
            if rectype in ("primary_db", "filelists_db", "other_db", "removed"):
                # Skip databases and removed
                continue

            if rectype not in processed_metadata:
                metadata_object = self._new_metadata(rectype)
                if metadata_object is not None:
                    self._debug("To be processed by general delta plugin: %s" \
                                % rectype)
                    metadata_objects[rectype] = metadata_object
                else:
                    self._debug("Not processed: %s - SKIP", rectype)

        if metadata_objects:
            # Use the plugin
            self._debug("Plugin {0}: Active".format(GENERAL_PLUGIN.NAME))
            plugin_instance = GENERAL_PLUGIN()
            plugin_instance.apply(metadata_objects, self.bundle)

            for md in metadata_objects.values():
                self._debug("Plugin {0}: Processed \"{1}\" delta record "\
                            "which produced:".format(
                            GENERAL_PLUGIN.NAME, md.metadata_type))
                for repomd_record in md.generated_repomd_records:
                    self._debug(" - {0}".format(repomd_record.type))
                    self.new_repomd.set_record(repomd_record)

        self._debug("Used plugins: {0}".format([p.NAME for p in used_plugins]))

        # Check if calculated repoids match
        self._debug("Checking expected repoids")

        if "old_repoid" in self.bundle:
            if self.old_id != self.bundle["old_repoid"]:
                message = "Repoid of the \"{0}\" repository doesn't match "\
                          "the real repoid ({1} != {2}).".format(
                           self.old_repo_path, self.old_id,
                           self.bundle["old_repoid"])
                self._error(message)
                raise DeltaRepoError(message)
            else:
                self._debug("Repoid of the old repo matches ({0})".format(
                            self.old_id))
        else:
            self._warning("\"old_repoid\" item is missing in bundle.")

        if "new_repoid" in self.bundle:
            if self.new_id != self.bundle["new_repoid"]:
                message = "Repoid of the \"{0}\" repository doesn't match "\
                          "the real repoid ({1} != {2}).".format(
                           self.new_repo_path, self.new_id,
                           self.bundle["new_repoid"])
                self._error(message)
                raise DeltaRepoError(message)
            else:
                self._debug("Repoid of the new repo matches ({0})".format(
                            self.new_id))
        else:
            self._warning("\"new_repoid\" item is missing in bundle.")

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

