#!/usr/bin/env python

import sys
import os
import os.path
import hashlib

import createrepo_c as cr

REPO_PATH = "repo/"

class CalculationException(Exception):
    pass

def calculate_contenthash(path):
    if not os.path.isdir(path) or \
       not os.path.isdir(os.path.join(path, "repodata/")):
        raise AttributeError("Not a repo: {0}".format(path))

    repomd_path = os.path.join(path, "repodata/repomd.xml")
    repomd = cr.Repomd(repomd_path)

    primary_path = None
    for rec in repomd.records:
        if rec.type == "primary":
            primary_path = rec.location_href
            break

    if not primary_path:
        raise CalculationException("primary metadata are missing")

    pkgids = []

    def pkgcb(pkg):
        pkgids.append("{0}{1}{2}".format(pkg.pkgId,
                                         pkg.location_href,
                                         pkg.location_base or ''))

    cr.xml_parse_primary(os.path.join(path, primary_path),
                         pkgcb=pkgcb)

    contenthash = hashlib.new("sha256")
    for pkgid in sorted(pkgids):
        contenthash.update(pkgid.encode('utf-8'))
    return contenthash.hexdigest()

if __name__ == "__main__":
    path = REPO_PATH
    if len(sys.argv) == 2:
        path = sys.argv[1]
    print(calculate_contenthash(path))
