#!/usr/bin/env python

import os
import sys
import shutil
import os.path
import createrepo_c as cr

def manual_method(path):
    # Prepare repodata/ directory
    repodata_path = os.path.join(path, "repodata")
    if os.path.exists(repodata_path):
        x = 0
        while True:
            new_repodata_path = "%s_%s" % (repodata_path, x)
            if not os.path.exists(new_repodata_path):
                shutil.move(repodata_path, new_repodata_path)
                break
            x += 1
    os.mkdir(repodata_path)

    # Prepare metadata files
    repomd_path  = os.path.join(repodata_path, "repomd.xml")
    pri_xml_path = os.path.join(repodata_path, "primary.xml.gz")
    fil_xml_path = os.path.join(repodata_path, "filelists.xml.gz")
    oth_xml_path = os.path.join(repodata_path, "other.xml.gz")

    pri_xml = cr.PrimaryXmlFile(pri_xml_path)
    fil_xml = cr.FilelistsXmlFile(fil_xml_path)
    oth_xml = cr.OtherXmlFile(oth_xml_path)

    # List directory and prepare list of files to process
    pkg_list = []
    with os.scandir(path) as entries:
        for entry in entries:
            if entry.is_file() and entry.path.endswith(".rpm"):
                pkg_list.append(entry.path)

    pri_xml.set_num_of_pkgs(len(pkg_list))
    fil_xml.set_num_of_pkgs(len(pkg_list))
    oth_xml.set_num_of_pkgs(len(pkg_list))

    # Process all packages
    for filename in pkg_list:
        pkg = cr.package_from_rpm(filename)
        pkg.location_href = os.path.basename(filename)
        print("Processing: %s" % pkg.nevra())
        pri_xml.add_pkg(pkg)
        fil_xml.add_pkg(pkg)
        oth_xml.add_pkg(pkg)

    pri_xml.close()
    fil_xml.close()
    oth_xml.close()

    # Prepare repomd.xml
    repomd = cr.Repomd()

    # Add records into the repomd.xml
    repomdrecords = (("primary",      pri_xml_path),
                     ("filelists",    fil_xml_path),
                     ("other",        oth_xml_path))
    for name, path in repomdrecords:
        record = cr.RepomdRecord(name, path)
        record.fill(cr.SHA256)
        repomd.set_record(record)

    # Write repomd.xml
    open(repomd_path, "w").write(repomd.xml_dump())

    # DONE!

if __name__ == "__main__":
    if len(sys.argv) != 2 or not os.path.isdir(sys.argv[1]):
        print("Usage: %s <directory>" % (sys.argv[0]))
        sys.exit(1)

    manual_method(sys.argv[1])

    print("Repository created in %s" % sys.argv[1])
