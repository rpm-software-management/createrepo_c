"""
"""

import _createrepo_c

VERSION_MAJOR = _createrepo_c.VERSION_MAJOR
VERSION_MINOR = _createrepo_c.VERSION_MINOR
VERSION_PATCH = _createrepo_c.VERSION_PATCH
VERSION = u"%d.%d.%d" % (VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH)

MD5     = _createrepo_c.MD5
SHA1    = _createrepo_c.SHA1
SHA256  = _createrepo_c.SHA256

AUTO_DETECT_COMPRESSION = _createrepo_c.AUTO_DETECT_COMPRESSION
UNKNOWN_COMPRESSION     = _createrepo_c.UNKNOWN_COMPRESSION
NO_COMPRESSION          = _createrepo_c.NO_COMPRESSION
GZ_COMPRESSION          = _createrepo_c.GZ_COMPRESSION
BZ2_COMPRESSION         = _createrepo_c.BZ2_COMPRESSION
XZ_COMPRESSION          = _createrepo_c.XZ_COMPRESSION

HT_KEY_DEFAULT  = _createrepo_c.HT_KEY_DEFAULT
HT_KEY_HASH     = _createrepo_c.HT_KEY_HASH
HT_KEY_NAME     = _createrepo_c.HT_KEY_NAME
HT_KEY_FILENAME = _createrepo_c.HT_KEY_FILENAME

DB_PRIMARY      = _createrepo_c.DB_PRIMARY
DB_FILELISTS    = _createrepo_c.DB_FILELISTS
DB_OTHER        = _createrepo_c.DB_OTHER

XMLFILE_PRIMARY   = _createrepo_c.XMLFILE_PRIMARY
XMLFILE_FILELISTS = _createrepo_c.XMLFILE_FILELISTS
XMLFILE_OTHER     = _createrepo_c.XMLFILE_OTHER

XML_WARNING_UNKNOWNTAG  = _createrepo_c.XML_WARNING_UNKNOWNTAG
XML_WARNING_MISSINGATTR = _createrepo_c.XML_WARNING_MISSINGATTR
XML_WARNING_UNKNOWNVAL  = _createrepo_c.XML_WARNING_UNKNOWNVAL
XML_WARNING_BADATTRVAL  = _createrepo_c.XML_WARNING_BADATTRVAL

CreaterepoCError = _createrepo_c.CreaterepoCError

# Metadata class

Metadata = _createrepo_c.Metadata

# MetadataLocation class

MetadataLocation = _createrepo_c.MetadataLocation

# Package class

class Package(_createrepo_c.Package):
    def __copy__(self):
        return self.copy()
    def __deepcopy__(self, _):
        return self.copy()

# Repomd class

Repomd = _createrepo_c.Repomd

# RepomdRecord class

class RepomdRecord(_createrepo_c.RepomdRecord):
    def compress_and_fill(self, hashtype, compresstype):
        rec = RepomdRecord("")
        _createrepo_c.RepomdRecord.compress_and_fill(self, rec, hashtype, compresstype)
        return rec


# Sqlite class

Sqlite = _createrepo_c.Sqlite

class PrimarySqlite(Sqlite):
    def __init__(self, filename):
        Sqlite.__init__(self, filename, DB_PRIMARY)

class FilelistsSqlite(Sqlite):
    def __init__(self, filename):
        Sqlite.__init__(self, filename, DB_FILELISTS)

class OtherSqlite(Sqlite):
    def __init__(self, filename):
        Sqlite.__init__(self, filename, DB_OTHER)

# XmlFile class

XmlFile = _createrepo_c.XmlFile

class PrimaryXmlFile(XmlFile):
    def __init__(self, filename, compressiontype=GZ_COMPRESSION):
        XmlFile.__init__(self, filename, XMLFILE_PRIMARY, compressiontype)

class FilelistsXmlFile(XmlFile):
    def __init__(self, filename, compressiontype=GZ_COMPRESSION):
        XmlFile.__init__(self, filename, XMLFILE_FILELISTS, compressiontype)

class OtherXmlFile(XmlFile):
    def __init__(self, filename, compressiontype=GZ_COMPRESSION):
        XmlFile.__init__(self, filename, XMLFILE_OTHER, compressiontype)

# XmlParser class

XmlParser = _createrepo_c.XmlParser

# Methods

xml_dump_primary = _createrepo_c.xml_dump_primary
xml_dump_filelists = _createrepo_c.xml_dump_filelists
xml_dump_other = _createrepo_c.xml_dump_other
xml_dump = _createrepo_c.xml_dump

def package_from_rpm(filename, checksum_type=SHA256, location_href=None,
                     location_base=None, changelog_limit=10):
    return _createrepo_c.package_from_rpm(filename, checksum_type,
                      location_href, location_base, changelog_limit)

def xml_from_rpm(filename, checksum_type=SHA256, location_href=None,
                     location_base=None, changelog_limit=10):
    return _createrepo_c.xml_from_rpm(filename, checksum_type,
                      location_href, location_base, changelog_limit)
