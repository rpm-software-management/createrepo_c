#!/usr/bin/env python

import os
import sys
import shutil
import os.path
import createrepo_c as cr

def do_repodata(path):
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
    pri_db_path  = os.path.join(repodata_path, "primary.sqlite")
    fil_db_path  = os.path.join(repodata_path, "filelists.sqlite")
    oth_db_path  = os.path.join(repodata_path, "other.sqlite")

    pri_xml = cr.PrimaryXmlFile(pri_xml_path)
    fil_xml = cr.FilelistsXmlFile(fil_xml_path)
    oth_xml = cr.OtherXmlFile(oth_xml_path)
    pri_db  = cr.PrimarySqlite(pri_db_path)
    fil_db  = cr.FilelistsSqlite(fil_db_path)
    oth_db  = cr.OtherSqlite(oth_db_path)

    # List directory and prepare list of files to process
    pkg_list = []
    for filename in os.listdir(path):
        filename = os.path.join(path, filename)
        if os.path.isfile(filename) and filename.endswith(".rpm"):
            pkg_list.append(filename)

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
        pri_db.add_pkg(pkg)
        fil_db.add_pkg(pkg)
        oth_db.add_pkg(pkg)

    pri_xml.close()
    fil_xml.close()
    oth_xml.close()

    # Note: DBs are still open! We have to calculate checksums of xml files
    # and insert them to the databases first!

    # Prepare repomd.xml
    repomd = cr.Repomd()

    # Add records into the repomd.xml
    repomdrecords = (("primary",      pri_xml_path, pri_db),
                     ("filelists",    fil_xml_path, fil_db),
                     ("other",        oth_xml_path, oth_db),
                     ("primary_db",   pri_db_path,  None),
                     ("filelists_db", fil_db_path,  None),
                     ("other_db",     oth_db_path,  None))
    for name, path, db_to_update in repomdrecords:
        record = cr.RepomdRecord(name, path)
        record.fill(cr.SHA256)
        if (db_to_update):
            db_to_update.dbinfo_update(record.checksum)
            db_to_update.close()
        repomd.set_record(record)

    # Write repomd.xml
    open(repomd_path, "w").write(repomd.xml_dump())

    # DONE!

if __name__ == "__main__":
    if len(sys.argv) != 2 or not os.path.isdir(sys.argv[1]):
        print("Usage: %s <directory>" % (sys.argv[0]))
        sys.exit(1)

    do_repodata(sys.argv[1])

    print("Repository created in %s" % sys.argv[1])
