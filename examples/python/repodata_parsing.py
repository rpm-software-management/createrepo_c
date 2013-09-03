#!/usr/bin/env python

import os
import os.path

import createrepo_c as cr

REPO_PATH = "repo/"

def first_method():
    md = cr.Metadata()
    md.locate_and_load_xml(REPO_PATH)
    for key in md.keys():
        pkg = md.get(key)
        print pkg, pkg.location_href

        #print pkg.name
        #print pkg.location_href
        #print "  Packages:"

        #for f in pkg.files:
        #    print f
        #print "  Changelogs:"
        #for c in pkg.changelogs:
        #    print c
        #print "  Obsoletes:"
        #for o in pkg.obsoletes:
        #    print o

def second_method():
    # Parse repomd.xml to get paths
    repomd = cr.Repomd(os.path.join(REPO_PATH, "repodata/repomd.xml"))

    primary_xml_path   = None
    filelists_xml_path = None
    other_xml_path     = None

    for record in repomd.records:
        if record.type == "primary":
            primary_xml_path = record.location_href
        elif record.type == "filelists":
            filelists_xml_path = record.location_href
        elif record.type == "other":
            other_xml_path = record.location_href

    # Parse the xml files
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

    cr.xml_parse_primary(os.path.join(REPO_PATH, primary_xml_path),
                         pkgcb=pkgcb,
                         do_files=0)

    cr.xml_parse_filelists(os.path.join(REPO_PATH, filelists_xml_path),
                            newpkgcb=newpkgcb)

    for pkg in packages.itervalues():
        print pkg, pkg.location_href


if __name__ == "__main__":
    print "First method:"
    first_method()

    print

    print "Second method:"
    second_method()

