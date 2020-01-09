#!/usr/bin/env python

import os
import os.path

import createrepo_c as cr

REPO_PATH = "repo/"

def parse_repomd(path):
    repomd = cr.Repomd(path)
    print("Revision:", repomd.revision)
    if repomd.contenthash:
        print("Contenthash:", repomd.contenthash)
        print("Contenthash type:", repomd.contenthash_type)
    print("Repo tags:", repomd.repo_tags)
    print("Content tags:", repomd.content_tags)
    print("Distro tags:", repomd.distro_tags)
    print()
    for rec in repomd.records:
        print("Type:", rec.type)
        print("Location href:", rec.location_href)
        print("Location base:", rec.location_base)
        print("Checksum:", rec.checksum)
        print("Checksum type:", rec.checksum_type)
        print("Checksum open:", rec.checksum_open)
        print("Checksum open type:", rec.checksum_open_type)
        print("Timestamp:", rec.timestamp)
        print("Size:", rec.size)
        print("Size open:", rec.size_open)
        if rec.db_ver:
            print("Db version:", rec.db_ver)
        print()

if __name__ == "__main__":
    repomd_path = os.path.join(REPO_PATH, "repodata/repomd.xml")
    parse_repomd(repomd_path)
