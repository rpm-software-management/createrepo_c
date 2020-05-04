#!/usr/bin/env python

import os
import os.path

import createrepo_c as cr

REPO_PATH = "repo/"


def print_package_info(pkg):
    def print_pcors(lst, requires=False):
        for item in lst:
            print("    Name: %s" % item[cr.PCOR_ENTRY_NAME])
            print("    Flags: %s" % item[cr.PCOR_ENTRY_FLAGS])
            print("    Epoch: %s" % item[cr.PCOR_ENTRY_EPOCH])
            print("    Version: %s" % item[cr.PCOR_ENTRY_VERSION])
            print("    Release: %s" % item[cr.PCOR_ENTRY_RELEASE])
            if requires:
                print("    Pre: %s" % item[cr.PCOR_ENTRY_PRE])
            print("    +-----------------------------------+")

    def print_files(lst):
        for item in lst:
            print("    Name: %s" % item[cr.FILE_ENTRY_NAME])
            print("    Path: %s" % item[cr.FILE_ENTRY_PATH])
            print("    Type: %s" % item[cr.FILE_ENTRY_TYPE])
            print("    +-----------------------------------+")

    def print_changelogs(lst):
        for item in lst:
            print("    Author: %s" % item[cr.CHANGELOG_ENTRY_AUTHOR])
            print("    Date: %s" % item[cr.CHANGELOG_ENTRY_DATE])
            print("    Changelog: %s" % item[cr.CHANGELOG_ENTRY_CHANGELOG])
            print("    +-----------------------------------+")

    print("+=======================================+")
    print("  %s" % pkg.name)
    print("+=======================================+")
    print("NEVRA: %s" % pkg.nevra())
    print("NVRA: %s" % pkg.nvra())
    print("Name: %s" % pkg.name)
    print("Checksum (pkgId): %s" % pkg.pkgId)
    print("Checksum type: %s" % pkg.checksum_type)
    print("Arch: %s" % pkg.arch)
    print("Version: %s" % pkg.version)
    print("Epoch: %s" % pkg.epoch)
    print("Release: %s" % pkg.release)
    print("Summary: %s" % pkg.summary)
    print("Description: %s" % pkg.description)
    print("URL: %s" % pkg.url)
    print("Time file: %s" % pkg.time_file)
    print("Time build: %s" % pkg.time_build)
    print("License: %s" % pkg.rpm_license)
    print("Vendor: %s" % pkg.rpm_vendor)
    print("Group: %s" % pkg.rpm_group)
    print("Buildhost: %s" % pkg.rpm_buildhost)
    print("Source RPM: %s" % pkg.rpm_sourcerpm)
    print("Header start: %s" % pkg.rpm_header_start)
    print("Header end: %s" % pkg.rpm_header_end)
    print("Packager: %s" % pkg.rpm_packager)
    print("Size package: %s" % pkg.size_package)
    print("Size installed: %s" % pkg.size_installed)
    print("Size archive: %s" % pkg.size_archive)
    print("Location href: %s" % pkg.location_href)
    print("Location base: %s" % pkg.location_base)
    print("Requires:")
    print_pcors(pkg.requires, requires=True)
    print("Provides:")
    print_pcors(pkg.provides)
    print("Conflicts:")
    print_pcors(pkg.conflicts)
    print("Obsoletes:")
    print_pcors(pkg.obsoletes)
    print("Files:")
    print_files(pkg.files)
    print("Changelogs:")
    print_changelogs(pkg.changelogs)

def first_method():
    """Use of this method is discouraged."""
    md = cr.Metadata()
    md.locate_and_load_xml(REPO_PATH)
    for key in md.keys():
        pkg = md.get(key)
        print_package_info(pkg)

def second_method():
    """Preferred method for repodata parsing.

    Important callbacks for repodata parsing:

    newpkgcb
    --------
    Via newpkgcb (Package callback) you could directly
    affect if the current package element should be parsed
    or not. This decision could be based on
    three values that are available as attributtes
    in the <package> element. This values are:
     - pkgId (package checksum)
     - name (package name)
     - arch (package architecture)
    (Note: This is applicable only for filelists.xml and other.xml,
     primary.xml doesn't contain this information in <package> element)

    If newpkgcb returns a package object, the parsed data
    will be loaded to this package object. If it returns a None,
    package element is skiped.

    This could help you to reduce a memory requirements because
    non wanted packages could be skiped without need to
    store them into the memory.

    If no newpkgcb is specified, default callback returning
    a new package object is used.

    pkgcb
    -----
    Callback called when a <package> element parsing is done.
    Its argument is a package object that has been previously
    returned by the newpkgcb.
    This function should return True if parsing should continue
    or False if parsing should be interrupted.

    Note: Both callbacks are optional, BUT at least one
          MUST be used (newpkgcb or pkgcb)!

    warningcb
    ---------
    Warning callbacks is called when a non-fatal oddity of prased XML
    is detected.
    If True is returned, parsing continues. If return value is False,
    parsing is terminated.
    This callback is optional.
    """

    primary_xml_path   = None
    filelists_xml_path = None
    other_xml_path     = None

    #
    # repomd.xml parsing
    #

    # Parse repomd.xml to get paths (1. Method - Repomd object based)
    #   Pros: Easy to use
    repomd = cr.Repomd(os.path.join(REPO_PATH, "repodata/repomd.xml"))

    # Parse repomd.xml (2. Method - Parser based)
    #   Pros: Warning callback could be specified
    def warningcb(warning_type, message):
        """Optional callback for warnings about
        wierd stuff and formatting in XML.

        :param warning_type: Integer value. One from
                             the XML_WARNING_* constants.
        :param message: String message.
        """
        print("PARSER WARNING: %s" % message)
        return True

    repomd2 = cr.Repomd()
    cr.xml_parse_repomd(os.path.join(REPO_PATH, "repodata/repomd.xml"),
                                     repomd2, warningcb)

    # Get stuff we need
    #   (repomd or repomd2 could be used, both have the same values)
    for record in repomd.records:
        if record.type == "primary":
            primary_xml_path = record.location_href
        elif record.type == "filelists":
            filelists_xml_path = record.location_href
        elif record.type == "other":
            other_xml_path = record.location_href


    #
    # Main XML metadata parsing (primary, filelists, other)
    #

    packages = {}

    def pkgcb(pkg):
        # Called when whole package entry in xml is parsed
        packages[pkg.pkgId] = pkg

    def newpkgcb(pkgId, name, arch):
        # Called when new package entry is encountered
        # And only opening <package> element is parsed
        # This function has to return a package to which
        # parsed data will be added or None if this package
        # should be skiped.
        return packages.get(pkgId, None)

    # Option do_files tells primary parser to skip <file> element of package.
    # If you plan to parse filelists.xml after the primary.xml, always
    # set do_files to False.
    cr.xml_parse_primary(os.path.join(REPO_PATH, primary_xml_path),
                         pkgcb=pkgcb,
                         do_files=False,
                         warningcb=warningcb)

    cr.xml_parse_filelists(os.path.join(REPO_PATH, filelists_xml_path),
                           newpkgcb=newpkgcb,
                           warningcb=warningcb)

    cr.xml_parse_other(os.path.join(REPO_PATH, other_xml_path),
                       newpkgcb=newpkgcb,
                       warningcb=warningcb)

    for pkg in packages.values():
        print_package_info(pkg)


if __name__ == "__main__":
    print('"All in one shot" method:')
    first_method()

    print()

    print("Callback based method:")
    second_method()

