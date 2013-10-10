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
from deltarepo.delta_plugins import PLUGINS
from deltarepo.errors import DeltaRepoError, DeltaRepoPluginError

__all__ = ['DeltaRepoGenerator']

class DeltaRepoGenerator(LoggingInterface):

    def __init__(self,
                 old_repo_path,
                 new_repo_path,
                 out_path=None,
                 logger=None,
                 repoid_type="sha256",
                 compression_type="xz"):

        # Initialization

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

        # Repoid type
        self.repoid_type_str = repoid_type or "sha256"
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

        self.old_id = self.old_repomd.repoid
        self.new_id = self.new_repomd.repoid

        # Prepare removed xml object
        self.removedxmlobj = RemovedXml()

        # Prepare bundle
        self.bundle = {}
        self.bundle["repoid_type_str"] = self.repoid_type_str
        self.bundle["removed_obj"] = self.removedxmlobj
        self.bundle["unique_md_filenames"] = self.unique_md_filenames

    def _new_metadata(self, metadata_type):
        """Return Metadata Object for the metadata_type or None"""

        if metadata_type not in self.new_records:
            return None

        metadata = Metadata(metadata_type)

        # Build new filename
        metadata.new_fn = os.path.join(self.new_repo_path,
                            self.new_records[metadata_type].location_href)

        # Build old filename
        if metadata_type in self.old_records:
            metadata.old_fn = os.path.join(self.old_repo_path,
                            self.old_records[metadata_type].location_href)

        # Set output directory
        metadata.out_dir = self.delta_repodata_path

        # Determine checksum type (for this specific repodata)
        # Detected checksum type should be probably same as the globally
        # detected one, but it is not guaranted
        metadata.checksum_type = cr.checksum_type(
                self.new_records[metadata_type].checksum_type)

        # Determine compression type
        metadata.compression_type = cr.detect_compression(metadata.new_fn)
        if (metadata.compression_type == cr.UNKNOWN_COMPRESSION):
            raise DeltaRepoError("Cannot detect compression type for {0}".format(
                    metadata.new_fn))

        return metadata

    def gen(self):

        # Prepare output path
        os.mkdir(self.delta_repodata_path)

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
                required_bundle_keys = set(plugin.GEN_REQUIRED_BUNDLE_KEYS)
                bundle_keys = set(self.bundle.keys())
                if not required_bundle_keys.issubset(bundle_keys):
                    self._debug("Plugin {0}: Skipped - Bundle keys {1} "\
                                "are not available".format(plugin.NAME,
                                (required_bundle_keys - bundle_keys)))
                    continue

                # Use the plugin
                self._debug("Plugin {0}: Active".format(plugin.NAME))
                plugin_instance = plugin()
                plugin_instance.gen(metadata_objects, self.bundle)

                # Check what bundle keys was added by the plugin
                new_bundle_keys = set(self.bundle.keys())
                diff = new_bundle_keys - bundle_keys
                if diff != set(plugin.GEN_BUNDLE_CONTRIBUTION):
                    message = "Plugin {0}: Error - Plugin should add: {1} " \
                               "bundle items but add: {2}".format(
                               plugin.NAME, plugin.GEN_BUNDLE_CONTRIBUTION,
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
                        self.delta_repomd.set_record(repomd_record)

                # Organization stuff
                processed_metadata.update(set(metadata_objects.keys()))
                used_plugins.add(plugin)
                plugin_used = True

        # TODO:
        # Process rest of the metadata files

        # Write out removed.xml
        self._debug("Writing removed.xml ...")
        removedxml_xml = self.removedxmlobj.xml_dump()
        removedxml_path = os.path.join(self.delta_repodata_path, "removed.xml")

        if (self.compression_type != cr.UNKNOWN_COMPRESSION):
            removedxml_path += cr.compression_suffix(self.compression_type)
            stat = cr.ContentStat(self.checksum_type)
            f = cr.CrFile(removedxml_path, cr.MODE_WRITE, cr.XZ, stat)
            f.write(removedxml_xml)
            f.close()
        else:
            open(removedxml_path, "w").write(removedxml_xml)

        removedxml_rec = cr.RepomdRecord("removed", removedxml_path)
        removedxml_rec.load_contentstat(stat)
        removedxml_rec.fill(self.checksum_type)
        if self.unique_md_filenames:
            removedxml_rec.rename_file()
        self.delta_repomd.set_record(removedxml_rec)

        # Check if calculated repoids match
        self._debug("Checking expected repoids")

        if not "new_repoid" in self.bundle or not "old_repoid" in self.bundle:
            message = "\"new_repoid\" or \"old_repoid\" is missing in bundle"
            self._error(message)
            raise DeltaRepoError(message)

        if self.old_id:
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
            self._debug("Repoid of the \"{0}\" is not part of its "\
                        "repomd".format(self.old_repo_path))

        if self.new_id:
            if self.new_id and self.new_id != self.bundle["new_repoid"]:
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
            self._debug("Repoid of the \"{0}\" is not part of its "\
                        "repomd".format(self.new_repo_path))

        # Prepare and write out the new repomd.xml
        self._debug("Preparing repomd.xml ...")
        deltarepoid = "{0}-{1}".format(self.bundle["old_repoid"],
                                       self.bundle["new_repoid"])
        self.delta_repomd.set_repoid(deltarepoid, self.repoid_type_str)
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

