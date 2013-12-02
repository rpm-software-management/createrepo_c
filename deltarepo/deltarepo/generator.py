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
from deltarepo.common import LoggingInterface, Metadata, DeltaMetadata, PluginBundle
from deltarepo.delta_plugins import GlobalBundle, PLUGINS, GENERAL_PLUGIN
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

        self.deltametadata = DeltaMetadata()

        # Prepare global bundle
        self.globalbundle = GlobalBundle()
        self.globalbundle.repoid_type_str = self.repoid_type_str
        self.globalbundle.unique_md_filenames = self.unique_md_filenames

    def _new_metadata(self, metadata_type):
        """Return Metadata Object for the metadata_type or None"""

        if metadata_type not in self.new_records:
            return None

        metadata = Metadata(metadata_type)

        # Build new filename
        metadata.new_fn = os.path.join(self.new_repo_path,
                            self.new_records[metadata_type].location_href)
        if not os.path.isfile(metadata.new_fn):
            self._warning("The file {0} doesn't exist!".format(metadata.new_fn))
            return None

        # Build old filename
        if metadata_type in self.old_records:
            metadata.old_fn = os.path.join(self.old_repo_path,
                            self.old_records[metadata_type].location_href)
            if not os.path.isfile(metadata.old_fn):
                self._warning("File {0} doesn't exist!".format(metadata.old_fn))
                metadata.old_fn = None

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
            plugin_instance = plugin(pluginbundle, self.globalbundle)
            plugin_instance.gen(metadata_objects)

            # Put repomd records from processed metadatas to repomd
            for md in metadata_objects.values():
                self._debug("Plugin {0}: Processed \"{1}\" delta record "\
                            "which produced:".format(
                            plugin.NAME, md.metadata_type))
                for repomd_record in md.generated_repomd_records:
                    self._debug(" - {0}".format(repomd_record.type))
                    self.delta_repomd.set_record(repomd_record)

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
            self._debug("Plugin {0}: Active".format(GENERAL_PLUGIN.NAME))
            plugin_instance = GENERAL_PLUGIN(None, self.globalbundle)
            plugin_instance.gen(metadata_objects)

            for md in metadata_objects.values():
                self._debug("Plugin {0}: Processed \"{1}\" delta record "\
                            "which produced:".format(
                            GENERAL_PLUGIN.NAME, md.metadata_type))
                for repomd_record in md.generated_repomd_records:
                    self._debug(" - {0}".format(repomd_record.type))
                    self.delta_repomd.set_record(repomd_record)

        # Write out deltametadata.xml
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

        # Check if calculated repoids match
        self._debug("Checking expected repoids")

        if self.globalbundle.calculated_new_repoid is None \
                or self.globalbundle.calculated_old_repoid is None:
            message = "\"new_repoid\" or \"old_repoid\" wasn't calculated"
            self._error(message)
            raise DeltaRepoError(message)

        if self.old_id:
            if self.old_id != self.globalbundle.calculated_old_repoid:
                message = "Repoid of the \"{0}\" repository doesn't match "\
                          "the real repoid ({1} != {2}).".format(
                           self.old_repo_path, self.old_id,
                           self.globalbundle.calculated_old_repoid)
                self._error(message)
                raise DeltaRepoError(message)
            else:
                self._debug("Repoid of the old repo matches ({0})".format(
                            self.old_id))
        else:
            self._debug("Repoid of the \"{0}\" is not part of its "\
                        "repomd".format(self.old_repo_path))

        if self.new_id:
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
            self._debug("Repoid of the \"{0}\" is not part of its "\
                        "repomd".format(self.new_repo_path))

        # Prepare and write out the new repomd.xml
        self._debug("Preparing repomd.xml ...")
        deltarepoid = "{0}-{1}".format(self.globalbundle.calculated_old_repoid,
                                       self.globalbundle.calculated_new_repoid)
        self.delta_repomd.set_repoid(deltarepoid, self.repoid_type_str)
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

