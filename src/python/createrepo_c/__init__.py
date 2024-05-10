"""
"""

import collections
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from . import _createrepo_c
from ._createrepo_c import *

VERSION_MAJOR = _createrepo_c.VERSION_MAJOR  #: Major version
VERSION_MINOR = _createrepo_c.VERSION_MINOR  #: Minor version
VERSION_PATCH = _createrepo_c.VERSION_PATCH  #: Patch version

#: Version string
VERSION = u"%d.%d.%d" % (VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH)

UNKNOWN_CHECKSUM = _createrepo_c.CHECKSUM_UNKNOWN #: Checksum unknown
CHECKSUM_UNKNOWN = _createrepo_c.CHECKSUM_UNKNOWN #: Checksum unknown

for hash_name in ('MD5', 'SHA', 'SHA1', 'SHA224', 'SHA256', 'SHA384', 'SHA512'):
    hash_attr = getattr(_createrepo_c, hash_name, None)
    if hash_attr:
        globals()[hash_name] = hash_attr

MODE_READ   = _createrepo_c.MODE_READ  #: Read open mode
MODE_WRITE  = _createrepo_c.MODE_WRITE #: Write open mode

#: Use compression autodetection
AUTO_DETECT_COMPRESSION = _createrepo_c.AUTO_DETECT_COMPRESSION

#: Unknown compression
UNKNOWN_COMPRESSION     = _createrepo_c.UNKNOWN_COMPRESSION

#: No compression
NO_COMPRESSION          = _createrepo_c.NO_COMPRESSION

#: Gzip compression
GZ_COMPRESSION          = _createrepo_c.GZ_COMPRESSION

#: Bzip2 compression
BZ2_COMPRESSION         = _createrepo_c.BZ2_COMPRESSION

#: XZ compression
XZ_COMPRESSION          = _createrepo_c.XZ_COMPRESSION

#: Zchunk compression
ZCK_COMPRESSION         = _createrepo_c.ZCK_COMPRESSION

#: Gzip compression alias
GZ                      = _createrepo_c.GZ_COMPRESSION

#: Bzip2 compression alias
BZ2                     = _createrepo_c.BZ2_COMPRESSION

#: XZ compression alias
XZ                      = _createrepo_c.XZ_COMPRESSION

#: Zchunk compression alias
ZCK                     = _createrepo_c.ZCK_COMPRESSION

#: Zstd compression alias
ZSTD                     = _createrepo_c.ZSTD_COMPRESSION

HT_KEY_DEFAULT  = _createrepo_c.HT_KEY_DEFAULT  #: Default key (hash)
HT_KEY_HASH     = _createrepo_c.HT_KEY_HASH     #: Package hash as a key
HT_KEY_NAME     = _createrepo_c.HT_KEY_NAME     #: Package name as a key
HT_KEY_FILENAME = _createrepo_c.HT_KEY_FILENAME #: Package filename as a key

HT_DUPACT_KEEPFIRST = _createrepo_c.HT_DUPACT_KEEPFIRST #: If an key is duplicated, keep only the first occurrence
HT_DUPACT_REMOVEALL = _createrepo_c.HT_DUPACT_REMOVEALL #: If an key is duplicated, discard all occurrences

DB_PRIMARY       = _createrepo_c.DB_PRIMARY       #: Primary database
DB_FILELISTS     = _createrepo_c.DB_FILELISTS     #: Filelists database
DB_FILELISTS_EXT = _createrepo_c.DB_FILELISTS_EXT #: Filelists_ext database
DB_OTHER         = _createrepo_c.DB_OTHER         #: Other database

XMLFILE_PRIMARY       = _createrepo_c.XMLFILE_PRIMARY       #: Primary xml file
XMLFILE_FILELISTS     = _createrepo_c.XMLFILE_FILELISTS     #: Filelists xml file
XMLFILE_FILELISTS_EXT = _createrepo_c.XMLFILE_FILELISTS_EXT #: Filelists_ext xml file
XMLFILE_OTHER         = _createrepo_c.XMLFILE_OTHER         #: Other xml file
XMLFILE_PRESTODELTA   = _createrepo_c.XMLFILE_PRESTODELTA   #: Prestodelta xml file
XMLFILE_UPDATEINFO    = _createrepo_c.XMLFILE_UPDATEINFO    #: Updateinfo xml file

#: XML warning - Unknown tag
XML_WARNING_UNKNOWNTAG  = _createrepo_c.XML_WARNING_UNKNOWNTAG

#: XML warning - Missing attribute
XML_WARNING_MISSINGATTR = _createrepo_c.XML_WARNING_MISSINGATTR

#: XML warning - Unknown value
XML_WARNING_UNKNOWNVAL  = _createrepo_c.XML_WARNING_UNKNOWNVAL

#: XML warning - Bad attribute value
XML_WARNING_BADATTRVAL  = _createrepo_c.XML_WARNING_BADATTRVAL

# Helper contants


# Tuple indexes for provide, conflict, obsolete or require entry
PCOR_ENTRY_NAME    = 0 #: PCOR entry tuple index - name
PCOR_ENTRY_FLAGS   = 1 #: PCOR entry tuple index - flags
PCOR_ENTRY_EPOCH   = 2 #: PCOR entry tuple index - epoch
PCOR_ENTRY_VERSION = 3 #: PCOR entry tuple index - version
PCOR_ENTRY_RELEASE = 4 #: PCOR entry tuple index - release
PCOR_ENTRY_PRE     = 5 #: PCOR entry tuple index - pre


# NOTE(amatej): Consider changing the tuple into a class if it should be extended with new data.
# Tuple indexes for file entry
FILE_ENTRY_TYPE = 0 #: File entry tuple index - file type
FILE_ENTRY_PATH = 1 #: File entry tuple index - path
FILE_ENTRY_NAME = 2 #: File entry tuple index - file name
FILE_ENTRY_DIGEST = 3 #: File entry tuple index - file digest, present only in filelists-ext

# Tuple indexes for changelog entry
CHANGELOG_ENTRY_AUTHOR    = 0 #: Changelog entry tuple index - Author
CHANGELOG_ENTRY_DATE      = 1 #: Changelog entry tuple index - Date
CHANGELOG_ENTRY_CHANGELOG = 2 #: Changelog entry tuple index - Changelog


# Exception

CreaterepoCError = _createrepo_c.CreaterepoCError

# ContentStat class

ContentStat = _createrepo_c.ContentStat

# CrFile class

class CrFile(_createrepo_c.CrFile):
    def __init__(self, filename, mode=MODE_READ,
                 comtype=NO_COMPRESSION, stat=None):
        """:arg filename: Filename
        :arg mode: MODE_READ or MODE_WRITE
        :arg comtype: Compression type (GZ, BZ, XZ or NO_COMPRESSION)
        :arg stat: ContentStat object or None"""
        _createrepo_c.CrFile.__init__(self, filename, mode, comtype, stat)

# Metadata class

Metadata = _createrepo_c.Metadata

# MetadataLocation class

MetadataLocation = _createrepo_c.MetadataLocation

# Package class

Package = _createrepo_c.Package

# Repomd class

class Repomd(_createrepo_c.Repomd):
    def __init__(self, path=None):
        """:arg path: Path to existing repomd.xml or None"""
        _createrepo_c.Repomd.__init__(self)
        self.warnings = []

        def _warningcb(warning_type, message):
            self.warnings.append((warning_type, message))
            return True  # continue parsing

        if path:
            xml_parse_repomd(path, self, warningcb=_warningcb)

    def __iter__(self):
        for rec in self.records:
            yield rec
        return

    def __getitem__(self, key):
        for rec in self.records:
            if rec.type == key:
                return rec
        self.__missing__(key)

    def __missing__(self, key):
        raise KeyError("Record with type '%s' doesn't exist" % key)

    def __contains__(self, key):
        for rec in self.records:
            if rec.type == key:
                return True
        return False

# RepomdRecord class

class RepomdRecord(_createrepo_c.RepomdRecord):
    def __init__(self, type=None, path=None):
        """:arg type: String with type of the file (e.g. other, other_db etc.)
        :arg path: Path to the file
        """
        _createrepo_c.RepomdRecord.__init__(self, type, path)

    def compress_and_fill(self, hashtype, compresstype):
        rec = RepomdRecord(self.type, None)
        _createrepo_c.RepomdRecord.compress_and_fill(self,
                                                     rec,
                                                     hashtype,
                                                     compresstype)
        return rec


# Sqlite class

Sqlite = _createrepo_c.Sqlite

class PrimarySqlite(Sqlite):
    def __init__(self, path):
        """:arg path: path to the primary.sqlite database"""
        Sqlite.__init__(self, path, DB_PRIMARY)

class FilelistsSqlite(Sqlite):
    def __init__(self, path):
        """:arg path: Path to the filelists.sqlite database"""
        Sqlite.__init__(self, path, DB_FILELISTS)

class OtherSqlite(Sqlite):
    def __init__(self, path):
        """:arg path: Path to the other.sqlite database"""
        Sqlite.__init__(self, path, DB_OTHER)


# UpdateCollection class

UpdateCollection = _createrepo_c.UpdateCollection

# UpdateCollectionModule class

UpdateCollectionModule = _createrepo_c.UpdateCollectionModule

# UpdateCollectionPackage class

UpdateCollectionPackage = _createrepo_c.UpdateCollectionPackage


# UpdateInfo class

class UpdateInfo(_createrepo_c.UpdateInfo):
    def __init__(self, path=None):
        """:arg path: Path to existing updateinfo.xml or None"""
        _createrepo_c.UpdateInfo.__init__(self)
        self.warnings = []

        def _warningcb(warning_type, message):
            self.warnings.append((warning_type, message))
            return True  # continue parsing

        if path:
            xml_parse_updateinfo(path, self, warningcb=_warningcb)

# UpdateRecord class

UpdateRecord = _createrepo_c.UpdateRecord


# UpdateReference class

UpdateReference = _createrepo_c.UpdateReference


# XmlFile class

XmlFile = _createrepo_c.XmlFile

class PrimaryXmlFile(XmlFile):
    def __init__(self, path, compressiontype=GZ_COMPRESSION,
                 contentstat=None):
        """:arg path: Path to the primary xml file
        :arg compressiontype: Compression type
        :arg contentstat: ContentStat object"""
        XmlFile.__init__(self, path, XMLFILE_PRIMARY,
                         compressiontype, contentstat)

class FilelistsXmlFile(XmlFile):
    def __init__(self, path, compressiontype=GZ_COMPRESSION,
                 contentstat=None):
        """:arg path: Path to the filelists[_ext] xml file
        :arg compressiontype: Compression type
        :arg contentstat: ContentStat object"""
        # TODO(aplanas) Do I need to differentiate?
        XmlFile.__init__(self, path, XMLFILE_FILELISTS,
                         compressiontype, contentstat)

class OtherXmlFile(XmlFile):
    def __init__(self, path, compressiontype=GZ_COMPRESSION,
                 contentstat=None):
        """:arg path: Path to the other xml file
        :arg compressiontype: Compression type
        :arg contentstat: ContentStat object"""
        XmlFile.__init__(self, path, XMLFILE_OTHER,
                         compressiontype, contentstat)

class UpdateInfoXmlFile(XmlFile):
    def __init__(self, path, compressiontype=GZ_COMPRESSION,
                 contentstat=None):
        """:arg path: Path to the updateinfo xml file
        :arg compressiontype: Compression type
        :arg contentstat: ContentStat object"""
        XmlFile.__init__(self, path, XMLFILE_UPDATEINFO,
                         compressiontype, contentstat)

# Functions

def package_from_rpm(filename, checksum_type=SHA256, location_href=None,
                     location_base=None, changelog_limit=10):
    """:class:`.Package` object from the rpm package"""
    return _createrepo_c.package_from_rpm(filename, checksum_type,
                      location_href, location_base, changelog_limit)

def xml_from_rpm(filename, checksum_type=SHA256, location_href=None,
                     location_base=None, changelog_limit=10):
    """XML for the rpm package"""
    return _createrepo_c.xml_from_rpm(filename, checksum_type,
                      location_href, location_base, changelog_limit)

xml_dump_primary        = _createrepo_c.xml_dump_primary
xml_dump_filelists      = _createrepo_c.xml_dump_filelists
xml_dump_filelists_ext  = _createrepo_c.xml_dump_filelists_ext
xml_dump_other          = _createrepo_c.xml_dump_other
xml_dump_updaterecord   = _createrepo_c.xml_dump_updaterecord
xml_dump                = _createrepo_c.xml_dump

def xml_parse_primary(path, newpkgcb=None, pkgcb=None,
                      warningcb=None, do_files=1):
    """Parse primary.xml"""
    return _createrepo_c.xml_parse_primary(path, newpkgcb, pkgcb,
                                           warningcb, do_files)

def xml_parse_filelists(path, newpkgcb=None, pkgcb=None, warningcb=None):
    """Parse filelists[_ext].xml"""
    return _createrepo_c.xml_parse_filelists(path, newpkgcb, pkgcb, warningcb)

def xml_parse_other(path, newpkgcb=None, pkgcb=None, warningcb=None):
    """Parse other.xml"""
    return _createrepo_c.xml_parse_other(path, newpkgcb, pkgcb, warningcb)

def xml_parse_primary_snippet(xml_string, newpkgcb=None, pkgcb=None,
                              warningcb=None, do_files=1):
    """Parse the contents of primary.xml from a string"""
    return _createrepo_c.xml_parse_primary_snippet(xml_string, newpkgcb, pkgcb,
                                           warningcb, do_files)

def xml_parse_filelists_snippet(xml_string, newpkgcb=None, pkgcb=None,
                                warningcb=None):
    """Parse the contents of filelists[_ext].xml from a string"""
    return _createrepo_c.xml_parse_filelists_snippet(xml_string, newpkgcb, pkgcb,
                                             warningcb)

def xml_parse_other_snippet(xml_string, newpkgcb=None, pkgcb=None,
                            warningcb=None):
    """Parse the contents of other.xml from a string"""
    return _createrepo_c.xml_parse_other_snippet(xml_string, newpkgcb, pkgcb,
                                         warningcb)

def xml_parse_updateinfo(path, updateinfoobj, warningcb=None):
    """Parse updateinfo.xml"""
    return _createrepo_c.xml_parse_updateinfo(path, updateinfoobj, warningcb)

def xml_parse_repomd(path, repomdobj, warningcb=None):
    """Parse repomd.xml"""
    return _createrepo_c.xml_parse_repomd(path, repomdobj, warningcb)

checksum_name_str   = _createrepo_c.checksum_name_str
checksum_type       = _createrepo_c.checksum_type

def compress_file(src, dst, comtype, stat=None):
    return _createrepo_c.compress_file_with_stat(src, dst, comtype, stat)

def decompress_file(src, dst, comtype, stat=None):
    return _createrepo_c.decompress_file_with_stat(src, dst, comtype, stat)

compression_suffix  = _createrepo_c.compression_suffix
detect_compression  = _createrepo_c.detect_compression
compression_type    = _createrepo_c.compression_type

class PackageIterator(_createrepo_c.PkgIterator):
    def __init__(self, primary_path, filelists_path, other_path, newpkgcb=None, warningcb=None):
        """Parse completed packages one at a time."""
        _createrepo_c.PkgIterator.__init__(
            self, primary_path, filelists_path, other_path, newpkgcb, warningcb)


class RepositoryReader:
    """Parser for RPM metadata."""

    def __init__(self):
        """Initialize empty (use one of the alternate constructors)."""
        self._primary_xml_path = None
        self._filelists_xml_path = None
        self._other_xml_path = None
        self._updateinfo_path = None
        self.repomd = None

    @staticmethod
    def from_path(path, warningcb=None):
        """Construct a parser from an on-disk repository."""

        if not warningcb:
            def _warningcb(warning_type, message):
                print("PARSER WARNING: %s" % message)
                return True
            warningcb = _warningcb

        repomd_path = Path(path) / "repodata" / "repomd.xml"
        if not repomd_path.exists():
            raise FileNotFoundError("No repository found at the provided path.")

        repomd = Repomd(str(repomd_path))
        metadata_files = {record.type: record for record in repomd.records}
        parser = RepositoryReader()
        parser._primary_xml_path = os.path.join(path, metadata_files["primary"].location_href)
        parser._filelists_xml_path = os.path.join(path, metadata_files["filelists"].location_href)
        parser._other_xml_path = os.path.join(path, metadata_files["other"].location_href)

        if metadata_files.get("updateinfo"):
            parser._updateinfo_path = os.path.join(path, metadata_files["updateinfo"].location_href)
        parser.repomd = repomd
        return parser

    @staticmethod
    def from_metadata_files(primary_xml_path, filelists_xml_path, other_xml_path, updateinfo_xml_path=None):
        """Construct a parser from the three main metadata files."""

        parser = RepositoryReader()
        parser._primary_xml_path = primary_xml_path
        parser._filelists_xml_path = filelists_xml_path
        parser._other_xml_path = other_xml_path
        parser._updateinfo_path = updateinfo_xml_path

        return parser

    def advisories(self):
        """Get advisories"""
        if not self._updateinfo_path:
            return []
        else:
            return UpdateInfo(self._updateinfo_path).updates

    def package_count(self):
        """Count the total number of packages."""
        # It would be much faster to just read the number in the header of the metadata.
        # But there's no way to do that. This gets fuzzy around the topic of duplicates.
        # If the same package is listed more than once, is that counted as more than one package?
        # Currently, no.
        return len(self.parse_packages(only_primary=True)[0])

    def iter_packages(self, warningcb=None):
        """
        Return an object which permits iterating over packages one at a time.

        This uses less memory than parse_packages() and works in the presence of duplicate packages.

        Kwargs:
            warningcb (callable): A callback function for warnings emitted by the parser.

        Returns:
            cr.PackageIterator object.
        """
        return PackageIterator(
            primary_path=self._primary_xml_path,
            filelists_path=self._filelists_xml_path,
            other_path=self._other_xml_path,
            warningcb=warningcb,
        )

    def parse_packages(self, only_primary=False):
        """
        Parse repodata to extract package info.

        Note: In the presence of duplicated packages in the repo (i.e. same pkgId), this will
        deduplicate them however the packages may have duplicate files or changelogs listed within.
        See: https://github.com/rpm-software-management/createrepo_c/issues/306

        Args:
            primary_xml_path (str): a path to a downloaded primary.xml
            filelists_xml_path (str): a path to a downloaded filelists.xml
            other_xml_path (str): a path to a downloaded other.xml

        Kwargs:
            only_primary (bool): If true, only the metadata in primary.xml will be parsed.

        Returns:
            A 2-item tuple containing:
                A dict containing createrepo_c package objects with the pkgId as a key
                A list of warnings encountered during parsing

        """

        warnings = []

        def warningcb(warning_type, message):
            """Optional callback for warnings about wierd stuff and formatting in XML.

            Args:
                warning_type (int): One of the XML_WARNING_* constants.
                message (str): Message.
            """
            warnings.append((warning_type, message))
            return True  # continue parsing

        def pkgcb(pkg):
            """
            A callback which is used when a whole package entry in xml is parsed.

            Args:
                pkg(preaterepo_c.Package): a parsed metadata for a package

            """
            packages[pkg.pkgId] = pkg

        def newpkgcb(pkgId, name, arch):
            """
            A callback which is used when a new package entry is encountered.

            Only opening <package> element is parsed at that moment.
            This function has to return a package which parsed data will be added to
            or None if a package should be skipped.

            pkgId, name and arch of a package can be used to skip further parsing. Available
            only for filelists.xml and other.xml.

            Args:
                pkgId(str): pkgId of a package
                name(str): name of a package
                arch(str): arch of a package

            Returns:
                createrepo_c.Package: a package which parsed data should be added to.

                If None is returned, further parsing of a package will be skipped.

            """
            return packages.get(pkgId, None)

        packages = collections.OrderedDict()

        xml_parse_primary(self._primary_xml_path, pkgcb=pkgcb, warningcb=warningcb, do_files=False)
        if not only_primary:
            xml_parse_filelists(self._filelists_xml_path, newpkgcb=newpkgcb, warningcb=warningcb)
            xml_parse_other(self._other_xml_path, newpkgcb=newpkgcb, warningcb=warningcb)
        return packages, warnings


# both the path and the *XmlFile objects need to be tracked together because there's no way to get the path
# back from the *XmlFile objects
MetadataInfoHolder = collections.namedtuple("MetadataInfoHolder", ["path", "writer"])

class RepositoryWriter:

    _FINISHED_ERR_MSG = "Cannot perform action after the repository has already finished being written"

    def __init__(self,
                 destination,
                 num_packages=None,
                 unique_md_filenames=True,
                 changelog_limit=10,
                 compression=ZSTD_COMPRESSION,
                 checksum_type=SHA256):

        if changelog_limit:
            assert isinstance(changelog_limit, int) and changelog_limit >= 0, "changelog_limit must be an integer >= 0"

        self.repomd = Repomd()
        self._destination_repo_path = Path(destination)

        self._unique_md_filenames = unique_md_filenames
        self._changelog_limit = changelog_limit
        self._checksum_type = checksum_type

        self._has_set_num_pkgs = False
        self._finished = False

        os.makedirs(self.path, exist_ok=True)
        os.makedirs(self.repodata_dir, exist_ok=True)

        def _compression_suffix(compressiontype):
            suffix = compression_suffix(compressiontype)
            return suffix if suffix else ""

        self._compression = compression
        self._compression_suffix = _compression_suffix(compression)

        pri_xml_path = self.repodata_dir / ("primary.xml" + self._compression_suffix)
        fil_xml_path = self.repodata_dir / ("filelists.xml" + self._compression_suffix)
        oth_xml_path = self.repodata_dir / ("other.xml" + self._compression_suffix)

        self.working_metadata_files = {
            "primary": MetadataInfoHolder(
                pri_xml_path, PrimaryXmlFile(str(pri_xml_path), compressiontype=compression)
            ),
            "filelists": MetadataInfoHolder(
                fil_xml_path, FilelistsXmlFile(str(fil_xml_path), compressiontype=compression)
            ),
            "other": MetadataInfoHolder(
                oth_xml_path, OtherXmlFile(str(oth_xml_path), compressiontype=compression)
            ),
        }
        self.additional_metadata_files = {}

        if num_packages is not None:
            self.set_num_of_pkgs(num_packages)

        self._updaterecords = []

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        if exc_type:
            pass
        else:
            self.finish()

    @property
    def path(self):
        return self._destination_repo_path

    @property
    def repodata_dir(self):
        return self.path / "repodata"

    def set_num_of_pkgs(self, num):
        """Set the number of packages that will be added - this has to be done before adding any packages."""
        assert not self._has_set_num_pkgs, "The number of packages has already been set"
        self._has_set_num_pkgs = True

        self.working_metadata_files["primary"].writer.set_num_of_pkgs(num)
        self.working_metadata_files["filelists"].writer.set_num_of_pkgs(num)
        self.working_metadata_files["other"].writer.set_num_of_pkgs(num)

    def add_pkg_from_file(self, path, output_dir=None):
        """Add a package to the repo from a provided path."""
        assert self._has_set_num_pkgs, "Must set the number of packages before adding packages"
        assert not self._finished, self._FINISHED_ERR_MSG

        filename = os.path.basename(path)
        try:
            relative_path = Path(path).relative_to(self.path)  # raises a ValueError if path is not relative
        except ValueError:
            if output_dir:
                relative_path = os.path.join(output_dir, filename)
                os.makedirs(self.path / output_dir, exist_ok=True)
            else:
                relative_path = filename
            new_path = shutil.copy2(path, self.path / relative_path)

        pkg = package_from_rpm(
            path,
            checksum_type=self._checksum_type,
            location_href=str(relative_path),
            location_base=None,
            changelog_limit=self._changelog_limit
        )

        self.add_pkg(pkg)
        return pkg

    def add_pkg(self, pkg):
        """Add a package to the repo from a pre-created Package object."""
        assert self._has_set_num_pkgs, "Must set the number of packages before adding packages"
        assert not self._finished, self._FINISHED_ERR_MSG

        self.working_metadata_files["primary"].writer.add_pkg(pkg)
        self.working_metadata_files["filelists"].writer.add_pkg(pkg)
        self.working_metadata_files["other"].writer.add_pkg(pkg)

    def add_repomd_metadata(self, name, path, use_compression=True):
        """Add an additional metadata file to the final repomd."""
        assert not self._finished, self._FINISHED_ERR_MSG

        if use_compression:
            dst = self.repodata_dir / (os.path.basename(path) + self._compression_suffix)
            compress_file(path, str(dst), self._compression)
            self.additional_metadata_files[name] = dst
        else:
            try:
                path = shutil.copy2(path, self.repodata_dir)
            except shutil.SameFileError:
                pass
            self.additional_metadata_files[name] = path

    def add_update_record(self, rec):
        """Add an advisory (update record) to the repository."""
        assert not self._finished, self._FINISHED_ERR_MSG

        self._updaterecords.append(rec)

    def finish(self):
        """Finish writing metadata."""
        assert not self._finished, self._FINISHED_ERR_MSG
        self._finished = True

        # if the user hasn't added any packages we can let them skip this step
        if not self._has_set_num_pkgs:
            self.set_num_of_pkgs(0)

        records = {}

        # fail if the user used add_repomd_metadata() for one of "primary", "filelists", "other",
        # "updateinfo" (if updaterecords added also), etc.
        created_record_names = set(self.working_metadata_files.keys())
        added_record_names = set(self.additional_metadata_files.keys())
        overlapping_records = created_record_names.intersection(added_record_names)
        assert not overlapping_records, "Added repomd metadata {} conflicts with created metadata".format(overlapping_records)

        # there's no good way to write this data out incrementally, unfortunately
        if self._updaterecords:
            upd_xml_path = self.repodata_dir / ("updateinfo.xml" + self._compression_suffix)
            writer = UpdateInfoXmlFile(str(upd_xml_path), compressiontype=self._compression)
            for rec in self._updaterecords:
                writer.add_chunk(_createrepo_c.xml_dump_updaterecord(rec))
            self.working_metadata_files["updateinfo"] = MetadataInfoHolder(upd_xml_path, writer)

        # Create all the repomdrecords for the standard metadata
        for record_name, metadata_info in self.working_metadata_files.items():
            # Close all of the metadata files being actively edited
            metadata_info.writer.close()
            record = RepomdRecord(record_name, str(metadata_info.path))
            record.fill(self._checksum_type)
            records[record_name] = record

        # Create all the repomdrecords for the externally-added metadata
        for record_name, path in self.additional_metadata_files.items():
            # if the user tried to add the same record twice, last one wins I guess?
            record = RepomdRecord(record_name, str(path))
            record.fill(self._checksum_type)
            records[record_name] = record

        # Rename the files (if requested) and then add all the repomdrecords to the repomd.xml
        for record in records.values():
            if self._unique_md_filenames:
                record.rename_file()
            self.repomd.set_record(record)

        # Write repomd.xml
        repomd_path = self.repodata_dir / "repomd.xml"
        with open(repomd_path, "w") as repomd_xml_file:
            repomd_xml_file.write(self.repomd.xml_dump())


# If we have been built as a Python package, e.g. "setup.py", this is where the binaries
# will be located.
_DATA_DIR = os.path.join(os.path.dirname(__file__), 'data')

# Where we will look for the binaries. Default to looking on the system PATH.
_BIN_DIR = ""

# If the following test succeeds, then look for binaries in the Python pkg "data" location.
# If not, we probably were not built as a Python package (e.g. RPM, "cmake ..; make").
# In that case, let's just assume that the binary will be on the PATH somewhere.
if os.path.exists(_DATA_DIR):
    _BIN_DIR = os.path.join(_DATA_DIR, 'bin')


def _program(name, args):
    return subprocess.call([os.path.join(_BIN_DIR, name)] + args)


def createrepo_c():
    raise SystemExit(_program('createrepo_c', sys.argv[1:]))


def mergerepo_c():
    raise SystemExit(_program('mergerepo_c', sys.argv[1:]))


def modifyrepo_c():
    raise SystemExit(_program('modifyrepo_c', sys.argv[1:]))


def sqliterepo_c():
    raise SystemExit(_program('sqliterepo_c', sys.argv[1:]))
