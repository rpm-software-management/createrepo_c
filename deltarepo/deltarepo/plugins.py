import os
import os.path
import shutil
import hashlib
import filecmp
import createrepo_c as cr
from .plugins_common import GlobalBundle, Metadata
from .common import LoggingInterface, DEFAULT_CHECKSUM_NAME
from .errors import DeltaRepoPluginError

# List of available plugins
PLUGINS = []

# Mapping - which metadata from deltarepo must be downloaded
# to get desired metadata.
METADATA_MAPPING = {}  # { "wanted_metadata_type": ["required_metadata_from_deltarepo", ...] }

# Plugin to gen/apply deltas over any metadata
# (over data that are not supported by other plugins)
GENERAL_PLUGIN = None

# Files with this suffixes will be considered as already compressed
# The list is subset of:
#   http://en.wikipedia.org/wiki/List_of_archive_formats
# Feel free to extend this list
COMPRESSION_SUFFIXES = [".bz2", ".gz", ".lz", ".lzma", ".lzo", ".xz",
                        ".7z", ".s7z", ".apk", ".rar", ".sfx", ".tgz",
                        ".tbz2", ".tlz", ".zip", ".zipx", ".zz"]

class DeltaRepoPlugin(LoggingInterface):

    # Plugin name
    NAME = ""

    # Plugin version (integer number!)
    VERSION = 1

    # List of Metadata this plugin takes care of.
    # The plugin HAS TO do deltas for each of listed metadata and be able
    # to apply deltas on them!
    METADATA = []

    # Says which delta metadata are needed to get required metadata
    # e.g. { "primary": ["primary"], "filelists": ["primary", "filelists"] }
    METADATA_MAPPING = {}

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

        # Internal stuff
        self.__metadata_notes_cache = None

    def _log(self, level, msg):
        new_msg = "{0}: {1}".format(self.NAME, msg)
        LoggingInterface._log(self, level, new_msg)

    def _metadata_notes_from_plugin_bundle(self, type):
        """From the pluginbundle extract info about specific metadata element"""

        if self.__metadata_notes_cache is None:
            self.__metadata_notes_cache = {}
            for dict in self.pluginbundle.get_list("metadata", []):
                if "type" not in dict:
                    self._warning("Metadata element in deltametadata.xml hasn't "
                                  "an attribute 'type'")
                    continue
                self.__metadata_notes_cache[dict["type"]] = dict

        return self.__metadata_notes_cache.get(type)

    def _metadata_notes_to_plugin_bundle(self, type, dictionary):
        """Store info about metadata persistently to pluginbundle"""
        notes = {"type": type}
        notes.update(dictionary)
        self.pluginbundle.append("metadata", notes)

    def gen_use_original(self, md, compression_type=cr.NO_COMPRESSION):
        """Function that takes original metadata file and
        copy it to the delta repo unmodified.
        Plugins could use this function when they cannot generate delta file
        for some reason (eg. file is newly added, so delta is
        meaningless/impossible)."""

        md.delta_fn = os.path.join(md.out_dir, os.path.basename(md.new_fn))

        # Compress or copy original file
        stat = None
        if (compression_type != cr.NO_COMPRESSION):
            md.delta_fn += cr.compression_suffix(compression_type)
            stat = cr.ContentStat(md.checksum_type)
            cr.compress_file(md.new_fn, md.delta_fn, compression_type, stat)
        else:
            shutil.copy2(md.new_fn, md.delta_fn)

        # Prepare repomd record of xml file
        rec = cr.RepomdRecord(md.metadata_type, md.delta_fn)
        if stat is not None:
            rec.load_contentstat(stat)
        rec.fill(md.checksum_type)
        if self.globalbundle.unique_md_filenames:
            rec.rename_file()
            md.delta_fn = rec.location_real

        return rec

    def apply_use_original(self, md, decompress=False):
        """Reversal function for the gen_use_original"""
        md.new_fn = os.path.join(md.out_dir, os.path.basename(md.delta_fn))

        if decompress:
            md.new_fn = md.new_fn.rsplit('.', 1)[0]
            cr.decompress_file(md.delta_fn, md.new_fn, cr.AUTO_DETECT_COMPRESSION)
        else:
            shutil.copy2(md.delta_fn, md.new_fn)

        # Prepare repomd record of xml file
        rec = cr.RepomdRecord(md.metadata_type, md.new_fn)
        rec.fill(md.checksum_type)
        if self.globalbundle.unique_md_filenames:
            rec.rename_file()
            md.new_fn = rec.location_real

        return rec

    def _gen_basic_delta(self, md, force_gen=False):
        """Resolve some common situation during delta generation.

        There is some situation which could appear during
        delta generation:

        # - Metadata file has a record in repomd.xml and the file really exists
        O - Metadata file has a record in repomd.xml but the file is missing
        X - Metadata file doesn't have a record in repomd.xml

        Old repository | New repository
        ---------------|---------------
               #       |      #         - Valid case
               #       |      X         - Valid case - metadata was removed
               #       |      O         - Invalid case - incomplete repo
               X       |      #         - Valid case - metadata was added
               X       |      X         - This shouldn't happen
               X       |      O         - Invalid case - incomplete repo
               O       |      #         - Invalid case - incomplete repo
               O       |      X         - Invalid case - incomplete repo
               O       |      O         - Invalid case - both repos are incomplete

        By default, Deltarepo should raise an exception when a invalid
        case is meet. But user could use --ignore-missing option and
        in that case, the Deltarepo should handle all invalid case
        like a charm.

        For example:

        O | # - Just copy the new metadata to the delta repo as is
        O | X - Just ignore that the old metadata is missing
        O | O - Just ignore this
        # | O - Just ignore this
        X | O - Just ignore this

        Most delta plugins should be only interested to "# | #" use case.
        The case where we have the both, old and new, metadata available.
        Other cases are mostly not important to the delta plugins.
        This is the reason why this function exits. It should solve the
        cases when the sophisticated delta is not possible.

        Returns (SC - Status code,
                 REC - Repomd record,
                 NOTES - Dict with persistent notes)

        If RC is True, then delta plugin shouldn't continue with
        processing of this metadata.
        """

        if not md:
            # No metadata - Nothing to do
            return (True, None, None)

        md.delta_rec = None
        md.delta_fn_exists = False

        if not md.old_rec and not md.new_rec:
            # None metadata record exists.
            self._debug("\"{0}\": Doesn't exist "
                        "in any repo".format(md.metadata_type))
            return (True, None, None)

        if not md.new_rec:
            # This record doesn't exists in the new version of repodata
            # This metadata were removed in the new version of repo
            self._debug("\"{0}\": Removed in the new version of repodata"
                        "".format(md.metadata_type))
            return (True, None, None)

        if not md.new_fn_exists:
            # The new metadata file is missing
            assert self.globalbundle.ignore_missing
            self._warning("\"{0}\": Delta cannot be generated - new metadata "
                          "are missing".format(md.metadata_type))
            return (True, None, None)

        if not md.old_rec or not md.old_fn_exists or \
                (force_gen and not filecmp.cmp(md.old_fn, md.new_fn)):
            # This metadata was newly added in the new version of repodata
            # Or we are just missing the old version of this metadata
            # Or we have both versions of metadata but the metadata are not
            #    same and in that case we simply want to gen a delta as a copy
            if md.old_fn_exists:
                self._debug("\"{0}\": Newly added in the new version of repodata")
            elif not md.old_fn_exists:
                self._warning("\"{0}\": Delta cannot be generated - old metadata "
                              "are missing - Using copy of the new one"
                              "".format(md.metadata_type))
            else:
                self._debug("\"{0}\": Delta is just a copy of the new metadata")

            # Suffix based detection of compression
            compressed = False
            for suffix in COMPRESSION_SUFFIXES:
                if md.new_fn.endswith(suffix):
                    compressed = True
                    break

            compression = cr.NO_COMPRESSION
            if not compressed:
                compression = cr.XZ

            # Gen record
            rec = self.gen_use_original(md, compression_type=compression)

            notes = {}
            notes["original"] = '1'
            if compression != cr.NO_COMPRESSION:
                notes["compressed"] = "1"

            md.delta_rec = rec
            md.delta_fn_exists = True

            return (True, rec, notes)

        # At this point we are sure that we have both metadata files

        if filecmp.cmp(md.old_fn, md.new_fn):
            # Both metadata files exists and are the same
            self._debug("\"{0}\": Same in both version of repodata"
                        "".format(md.metadata_type))
            notes = {}
            if os.path.basename(md.old_fn) != os.path.basename(md.new_fn):
                notes["new_name"] = os.path.basename(md.new_fn)

            notes["unchanged"] = "1"
            notes["checksum_name"] = cr.checksum_name_str(md.checksum_type)
            return (True, None, notes)

        # Both metadata files exists and are different,
        # this is job for a real delta plugin :)
        return (False, None, None)

    def _apply_basic_delta(self, md, notes):
        """

        """

        if not md:
            # No metadata - Nothing to do
            return (True, None)

        # Init some stuff in md
        # This variables should be set only if new record was generated
        # Otherwise it should by None/False
        md.new_rec = None
        md.new_fn_exists = False

        if not notes:
            # No notes - Nothing to do
            return (True, None)

        if not md.old_rec and not md.delta_rec:
            # None metadata record exists.
            self._debug("\"{0}\": Doesn't exist "
                        "in any repo".format(md.metadata_type))
            return (True, None)

        if not md.delta_rec:
            # This record is missing in delta repo
            if notes.get("unchanged") != "1":
                # This metadata were removed in the new version of repo
                self._debug("\"{0}\": Removed in the new version of repodata"
                            "".format(md.metadata_type))
                return (True, None)

            # Copy from the old repo should be used
            if not md.old_fn_exists:
                # This is missing in the old repo
                self._warning("\"{0}\": From old repo should be used, but "
                              "it is missing".format(md.metadata_type))
                return (True, None)

            # Use copy from the old repo

            # Check if old file should have a new name
            basename = notes.get("new_name")
            if not basename:
                basename = os.path.basename(md.old_fn)

            md.new_fn = os.path.join(md.out_dir, basename)

            checksum_name = notes.get("checksum_name", DEFAULT_CHECKSUM_NAME)
            checksum_type = cr.checksum_type(checksum_name)

            # Copy the file and create repomd record
            shutil.copy2(md.old_fn, md.new_fn)
            rec = cr.RepomdRecord(md.metadata_type, md.new_fn)
            rec.fill(checksum_type)
            if self.globalbundle.unique_md_filenames:
                rec.rename_file()
                md.new_fn = rec.location_real

            md.new_rec = rec
            md.new_fn_exists = True

            return (True, rec)

        if not md.delta_fn_exists:
            # Delta is missing
            self._warning("\"{0}\": Delta file is missing"
                          "".format(md.metadata_type))
            return (True, None)

        # At this point we are sure, we have a delta file

        if notes.get("original") == "1":
            # Delta file is the target file

            # Check if file should be uncompressed
            decompress = False
            if notes.get("compressed") == "1":
                decompress = True

            rec = self.apply_use_original(md, decompress)
            self._debug("\"{0}\": Used delta is just a copy")

            md.new_rec = rec
            md.new_fn_exists = True

            return (True, rec)

        if not md.old_fn_exists:
            # Old file is missing
            self._warning("\"{0}\": Old file is missing"
                          "".format(md.metadata_type))
            return (True, None)

        # Delta file exists and it is not a copy nor metadata
        # file from old repo should be used.
        # this is job for a real delta plugin :)
        return (False, None)

    def apply(self, metadata):
        raise NotImplementedError("Not implemented")

    def gen(self, metadata):
        raise NotImplementedError("Not implemented")


class GeneralDeltaRepoPlugin(DeltaRepoPlugin):

    NAME = "GeneralDeltaPlugin"
    VERSION = 1
    METADATA = []
    METADATA_MAPPING = {}

    def gen(self, metadata):

        gen_repomd_recs = []

        for md in metadata.values():
            rc, rec, notes = self._gen_basic_delta(md, force_gen=True)
            assert rc
            if rec:
                gen_repomd_recs.append(rec)
            if notes:
                self._metadata_notes_to_plugin_bundle(md.metadata_type, notes)

        return gen_repomd_recs

    def apply(self, metadata):

        gen_repomd_recs = []

        for md in metadata.values():
            notes = self._metadata_notes_from_plugin_bundle(md.metadata_type)
            rc, rec = self._apply_basic_delta(md, notes)
            assert rc
            if rec:
                gen_repomd_recs.append(rec)

        return gen_repomd_recs

GENERAL_PLUGIN = GeneralDeltaRepoPlugin


class MainDeltaRepoPlugin(DeltaRepoPlugin):

    NAME = "MainDeltaPlugin"
    VERSION = 1
    METADATA = ["primary", "filelists", "other",
                "primary_db", "filelists_db", "other_db"]
    METADATA_MAPPING = {
        "primary":         ["primary"],
        "filelists":       ["primary", "filelists"],
        "other":           ["primary", "other"],
        "primary_db":      ["primary"],
        "filelists_db":    ["primary", "filelists"],
        "other_db":        ["primary", "other"],
    }

    def _pkg_id_tuple(self, pkg):
        """Return tuple identifying a package in repodata.
        (pkgId, location_href, location_base)"""
        return (pkg.pkgId, pkg.location_href, pkg.location_base)

    def _pkg_id_str(self, pkg):
        """Return string identifying a package in repodata.
        This strings are used for the content hash calculation."""
        if not pkg.pkgId:
            self._warning("Missing pkgId in a package!")
        if not pkg.location_href:
            self._warning("Missing location_href at package %s %s" % \
                          (pkg.name, pkg.pkgId))

        idstr = "%s%s%s" % (pkg.pkgId or '',
                          pkg.location_href or '',
                          pkg.location_base or '')
        return idstr

    def _gen_db_from_xml(self, md):
        """Gen sqlite db from the delta metadata.
        """
        mdtype = md.metadata_type

        if mdtype == "primary":
            dbclass = cr.PrimarySqlite
            parsefunc = cr.xml_parse_primary
        elif mdtype == "filelists":
            dbclass = cr.FilelistsSqlite
            parsefunc = cr.xml_parse_filelists
        elif mdtype == "other":
            dbclass = cr.OtherSqlite
            parsefunc = cr.xml_parse_other
        else:
            raise DeltaRepoPluginError("Unsupported type of metadata {0}".format(mdtype))

        src_fn = md.new_fn
        src_rec = md.new_rec

        md.db_fn = os.path.join(md.out_dir, "{0}.sqlite".format(mdtype))
        db = dbclass(md.db_fn)

        def pkgcb(pkg):
            db.add_pkg(pkg)

        parsefunc(src_fn, pkgcb=pkgcb)

        db.dbinfo_update(src_rec.checksum)
        db.close()

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

        return db_rec

    def apply(self, metadata):
        # Check input arguments
        if "primary" not in metadata:
            self._error("primary.xml metadata file is missing")
            raise DeltaRepoPluginError("Primary metadata missing")

        gen_repomd_recs = []

        removed_packages = {}

        pri_md = metadata.get("primary")
        fil_md = metadata.get("filelists")
        oth_md = metadata.get("other")

        def try_simple_delta(md, dbclass):
            if not md:
                return

            notes = self._metadata_notes_from_plugin_bundle(md.metadata_type)
            if not notes:
                self._warning("Metadata \"{0}\" doesn't have a record in "
                              "deltametadata.xml - Ignoring")
                return True
            rc, rec = self._apply_basic_delta(md, notes)
            if not rc:
                return False
            if rec:
                gen_repomd_recs.append(rec)

            if not md.new_fn_exists:
                return True

            # Gen DB here
            if self.globalbundle.force_database or notes.get("database") == "1":
                rec = self._gen_db_from_xml(md)
                gen_repomd_recs.append(rec)

            return True

        # At first try to simple delta

        simple_pri_delta = try_simple_delta(pri_md, cr.PrimarySqlite)
        simple_fil_delta = try_simple_delta(fil_md, cr.FilelistsSqlite)
        simple_oth_delta = try_simple_delta(oth_md, cr.OtherSqlite)

        if simple_pri_delta:
            assert simple_fil_delta
            assert simple_oth_delta
            return gen_repomd_recs

        # Ignore already processed metadata
        if simple_fil_delta:
            fil_md = None
        if simple_oth_delta:
            oth_md = None

        # Make a dict of removed packages key is location_href,
        # value is location_base
        for record in self.pluginbundle.get_list("removedpackage", []):
            location_href = record.get("location_href")
            if not location_href:
                continue
            location_base = record.get("location_base")
            removed_packages[location_href] = location_base

        # Prepare output xml files and check if dbs should be generated
        # Note: This information are stored directly to the Metadata
        # object which someone could see as little hacky.
        def prepare_paths_in_metadata(md, xmlclass, dbclass):
            if md is None:
                return

            notes = self._metadata_notes_from_plugin_bundle(md.metadata_type)
            if not notes:
                # TODO: Add flag to ignore this kind of warnings (?)
                self._warning("Metadata \"{0}\" doesn't have a record in "
                              "deltametadata.xml - Ignoring")
                return

            suffix = cr.compression_suffix(md.compression_type) or ""
            md.new_fn = os.path.join(md.out_dir,
                                     "{0}.xml{1}".format(
                                     md.metadata_type, suffix))
            md.new_f_stat = cr.ContentStat(md.checksum_type)
            md.new_f = xmlclass(md.new_fn,
                                md.compression_type,
                                md.new_f_stat)

            if self.globalbundle.force_database or notes.get("database") == "1":
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

        old_contenthash_strings = []
        new_contenthash_strings = []

        def old_pkgcb(pkg):
            old_contenthash_strings.append(self._pkg_id_str(pkg))
            if pkg.location_href in removed_packages:
                if removed_packages[pkg.location_href] == pkg.location_base:
                    # This package won't be in new metadata
                    return
            new_contenthash_strings.append(self._pkg_id_str(pkg))
            all_packages[pkg.pkgId] = pkg

        def delta_pkgcb(pkg):
            new_contenthash_strings.append(self._pkg_id_str(pkg))
            all_packages[pkg.pkgId] = pkg

        filelists_from_primary = True
        if fil_md:
            filelists_from_primary = False

        # Parse both old and delta primary.xml files
        cr.xml_parse_primary(pri_md.old_fn, pkgcb=old_pkgcb,
                             do_files=filelists_from_primary)
        cr.xml_parse_primary(pri_md.delta_fn, pkgcb=delta_pkgcb,
                             do_files=filelists_from_primary)

        # Calculate content hashes
        h = hashlib.new(self.globalbundle.contenthash_type_str)
        old_contenthash_strings.sort()
        for i in old_contenthash_strings:
            h.update(i)
        self.globalbundle.calculated_old_contenthash = h.hexdigest()

        h = hashlib.new(self.globalbundle.contenthash_type_str)
        new_contenthash_strings.sort()
        for i in new_contenthash_strings:
            h.update(i)
        self.globalbundle.calculated_new_contenthash = h.hexdigest()

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
        if fil_md:
            self._debug("Parsing filelists xmls")
            cr.xml_parse_filelists(fil_md.old_fn, newpkgcb=newpkgcb)
            cr.xml_parse_filelists(fil_md.delta_fn, newpkgcb=newpkgcb)

        if oth_md:
            self._debug("Parsing other xmls")
            cr.xml_parse_other(oth_md.old_fn, newpkgcb=newpkgcb)
            cr.xml_parse_other(oth_md.delta_fn, newpkgcb=newpkgcb)

        num_of_packages = len(all_packages_sorted)

        # Write out primary
        self._debug("Writing primary xml: {0}".format(pri_md.new_fn))
        pri_md.new_f.set_num_of_pkgs(num_of_packages)
        for pkg in all_packages_sorted:
            pri_md.new_f.add_pkg(pkg)
            if pri_md.db:
                pri_md.db.add_pkg(pkg)

        # Write out filelists
        if fil_md:
            self._debug("Writing filelists xml: {0}".format(fil_md.new_fn))
            fil_md.new_f.set_num_of_pkgs(num_of_packages)
            for pkg in all_packages_sorted:
                fil_md.new_f.add_pkg(pkg)
                if fil_md.db:
                    fil_md.db.add_pkg(pkg)

        # Write out other
        if oth_md:
            self._debug("Writing other xml: {0}".format(oth_md.new_fn))
            oth_md.new_f.set_num_of_pkgs(num_of_packages)
            for pkg in all_packages_sorted:
                oth_md.new_f.add_pkg(pkg)
                if oth_md.db:
                    oth_md.db.add_pkg(pkg)

        # Finish metadata
        def finish_metadata(md):
            if md is None:
                return

            # Close XML file
            md.new_f.close()

            # Prepare repomd record of xml file
            rec = cr.RepomdRecord(md.metadata_type, md.new_fn)
            rec.load_contentstat(md.new_f_stat)
            rec.fill(md.checksum_type)
            if self.globalbundle.unique_md_filenames:
                rec.rename_file()

            md.new_rec = rec
            md.new_fn_exists = True

            gen_repomd_recs.append(rec)

            # Prepare database
            if hasattr(md, "db") and md.db:
                self._debug("Generating database: {0}".format(md.db_fn))
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

                gen_repomd_recs.append(db_rec)

        # Add records to the bundle

        finish_metadata(pri_md)
        finish_metadata(fil_md)
        finish_metadata(oth_md)

        return gen_repomd_recs

    def gen(self, metadata):
        # Check input arguments
        if "primary" not in metadata:
            self._error("primary.xml metadata file is missing")
            raise DeltaRepoPluginError("Primary metadata missing")

        gen_repomd_recs = []

        # Medadata info that will be persistently stored
        metadata_notes = {}

        pri_md = metadata.get("primary")
        fil_md = metadata.get("filelists")
        oth_md = metadata.get("other")

        def try_simple_delta(md, force_gen=False):
            """Try to do simple delta. If successful, return True"""
            rc, rec, notes = self._gen_basic_delta(md, force_gen=force_gen)
            if not rc:
                return False
            if rec:
                gen_repomd_recs.append(rec)
            if not notes:
                notes = {}
            if metadata.get(md.metadata_type+"_db").new_fn_exists:
                notes["database"] = "1"
            else:
                notes["database"] = "0"
            self._metadata_notes_to_plugin_bundle(md.metadata_type, notes)
            return True

        # At first try to do simple delta for primary
        # If successful, force simple delta for filelists and other too

        simple_pri_delta = try_simple_delta(pri_md)
        simple_fil_delta = try_simple_delta(fil_md, force_gen=simple_pri_delta)
        simple_oth_delta = try_simple_delta(oth_md, force_gen=simple_pri_delta)

        if simple_pri_delta:
            # Simple delta for primary means that simple deltas were done
            # for all other metadata too
            return gen_repomd_recs

        # At this point we know that simple delta for the primary wasn't done
        # This mean that at lest for primary, both metadata files (the new one
        # and the old one) exists, and we have to do a more sophisticated delta

        # Ignore files for which, the simple delta was successful
        if simple_fil_delta:
            fil_md = None
        if simple_oth_delta:
            oth_md = None

        # Prepare output xml files and check if dbs should be generated
        # Note: This information are stored directly to the Metadata
        # object which someone could see as little hacky.
        def prepare_paths_in_metadata(md, xmlclass):
            if md is None:
                return None

            # Make a note about if the database should be generated
            db_available = metadata.get(md.metadata_type+"_db").new_fn_exists
            if db_available or self.globalbundle.force_database:
                metadata_notes.setdefault(md.metadata_type, {})["database"] = "1"
            else:
                metadata_notes.setdefault(md.metadata_type, {})["database"] = "0"

            suffix = cr.compression_suffix(md.compression_type) or ""
            md.delta_fn = os.path.join(md.out_dir,
                                     "{0}.xml{1}".format(
                                     md.metadata_type, suffix))
            md.delta_f_stat = cr.ContentStat(md.checksum_type)
            md.delta_f = xmlclass(md.delta_fn,
                                  md.compression_type,
                                  md.delta_f_stat)
            return md

        # Primary
        pri_md = prepare_paths_in_metadata(pri_md, cr.PrimaryXmlFile)

        # Filelists
        fil_md = prepare_paths_in_metadata(fil_md, cr.FilelistsXmlFile)

        # Other
        oth_md = prepare_paths_in_metadata(oth_md, cr.OtherXmlFile)

        # Gen delta

        old_packages = set()
        added_packages = {}         # dict { 'pkgId': pkg }
        added_packages_ids = []     # list of package ids

        old_contenthash_strings = []
        new_contenthash_strings = []

        def old_pkgcb(pkg):
            old_packages.add(self._pkg_id_tuple(pkg))
            old_contenthash_strings.append(self._pkg_id_str(pkg))

        def new_pkgcb(pkg):
            new_contenthash_strings.append(self._pkg_id_str(pkg))
            pkg_id_tuple = self._pkg_id_tuple(pkg)
            if not pkg_id_tuple in old_packages:
                # This package is only in new repodata
                added_packages[pkg.pkgId] = pkg
                added_packages_ids.append(pkg.pkgId)
            else:
                # This package is also in the old repodata
                old_packages.remove(pkg_id_tuple)

        filelists_from_primary = True
        if fil_md:
            # Filelists will be parsed from filelists
            filelists_from_primary = False

        cr.xml_parse_primary(pri_md.old_fn, pkgcb=old_pkgcb, do_files=False)
        cr.xml_parse_primary(pri_md.new_fn, pkgcb=new_pkgcb,
                             do_files=filelists_from_primary)

        # Calculate content hashes
        h = hashlib.new(self.globalbundle.contenthash_type_str)
        old_contenthash_strings.sort()
        for i in old_contenthash_strings:
            h.update(i)
        src_contenthash = h.hexdigest()
        self.globalbundle.calculated_old_contenthash = src_contenthash

        h = hashlib.new(self.globalbundle.contenthash_type_str)
        new_contenthash_strings.sort()
        for i in new_contenthash_strings:
            h.update(i)
        dst_contenthash = h.hexdigest()
        self.globalbundle.calculated_new_contenthash = dst_contenthash

        # Set the content hashes to the plugin bundle
        self.pluginbundle.set("contenthash_type", self.globalbundle.contenthash_type_str)
        self.pluginbundle.set("src_contenthash", src_contenthash)
        self.pluginbundle.set("dst_contenthash", dst_contenthash)

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

        # Parse filelist.xml and write out its delta
        if fil_md:
            cr.xml_parse_filelists(fil_md.new_fn, newpkgcb=newpkgcb)
            fil_md.delta_f.set_num_of_pkgs(num_of_packages)
            for pkgid in added_packages_ids:
                fil_md.delta_f.add_pkg(added_packages[pkgid])
            fil_md.delta_f.close()

        # Parse other.xml and write out its delta
        if oth_md:
            cr.xml_parse_other(oth_md.new_fn, newpkgcb=newpkgcb)
            oth_md.delta_f.set_num_of_pkgs(num_of_packages)
            for pkgid in added_packages_ids:
                oth_md.delta_f.add_pkg(added_packages[pkgid])
            oth_md.delta_f.close()

        # Write out primary delta
        # Note: Writing of primary delta has to be after parsing of filelists
        # Otherwise cause missing files if filelists_from_primary was False
        pri_md.delta_f.set_num_of_pkgs(num_of_packages)
        for pkgid in added_packages_ids:
            pri_md.delta_f.add_pkg(added_packages[pkgid])
        pri_md.delta_f.close()

        # Finish metadata
        def finish_metadata(md):
            if md is None:
                return

            # Close XML file
            md.delta_f.close()

            # Prepare repomd record of xml file
            rec = cr.RepomdRecord(md.metadata_type, md.delta_fn)
            rec.load_contentstat(md.delta_f_stat)
            rec.fill(md.checksum_type)
            if self.globalbundle.unique_md_filenames:
                rec.rename_file()

            md.delta_rec = rec
            md.delta_fn_exists = True

            gen_repomd_recs.append(rec)

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

                gen_repomd_recs.append(db_rec)

        # Add records to medata objects
        finish_metadata(pri_md)
        finish_metadata(fil_md)
        finish_metadata(oth_md)

        # Store data persistently
        for metadata_type, notes in metadata_notes.items():
            self._metadata_notes_to_plugin_bundle(metadata_type, notes)

        return gen_repomd_recs

PLUGINS.append(MainDeltaRepoPlugin)

class GroupsDeltaRepoPlugin(DeltaRepoPlugin):

    NAME = "GroupDeltaRepoPlugin"
    VERSION = 1
    METADATA = ["group", "group_gz"]
    METADATA_MAPPING = {
        "group": ["group"],
        "group_gz": ["group", "group_gz"]
    }

    def gen(self, metadata):

        gen_repomd_recs = []

        md_group = metadata.get("group")
        md_group_gz = metadata.get("group_gz")

        if md_group and not md_group.new_fn_exists:
            md_group = None

        if md_group_gz and not md_group_gz.new_fn_exists:
            md_group_gz = None

        if md_group:
            rc, rec, notes = self._gen_basic_delta(md_group, force_gen=True)
            assert rc
            if rec:
                gen_repomd_recs.append(rec)
            if notes:
                if md_group_gz:
                    notes["gen_group_gz"] = "1"
                else:
                    notes["gen_group_gz"] = "0"
                self._metadata_notes_to_plugin_bundle(md_group.metadata_type,
                                                      notes)
        elif md_group_gz:
            rc, rec, notes = self._gen_basic_delta(md_group_gz, force_gen=True)
            assert rc
            if rec:
                gen_repomd_recs.append(rec)
            if notes:
                self._metadata_notes_to_plugin_bundle(md_group_gz.metadata_type,
                                                      notes)

        return gen_repomd_recs

    def apply(self, metadata):

        gen_repomd_recs = []

        md_group = metadata.get("group")
        md_group_gz = metadata.get("group_gz")

        if md_group and (not md_group.delta_fn_exists
                         and not md_group.old_fn_exists):
            md_group = None

        if md_group_gz and (not md_group_gz.delta_fn_exists
                            and not md_group_gz.old_fn_exists):
            md_group_gz = None

        if md_group:
            notes = self._metadata_notes_from_plugin_bundle(md_group.metadata_type)
            rc, rec = self._apply_basic_delta(md_group, notes)
            assert rc
            if rec:
                gen_repomd_recs.append(rec)
                if notes.get("gen_group_gz"):
                    # Gen group_gz metadata from the group metadata
                    stat = cr.ContentStat(md_group.checksum_type)
                    group_gz_fn = md_group.new_fn+".gz"
                    cr.compress_file(md_group.new_fn, group_gz_fn, cr.GZ, stat)
                    rec = cr.RepomdRecord("group_gz", group_gz_fn)
                    rec.load_contentstat(stat)
                    rec.fill(md_group.checksum_type)
                    if self.globalbundle.unique_md_filenames:
                        rec.rename_file()
                    gen_repomd_recs.append(rec)
        elif md_group_gz:
            notes = self._metadata_notes_from_plugin_bundle(md_group_gz.metadata_type)
            rc, rec = self._apply_basic_delta(md_group_gz, notes)
            assert rc
            if rec:
                gen_repomd_recs.append(rec)

        return gen_repomd_recs

PLUGINS.append(GroupsDeltaRepoPlugin)

for plugin in PLUGINS:
    METADATA_MAPPING.update(plugin.METADATA_MAPPING)

def needed_delta_metadata(required_metadata):
    """
    @param required_metadata    List of required metadatas.
    @return                     None if required_metadata is None
                                List of needed delta metadata files
                                in case that required_metadata is list
    """

    if required_metadata is None:
        return None

    needed_metadata = set(["deltametadata"])
    needed_metadata.add("primary")  # Currently, we always need primary.xml

    for required in required_metadata:
        if required in METADATA_MAPPING:
            needed_metadata |= set(METADATA_MAPPING[required])
        else:
            needed_metadata.add(required)

    return list(needed_metadata)