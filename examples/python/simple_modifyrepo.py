#!/usr/bin/env python
"""
An example of inserting updateinfo.xml into repodata.
"""
import os
import shutil

import createrepo_c as cr

REPO_PATH = "repo/"

def modifyrepo(filename, repodata):
    repodata = os.path.join(repodata, 'repodata')
    uinfo_xml = os.path.join(repodata, os.path.basename(filename))
    shutil.copyfile(filename, uinfo_xml)

    uinfo_rec = cr.RepomdRecord('updateinfo', uinfo_xml)
    uinfo_rec.fill(cr.SHA256)
    uinfo_rec.rename_file()

    repomd_xml = os.path.join(repodata, 'repomd.xml')
    repomd = cr.Repomd(repomd_xml)
    repomd.set_record(uinfo_rec)
    with open(repomd_xml, 'w') as repomd_file:
        repomd_file.write(repomd.xml_dump())


if __name__ == '__main__':
    # Generate the updateinfo.xml
    exec(open("./updateinfo_gen_02.py").read())

    modifyrepo(OUT_FILE, REPO_PATH)
