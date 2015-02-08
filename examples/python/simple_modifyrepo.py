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
    uinfo_xml = os.path.join(repodata, 'updateinfo.xml')
    shutil.copyfile(filename, uinfo_xml)

    uinfo_rec = cr.RepomdRecord('updateinfo', uinfo_xml)
    uinfo_rec_comp = uinfo_rec.compress_and_fill(cr.SHA256, cr.XZ)
    uinfo_rec_comp.rename_file()
    uinfo_rec_comp.type = 'updateinfo'
    os.unlink(uinfo_xml)

    repomd_xml = os.path.join(repodata, 'repomd.xml')
    repomd = cr.Repomd(repomd_xml)
    repomd.set_record(uinfo_rec_comp)
    with file(repomd_xml, 'w') as repomd_file:
        repomd_file.write(repomd.xml_dump())


if __name__ == '__main__':
    # Generate the updateinfo.xml
    execfile('updateinfo_gen_02.py')

    modifyrepo(OUT_FILE, REPO_PATH)
