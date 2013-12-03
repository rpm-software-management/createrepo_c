import os
import os.path
import shutil
import hashlib
import createrepo_c as cr
from deltarepo.common import LoggingInterface
from deltarepo.errors import DeltaRepoError, DeltaRepoPluginError

PLUGINS = []
GENERAL_PLUGIN = None

class GlobalBundle(object):

    __slots__ = ("repoid_type_str",
                 "unique_md_filenames",
                 "calculated_old_repoid",
                 "calculated_new_repoid",
                 "force_database",
                 "ignore_missing")

    def __init__(self):
        self.repoid_type_str = "sha256"
        self.unique_md_filenames = True
        self.force_database = False
        self.ignore_missing = False

        # Filled by plugins
        self.calculated_old_repoid = None
        self.calculated_new_repoid = None

class DeltaRepoPlugin(LoggingInterface):

    # Plugin name
    NAME = ""

    # Plugin version (integer number!)
    VERSION = 1

    # List of Metadata this plugin takes care of.
    # The plugin HAS TO do deltas for each of listed metadata and be able
    # to apply deltas on them!
    METADATA = []

    def __init__(self, pluginbundle, globalbundle, logger=None):

        LoggingInterface.__init__(self, logger)

        # PluginBundle object.
        # This object store data in persistent way to the generated delta repodata.
        # This object is empty when gen() plugin method is called and plugin
        # should use it to store necessary information.
        # During apply() this object should be filled with data
        # previously stored during gen() method
        self.pluginbundle = pluginbundle

        # Global bundle carry
        self.globalbundle = globalbundle

    def gen_use_original(self, md):
        """Function that takes original metadata file, and use it as a delta
        Plugins could use this function when they cannot generate delta file
        for some reason (eg. file is newly added, so delta is
        meaningless/impossible)."""
        md.delta_fn = os.path.join(md.out_dir, os.path.basename(md.new_fn))
        shutil.copy2(md.new_fn, md.delta_fn)

        # Prepare repomd record of xml file
        rec = cr.RepomdRecord(md.metadata_type, md.delta_fn)
        rec.fill(md.checksum_type)
        if self.globalbundle.unique_md_filenames:
            rec.rename_file()

        md.generated_repomd_records.append(rec)

    def apply_use_original(self, md):
        """Reversal function for the gen_use_original"""
        md.new_fn = os.path.join(md.out_dir, os.path.basename(md.delta_fn))
        shutil.copy2(md.delta_fn, md.new_fn)

        # Prepare repomd record of xml file
        rec = cr.RepomdRecord(md.metadata_type, md.new_fn)
        rec.fill(md.checksum_type)
        if self.globalbundle.unique_md_filenames:
            rec.rename_file()

        md.generated_repomd_records.append(rec)

    def apply(self, metadata):
        """
        :arg metadata: Dict with available metadata listed in METADATA.
            key is metadata type (e.g. "primary", "filelists", ...)
            value is Metadata object
            This method is called only if at least one metadata listed
            in METADATA are found in delta repomd.

        Apply method has to do:
         * Raise DeltaRepoPluginError if something is bad
         * Build a new filename and create a file for each metadata and
           store it to Metadata Object.
        """
        raise NotImplementedError("Not implemented")

    def gen(self, metadata):
        raise NotImplementedError("Not implemented")


class GeneralDeltaRepoPlugin(DeltaRepoPlugin):

    NAME = "GeneralDeltaPlugin"
    VERSION = 1
    METADATA = []

    def _path(self, path, record):
        """Return path to the repodata file."""
        return os.path.join(path, record.location_href)

    def apply(self, metadata):
        for md in metadata.values():
            self.apply_use_original(md)

    def gen(self, metadata):
        ## TODO: Compress uncompressed data
        for md in metadata.values():
            self.gen_use_original(md)

GENERAL_PLUGIN = GeneralDeltaRepoPlugin


class MainDeltaRepoPlugin(DeltaRepoPlugin):

    NAME = "MainDeltaPlugin"
    VERSION = 1
    METADATA = ["primary", "filelists", "other",
                "primary_db", "filelists_db", "other_db"]

    def _path(self, path, record):
        """Return path to the repodata file."""
        return os.path.join(path, record.location_href)

    def _pkg_id_tuple(self, pkg):
        """Return tuple identifying a package in repodata.
        (pkgId, location_href, location_base)"""
        return (pkg.pkgId, pkg.location_href, pkg.location_base)

    def _pkg_id_str(self, pkg):
        """Return string identifying a package in repodata.
        This strings are used for the RepoId calculation."""
        if not pkg.pkgId:
            self._warning("Missing pkgId in a package!")
        if not pkg.location_href:
            self._warning("Missing location_href at package %s %s" % \
                          (pkg.name, pkg.pkgId))

        idstr = "%s%s%s" % (pkg.pkgId or '',
                          pkg.location_href or '',
                          pkg.location_base or '')
        return idstr

    def apply(self, metadata):

        gen_db_for = set([])
        only_copied_metadata = set({})
        removed_packages = {}

        # Make a set of md_types for which databases should be generated
        for record in self.pluginbundle.get_list("metadata", []):
            mdtype = record.get("type")
            if not mdtype:
                continue
            if self.globalbundle.force_database or record.get("database", "") == "1":
                gen_db_for.add(mdtype)
            if record.get("original", "") == "1":
                only_copied_metadata.add(mdtype)

        # Make a dict of removed packages key is location_href,
        # value is location_base
        for record in self.pluginbundle.get_list("removedpackage", []):
            location_href = record.get("location_href")
            if not location_href:
                continue
            location_base = record.get("location_base")
            removed_packages[location_href] = location_base

        # Check input arguments
        if "primary" not in metadata:
            self._error("primary.xml metadata file is missing")
            raise DeltaRepoPluginError("Primary metadata missing")

        # Metadata that no need to be processed, because they are just copies.
        # This metadata are copied and compressed xml (filelists or other)
        # Their sqlite databases have to be generated if they are required
        no_processed_metadata = []

        pri_md = metadata.get("primary")
        fil_md = metadata.get("filelists")
        oth_md = metadata.get("other")

        # Build and prepare destination paths
        # (And store them in the same Metadata object)
        def prepare_paths_in_metadata(md, xmlclass, dbclass):
            if md is None:
                return

            md.is_a_copy = False
            md.could_be_processed = True

            if md.metadata_type in only_copied_metadata:
                # Metadata file in delta repo is a copy (not a delta).
                # We don't want to process this file as delta file
                no_processed_metadata.append(md)
                md.is_a_copy = True
                return

            if not md.old_fn:
                # The old file is missing, if the corresponding file
                # in the deltarepo is not a copy (but just a delta),
                # this metadata cannot not be generated! (because
                # there is no file to which delta could be applied)
                md.could_be_processed = False
                return

            if not md.delta_fn:
                # The delfa file is missing. Thus delta cannot be applied
                md.could_be_processed = False
                return

            suffix = cr.compression_suffix(md.compression_type) or ""
            md.new_fn = os.path.join(md.out_dir,
                                     "{0}.xml{1}".format(
                                     md.metadata_type, suffix))
            md.new_f_stat = cr.ContentStat(md.checksum_type)
            md.new_f = xmlclass(md.new_fn,
                                md.compression_type,
                                md.new_f_stat)

            if md.metadata_type in gen_db_for:
                md.db_fn = os.path.join(md.out_dir, "{0}.sqlite".format(
                                        md.metadata_type))
                md.db = dbclass(md.db_fn)
            else:
                md.db_fn = None
                md.db = None

        # Primary
        prepare_paths_in_metadata(pri_md,
                                  cr.PrimaryXmlFile,
                                  cr.PrimarySqlite)

        # Filelists
        prepare_paths_in_metadata(fil_md,
                                  cr.FilelistsXmlFile,
                                  cr.FilelistsSqlite)

        # Other
        prepare_paths_in_metadata(oth_md,
                                  cr.OtherXmlFile,
                                  cr.OtherSqlite)

        # Apply delta

        all_packages = {}        # dict { 'pkgId': pkg }

        old_repoid_strings = []
        new_repoid_strings = []

        def old_pkgcb(pkg):
            old_repoid_strings.append(self._pkg_id_str(pkg))
            if pkg.location_href in removed_packages:
                if removed_packages[pkg.location_href] == pkg.location_base:
                    # This package won't be in new metadata
                    return
            new_repoid_strings.append(self._pkg_id_str(pkg))
            all_packages[pkg.pkgId] = pkg

        def delta_pkgcb(pkg):
            new_repoid_strings.append(self._pkg_id_str(pkg))
            all_packages[pkg.pkgId] = pkg

        do_primary_files = True
        if fil_md and (fil_md.is_a_copy or fil_md.could_be_processed):
            # We got:
            # - whole original filelists.xml, we don't need parse
            #   files from primary.xml files.
            # OR:
            # - We got old file and delta file so again we don't
            #   need parse files from primary.xml files.
            do_primary_files = False
        else:
            self._debug("Filelist's entries are going to be parsed "
                        "from primary.xml because filelists.xml content"
                        "is not available.")

        # Parse both old and delta primary.xml files
        cr.xml_parse_primary(pri_md.old_fn, pkgcb=old_pkgcb,
                             do_files=do_primary_files)
        cr.xml_parse_primary(pri_md.delta_fn, pkgcb=delta_pkgcb,
                             do_files=do_primary_files)

        # Calculate RepoIds
        old_repoid = ""
        new_repoid = ""

        h = hashlib.new(self.globalbundle.repoid_type_str)
        old_repoid_strings.sort()
        for i in old_repoid_strings:
            h.update(i)
        old_repoid = h.hexdigest()

        h = hashlib.new(self.globalbundle.repoid_type_str)
        new_repoid_strings.sort()
        for i in new_repoid_strings:
            h.update(i)
        new_repoid = h.hexdigest()

        # Sort packages
        def cmp_pkgs(x, y):
            # Compare only by filename
            ret = cmp(os.path.basename(x.location_href),
                      os.path.basename(y.location_href))
            if ret != 0:
                return ret

            # Compare by full location_href path
            return  cmp(x.location_href, y.location_href)

        all_packages_sorted = sorted(all_packages.values(), cmp=cmp_pkgs)

        def newpkgcb(pkgId, name, arch):
            return all_packages.get(pkgId, None)

        # Parse filelists
        if fil_md and fil_md.is_a_copy:
            # We got the original filelists.xml in the delta repo
            # So it is sufficient to parse only this filelists.xml
            cr.xml_parse_filelists(fil_md.delta_fn, newpkgcb=newpkgcb)
        elif fil_md and fil_md.could_be_processed:
            cr.xml_parse_filelists(fil_md.old_fn, newpkgcb=newpkgcb)
            cr.xml_parse_filelists(fil_md.delta_fn, newpkgcb=newpkgcb)

        # Parse other
        # TODO
        #if oth_md and oth_md.is_a_copy:
        #    # We got the original other.xml in the delta repo
        #    # So it is sufficient to parse only this other.xml
        #    cr.xml_parse_other(oth_md.delta_fn, newpkgcb=newpkgcb)
        if oth_md and oth_md.could_be_processed and not oth_md.is_a_copy:
            cr.xml_parse_other(oth_md.old_fn, newpkgcb=newpkgcb)
            cr.xml_parse_other(oth_md.delta_fn, newpkgcb=newpkgcb)

        num_of_packages = len(all_packages_sorted)

        # Write out primary
        pri_md.new_f.set_num_of_pkgs(num_of_packages)
        for pkg in all_packages_sorted:
            pri_md.new_f.add_pkg(pkg)
            if pri_md.db:
                pri_md.db.add_pkg(pkg)

        # Write out filelists
        if fil_md and fil_md.could_be_processed and not fil_md.is_a_copy:
            fil_md.new_f.set_num_of_pkgs(num_of_packages)
            for pkg in all_packages_sorted:
                fil_md.new_f.add_pkg(pkg)
                if fil_md.db:
                    fil_md.db.add_pkg(pkg)

        # Write out other
        if oth_md and oth_md.could_be_processed and not oth_md.is_a_copy:
            oth_md.new_f.set_num_of_pkgs(num_of_packages)
            for pkg in all_packages_sorted:
                oth_md.new_f.add_pkg(pkg)
                if oth_md.db:
                    oth_md.db.add_pkg(pkg)

        # Finish metadata
        def finish_metadata(md):
            if md is None or not md.could_be_processed or md.is_a_copy:
                return

            # Close XML file
            md.new_f.close()

            # Prepare repomd record of xml file
            rec = cr.RepomdRecord(md.metadata_type, md.new_fn)
            rec.load_contentstat(md.new_f_stat)
            rec.fill(md.checksum_type)
            if self.globalbundle.unique_md_filenames:
                rec.rename_file()

            md.generated_repomd_records.append(rec)

            # Prepare database
            if hasattr(md, "db") and md.db:
                md.db.dbinfo_update(rec.checksum)
                md.db.close()
                db_stat = cr.ContentStat(md.checksum_type)
                db_compressed = md.db_fn+".bz2"
                cr.compress_file(md.db_fn, None, cr.BZ2, db_stat)
                os.remove(md.db_fn)

                # Prepare repomd record of database file
                db_rec = cr.RepomdRecord("{0}_db".format(md.metadata_type),
                                         db_compressed)
                db_rec.load_contentstat(db_stat)
                db_rec.fill(md.checksum_type)
                if self.globalbundle.unique_md_filenames:
                    db_rec.rename_file()

                md.generated_repomd_records.append(db_rec)

        # Add records to the bundle

        finish_metadata(pri_md)
        finish_metadata(fil_md)
        finish_metadata(oth_md)

        # Process XML files that was just copied
        def finish_skipped_metadata(md):
            if md is None:
                return

            if not md.old_fn and not md.is_a_copy:
                # This metadata file is not a copy
                # This is a delta
                # But we don't have a original file on which the delta
                # could be applied
                msg = "Old file of type {0} is missing. The delta " \
                      "file {1} thus cannot be applied.".format(
                        md.metadata_type, md.delta_fn)
                self._warning(msg)
                if not self.globalbundle.ignore_missing:
                    raise DeltaRepoPluginError(msg + "Use --ignore-missing "
                                                     "to ignore this")
                return

            # Copy the file here
            self.apply_use_original(md)

            # Check if sqlite should be generated
            if md.metadata_type in gen_db_for:
                md.db_fn = os.path.join(md.out_dir, "{0}.sqlite".format(
                                        md.metadata_type))

                # Note: filelists (if needed) has been already parsed.
                # Because it has to be parsed before primary.xml is written
                if md.metadata_type == "filelists":
                    md.db = cr.FilelistsSqlite(md.db_fn)
                elif md.metadata_type == "other":
                    md.db = cr.OtherSqlite(md.db_fn)
                    cr.xml_parse_other(md.delta_fn, newpkgcb=newpkgcb)

                for pkg in all_packages_sorted:
                    md.db.add_pkg(pkg)

                md.db.dbinfo_update(md.delta_checksum)
                md.db.close()
                db_stat = cr.ContentStat(md.checksum_type)
                db_compressed = md.db_fn+".bz2"
                cr.compress_file(md.db_fn, None, cr.BZ2, db_stat)
                os.remove(md.db_fn)

                # Prepare repomd record of database file
                db_rec = cr.RepomdRecord("{0}_db".format(md.metadata_type),
                                         db_compressed)
                db_rec.load_contentstat(db_stat)
                db_rec.fill(md.checksum_type)
                if self.globalbundle.unique_md_filenames:
                    db_rec.rename_file()

                md.generated_repomd_records.append(db_rec)

        for md in no_processed_metadata:
            finish_skipped_metadata(md)

        self.globalbundle.calculated_old_repoid = old_repoid
        self.globalbundle.calculated_new_repoid = new_repoid

    def gen(self, metadata):
        # Check input arguments
        if "primary" not in metadata:
            self._error("primary.xml metadata file is missing")
            raise DeltaRepoPluginError("Primary metadata missing")

        # Metadata for which no delta will be generated
        no_processed = []

        # Medadata info that will be persistently stored
        persistent_metadata_info = {}

        pri_md = metadata.get("primary")
        fil_md = metadata.get("filelists")
        oth_md = metadata.get("other")

        # Build and prepare destination paths
        # (And store them in the same Metadata object)
        def prepare_paths_in_metadata(md, xmlclass, dbclass):
            if md is None:
                return

            # Make a note to the removed obj if the database should be generated
            db_type = "{0}_db".format(md.metadata_type)
            db_available = metadata.get(db_type)
            if db_available or self.globalbundle.force_database:
                persistent_metadata_info.setdefault(md.metadata_type, {})["database"] = "1"
            else:
                persistent_metadata_info.setdefault(md.metadata_type, {})["database"] = "0"

            if not md.old_fn:
                # Old file of this piece of metadata doesn't exist
                # This file has to be newly added.
                no_processed.append(md)
                md.skip = True
                return
            md.skip = False

            suffix = cr.compression_suffix(md.compression_type) or ""
            md.delta_fn = os.path.join(md.out_dir,
                                     "{0}.xml{1}".format(
                                     md.metadata_type, suffix))
            md.delta_f_stat = cr.ContentStat(md.checksum_type)
            md.delta_f = xmlclass(md.delta_fn,
                                  md.compression_type,
                                  md.delta_f_stat)
            # Database for delta repo is redundant
            #md.db_fn = os.path.join(md.out_dir, "{0}.sqlite".format(
            #                        md.metadata_type))
            #md.db = dbclass(md.db_fn)

        # Primary
        prepare_paths_in_metadata(pri_md,
                                  cr.PrimaryXmlFile,
                                  cr.PrimarySqlite)

        # Filelists
        prepare_paths_in_metadata(fil_md,
                                  cr.FilelistsXmlFile,
                                  cr.FilelistsSqlite)

        # Other
        prepare_paths_in_metadata(oth_md,
                                  cr.OtherXmlFile,
                                  cr.OtherSqlite)

        # Gen delta

        old_packages = set()
        added_packages = {}         # dict { 'pkgId': pkg }
        added_packages_ids = []     # list of package ids

        old_repoid_strings = []
        new_repoid_strings = []

        def old_pkgcb(pkg):
            old_packages.add(self._pkg_id_tuple(pkg))
            old_repoid_strings.append(self._pkg_id_str(pkg))

        def new_pkgcb(pkg):
            new_repoid_strings.append(self._pkg_id_str(pkg))
            pkg_id_tuple = self._pkg_id_tuple(pkg)
            if not pkg_id_tuple in old_packages:
                # This package is only in new repodata
                added_packages[pkg.pkgId] = pkg
                added_packages_ids.append(pkg.pkgId)
            else:
                # This package is also in the old repodata
                old_packages.remove(pkg_id_tuple)

        do_new_primary_files = 1
        if fil_md and not fil_md.skip and fil_md.delta_f and fil_md.new_fn:
            # All files will be parsed from filelists
            do_new_primary_files = 0

        cr.xml_parse_primary(pri_md.old_fn, pkgcb=old_pkgcb, do_files=0)
        cr.xml_parse_primary(pri_md.new_fn, pkgcb=new_pkgcb,
                             do_files=do_new_primary_files)

        # Calculate RepoIds
        old_repo_id = ""
        new_repo_id = ""

        h = hashlib.new(self.globalbundle.repoid_type_str)
        old_repoid_strings.sort()
        for i in old_repoid_strings:
            h.update(i)
        old_repoid = h.hexdigest()

        h = hashlib.new(self.globalbundle.repoid_type_str)
        new_repoid_strings.sort()
        for i in new_repoid_strings:
            h.update(i)
        new_repoid = h.hexdigest()

        # Prepare list of removed packages
        removed_pkgs = sorted(old_packages)
        for _, location_href, location_base in removed_pkgs:
            dictionary = {"location_href": location_href}
            if location_base:
                dictionary["location_base"] = location_base
            self.pluginbundle.append("removedpackage", dictionary)

        num_of_packages = len(added_packages)

        # Filelists and Other cb
        def newpkgcb(pkgId, name, arch):
            return added_packages.get(pkgId, None)

        # Write out filelists delta
        if fil_md and not fil_md.skip and fil_md.delta_f and fil_md.new_fn:
            cr.xml_parse_filelists(fil_md.new_fn, newpkgcb=newpkgcb)
            fil_md.delta_f.set_num_of_pkgs(num_of_packages)
            for pkgid in added_packages_ids:
                fil_md.delta_f.add_pkg(added_packages[pkgid])
            fil_md.delta_f.close()

        # Write out other delta
        if oth_md and not oth_md.skip and oth_md.delta_f and oth_md.new_fn:
            cr.xml_parse_other(oth_md.new_fn, newpkgcb=newpkgcb)
            oth_md.delta_f.set_num_of_pkgs(num_of_packages)
            for pkgid in added_packages_ids:
                oth_md.delta_f.add_pkg(added_packages[pkgid])
            oth_md.delta_f.close()

        # Write out primary delta
        # Note: Writing of primary delta has to be after parsing of filelists
        # Otherway cause missing files if do_new_primary_files was 0
        pri_md.delta_f.set_num_of_pkgs(num_of_packages)
        for pkgid in added_packages_ids:
            pri_md.delta_f.add_pkg(added_packages[pkgid])
        pri_md.delta_f.close()

        # Finish metadata
        def finish_metadata(md):
            if md is None or md.skip:
                return

            # Close XML file
            md.delta_f.close()

            # Prepare repomd record of xml file
            rec = cr.RepomdRecord(md.metadata_type, md.delta_fn)
            rec.load_contentstat(md.delta_f_stat)
            rec.fill(md.checksum_type)
            if self.globalbundle.unique_md_filenames:
                rec.rename_file()

            md.generated_repomd_records.append(rec)

            # Prepare database
            if hasattr(md, "db") and md.db:
                md.db.dbinfo_update(rec.checksum)
                md.db.close()
                db_stat = cr.ContentStat(md.checksum_type)
                db_compressed = md.db_fn+".bz2"
                cr.compress_file(md.db_fn, None, cr.BZ2, db_stat)
                os.remove(md.db_fn)

                # Prepare repomd record of database file
                db_rec = cr.RepomdRecord("{0}_db".format(md.metadata_type),
                                         db_compressed)
                db_rec.load_contentstat(db_stat)
                db_rec.fill(md.checksum_type)
                if self.globalbundle.unique_md_filenames:
                    db_rec.rename_file()

                md.generated_repomd_records.append(db_rec)

        # Add records to medata objects
        finish_metadata(pri_md)
        finish_metadata(fil_md)
        finish_metadata(oth_md)

        # Process no processed items
        # This files are just copied and compressed, no real delta
        for md in no_processed:
            self.gen_use_original(md)
            persistent_metadata_info.setdefault(md.metadata_type, {})["original"] = "1"

        # Store data persistently
        for key, val in persistent_metadata_info.items():
            val["type"] = key
            self.pluginbundle.append("metadata", val)

        self.globalbundle.calculated_old_repoid = old_repoid
        self.globalbundle.calculated_new_repoid = new_repoid

PLUGINS.append(MainDeltaRepoPlugin)
