import os
import os.path
import shutil
import hashlib
import createrepo_c as cr
from deltarepo.errors import DeltaRepoError, DeltaRepoPluginError

PLUGINS = []
GENERAL_PLUGIN = None

class DeltaRepoPlugin(object):

    # Plugin name
    NAME = ""

    # Plugin version (integer number!)
    VERSION = 1

    # List of Metadata this plugin takes care of.
    # The plugin HAS TO do deltas for each of listed metadata and be able
    # to apply deltas on them!
    MATADATA = []

    # Its highly recomended for plugin to be maximal independend on
    # other plugins and metadata not specified in METADATA.
    # But some kind of dependency mechanism can be implemented via
    # *_REQUIRED_BUNDLE_KEYS and *_BUDLE_CONTRIBUTION.

    # List of bundle keys that have to be filled before
    # apply() method of this plugin should be called
    APPLY_REQUIRED_BUNDLE_KEYS = []

    # List of bundle key this pulugin adds during apply() method call
    APPLY_BUNDLE_CONTRIBUTION = []

    # List of bundle keys that have to be filled before
    # gen() method of this plugin should be called
    GEN_REQUIRED_BUNDLE_KEYS = []

    # List of bundle key this pulugin adds during gen() method call
    GEN_BUNDLE_CONTRIBUTION = []

    # If two plugins want to add the same key to the bundle
    # then exception is raised.
    # If plugin requires a key that isn't provided by any of registered
    # plugins then exception is raised.
    # If plugin adds to bundle a key that is not listed in BUNDLE_CONTRIBUTION,
    # then exception is raised.

    def __init__(self):
        pass

    def apply(self, metadata, bundle):
        """
        :arg metadata: Dict with available metadata listed in METADATA.
            key is metadata type (e.g. "primary", "filelists", ...)
            value is Metadata object
            This method is called only if at least one metadata listed
            in METADATA are found in delta repomd.
        :arg bundle: Dict with various metadata.

        Apply method has to do:
         * Raise DeltaRepoPluginError if something is bad
         * Build a new filename for each metadata and store it
           to Metadata Object.
         * 
        """
        raise NotImplementedError("Not implemented")

    def gen(self, metadata, bundle):
        raise NotImplementedError("Not implemented")


class GeneralDeltaRepoPlugin(DeltaRepoPlugin):

    NAME = "GeneralDeltaPlugin"
    VERSION = 1
    METADATA = []
    APPLY_REQUIRED_BUNDLE_KEYS = ["removed_obj",
                                  "unique_md_filenames"]
    APPLY_BUNDLE_CONTRIBUTION = []
    GEN_REQUIRED_BUNDLE_KEYS = ["removed_obj",
                                "unique_md_filenames"]
    GEN_BUNDLE_CONTRIBUTION = []

    def _path(self, path, record):
        """Return path to the repodata file."""
        return os.path.join(path, record.location_href)

    def apply(self, metadata, bundle):

        # Get info from bundle
        removed_obj = bundle["removed_obj"]
        unique_md_filenames = bundle["unique_md_filenames"]

        #
        for md in metadata.values():
            md.new_fn = os.path.join(md.out_dir, os.path.basename(md.delta_fn))
            shutil.copy2(md.delta_fn, md.new_fn)

            # Prepare repomd record of xml file
            rec = cr.RepomdRecord(md.metadata_type, md.new_fn)
            rec.fill(md.checksum_type)
            if unique_md_filenames:
                rec.rename_file()

            md.generated_repomd_records.append(rec)

    def gen(self, metadata, bundle):

        ## TODO: Compress uncompressed data

        # Get info from bundle
        removed_obj = bundle["removed_obj"]
        unique_md_filenames = bundle["unique_md_filenames"]

        for md in metadata.values():
            md.delta_fn = os.path.join(md.out_dir, os.path.basename(md.new_fn))
            shutil.copy2(md.new_fn, md.delta_fn)

            # Prepare repomd record of xml file
            rec = cr.RepomdRecord(md.metadata_type, md.delta_fn)
            rec.fill(md.checksum_type)
            if unique_md_filenames:
                rec.rename_file()

            md.generated_repomd_records.append(rec)

GENERAL_PLUGIN = GeneralDeltaRepoPlugin


class MainDeltaRepoPlugin(DeltaRepoPlugin):

    NAME = "MainDeltaPlugin"
    VERSION = 1
    METADATA = ["primary", "filelists", "other", "primary_db", "filelists_db", "other_db"]
    APPLY_REQUIRED_BUNDLE_KEYS = ["repoid_type_str",
                                  "removed_obj",
                                  "unique_md_filenames"]
    APPLY_BUNDLE_CONTRIBUTION = ["old_repoid", "new_repoid", "no_processed"]
    GEN_REQUIRED_BUNDLE_KEYS = ["repoid_type_str",
                                "removed_obj",
                                "unique_md_filenames"]
    GEN_BUNDLE_CONTRIBUTION = ["old_repoid", "new_repoid", "no_processed"]

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

    def apply(self, metadata, bundle):

        # Get info from bundle
        removed_obj = bundle["removed_obj"]
        repoid_type_str = bundle["repoid_type_str"]
        unique_md_filenames = bundle["unique_md_filenames"]

        # Check input arguments
        if "primary" not in metadata:
            raise DeltaRepoPluginError("Primary metadata missing")

        # Names of metadata that was not processed by this plugin
        no_processed = []

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

            if not md.old_fn:
                # Old file of this piece of metadata doesn't exist
                # This file has to be newly added.
                no_processed.append(md.metadata_type)
                no_processed_metadata.append(md)
                md.skip = True
                return
            md.skip = False

            suffix = cr.compression_suffix(md.compression_type) or ""
            md.new_fn = os.path.join(md.out_dir,
                                     "{0}.xml{1}".format(
                                     md.metadata_type, suffix))
            md.new_f_stat = cr.ContentStat(md.checksum_type)
            md.new_f = xmlclass(md.new_fn,
                                md.compression_type,
                                md.new_f_stat)

            if removed_obj and removed_obj.get_database(md.metadata_type, False):
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

        removed_packages = set() # set of pkgIds (hashes)
        all_packages = {}        # dict { 'pkgId': pkg }

        old_repoid_strings = []
        new_repoid_strings = []

        def old_pkgcb(pkg):
            old_repoid_strings.append(self._pkg_id_str(pkg))
            if pkg.location_href in removed_obj.packages:
                if removed_obj.packages[pkg.location_href] == pkg.location_base:
                    # This package won't be in new metadata
                    return
            new_repoid_strings.append(self._pkg_id_str(pkg))
            all_packages[pkg.pkgId] = pkg

        def delta_pkgcb(pkg):
            new_repoid_strings.append(self._pkg_id_str(pkg))
            all_packages[pkg.pkgId] = pkg

        do_primary_files = 1
        if fil_md and not fil_md.skip and fil_md.delta_fn and fil_md.old_fn:
            # Don't read files from primary if there is filelists.xml
            do_primary_files = 0
        if fil_md and fil_md.skip:
            # We got whole original filelists.xml, we do not need parse
            # files from primary.xml files.
            do_primary_files = 0

        cr.xml_parse_primary(pri_md.old_fn, pkgcb=old_pkgcb,
                             do_files=do_primary_files)
        cr.xml_parse_primary(pri_md.delta_fn, pkgcb=delta_pkgcb,
                             do_files=do_primary_files)

        # Calculate RepoIds
        old_repoid = ""
        new_repoid = ""

        h = hashlib.new(repoid_type_str)
        old_repoid_strings.sort()
        for i in old_repoid_strings:
            h.update(i)
        old_repoid = h.hexdigest()

        h = hashlib.new(repoid_type_str)
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
        if fil_md and not fil_md.skip and fil_md.delta_fn and fil_md.old_fn:
            cr.xml_parse_filelists(fil_md.old_fn, newpkgcb=newpkgcb)
            cr.xml_parse_filelists(fil_md.delta_fn, newpkgcb=newpkgcb)
        elif fil_md and fil_md.skip:
            # We got original filelists.xml we need to parse it here, to fill
            # package.
            cr.xml_parse_filelists(fil_md.delta_fn, newpkgcb=newpkgcb)

        # Parse other
        if oth_md and not oth_md.skip and oth_md.delta_fn and oth_md.old_fn:
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
        if fil_md and not fil_md.skip and fil_md.new_f:
            fil_md.new_f.set_num_of_pkgs(num_of_packages)
            for pkg in all_packages_sorted:
                fil_md.new_f.add_pkg(pkg)
                if fil_md.db:
                    fil_md.db.add_pkg(pkg)

        # Write out other
        if oth_md and not oth_md.skip and oth_md.new_f:
            oth_md.new_f.set_num_of_pkgs(num_of_packages)
            for pkg in all_packages_sorted:
                oth_md.new_f.add_pkg(pkg)
                if oth_md.db:
                    oth_md.db.add_pkg(pkg)

        # Finish metadata
        def finish_metadata(md):
            if md is None or md.skip:
                return

            # Close XML file
            md.new_f.close()

            # Prepare repomd record of xml file
            rec = cr.RepomdRecord(md.metadata_type, md.new_fn)
            rec.load_contentstat(md.new_f_stat)
            rec.fill(md.checksum_type)
            if unique_md_filenames:
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
                if unique_md_filenames:
                    db_rec.rename_file()

                md.generated_repomd_records.append(db_rec)

        # Add records to the bundle

        finish_metadata(pri_md)
        finish_metadata(fil_md)
        finish_metadata(oth_md)

        # Process XML files that was not processed if sqlite should be generated
        def finish_skipped_metadata(md):
            if md is None:
                return

            # Check if sqlite should be generated
            if removed_obj and removed_obj.get_database(md.metadata_type, False):
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
                if unique_md_filenames:
                    db_rec.rename_file()

                md.generated_repomd_records.append(db_rec)

        for md in no_processed_metadata:
            finish_skipped_metadata(md)

        # Add other bundle stuff
        bundle["old_repoid"] = old_repoid
        bundle["new_repoid"] = new_repoid
        bundle["no_processed"] = no_processed

    def gen(self, metadata, bundle):

        # Get info from bundle
        removed_obj = bundle["removed_obj"]
        repoid_type_str = bundle["repoid_type_str"]
        unique_md_filenames = bundle["unique_md_filenames"]

        # Check input arguments
        if "primary" not in metadata:
            raise DeltaRepoPluginError("Primary metadata missing")

        # Metadata that was not processed by this plugin
        no_processed = []

        pri_md = metadata.get("primary")
        fil_md = metadata.get("filelists")
        oth_md = metadata.get("other")

        # Build and prepare destination paths
        # (And store them in the same Metadata object)
        def prepare_paths_in_metadata(md, xmlclass, dbclass):
            if md is None:
                return

            if removed_obj:
                # Make a note to the removed obj if the database should be generated
                db_type = "{0}_db".format(md.metadata_type)
                available = metadata.get(db_type)
                removed_obj.set_database(md.metadata_type, available)

            if not md.old_fn:
                # Old file of this piece of metadata doesn't exist
                # This file has to be newly added.
                no_processed.append(md.metadata_type)
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

        h = hashlib.new(repoid_type_str)
        old_repoid_strings.sort()
        for i in old_repoid_strings:
            h.update(i)
        old_repoid = h.hexdigest()

        h = hashlib.new(repoid_type_str)
        new_repoid_strings.sort()
        for i in new_repoid_strings:
            h.update(i)
        new_repoid = h.hexdigest()

        # Prepare list of removed packages
        removed_pkgs = sorted(old_packages)
        for _, location_href, location_base in removed_pkgs:
            removed_obj.add_pkg_locations(location_href, location_base)

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
            if unique_md_filenames:
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
                if unique_md_filenames:
                    db_rec.rename_file()

                md.generated_repomd_records.append(db_rec)

        # Add records to the bundle

        finish_metadata(pri_md)
        finish_metadata(fil_md)
        finish_metadata(oth_md)

        bundle["old_repoid"] = old_repoid
        bundle["new_repoid"] = new_repoid
        bundle["no_processed"] = no_processed


PLUGINS.append(MainDeltaRepoPlugin)
