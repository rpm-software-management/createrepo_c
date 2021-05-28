#!/usr/bin/env python

import os
import sys
import createrepo_c as cr


def write_repository_v1(path):
    # List directory and prepare list of files to process
    pkg_list = []

    with os.scandir(path) as entries:
        for entry in entries:
            if entry.is_file() and entry.path.endswith(".rpm"):
                pkg_list.append(entry.path)

    # create a RepositoryWriter with a context manager - finish() is called automatically
    # let's just use the default options
    with cr.RepositoryWriter(path) as writer:
        writer.repomd.add_repo_tag("Fedora 34")
        writer.repomd.set_revision("1628310033")
        # we have to set the number of packages we will add, before we add them
        writer.set_num_of_pkgs(len(pkg_list))

        for filename in pkg_list:
            pkg = writer.add_pkg_from_file(filename)
            print("Added: %s" % pkg.nevra())


def write_repository_v2(path):
    # List directory and prepare list of files to process
    pkg_list = []

    with os.scandir(path) as entries:
        for entry in entries:
            if entry.is_file() and entry.path.endswith(".rpm"):
                pkg_list.append(entry.path)

    # create a writer without a context manager - you need to manually call finish()
    # change a couple of the defaults too
    writer = cr.RepositoryWriter(
        path,
        unique_md_filenames=False,
        changelog_limit=4,
        checksum_type=cr.SHA512,
        compression=cr.GZ_COMPRESSION,
    )
    writer.repomd.add_repo_tag("Fedora 34")
    writer.repomd.set_revision("1628310033")
    writer.set_num_of_pkgs(len(pkg_list))

    for filename in pkg_list:
        pkg = writer.add_pkg_from_file(filename)
        print("Added: %s" % pkg.nevra())

    writer.finish()


if __name__ == "__main__":
    if len(sys.argv) != 2 or not os.path.isdir(sys.argv[1]):
        print("Usage: %s <directory>" % (sys.argv[0]))
        sys.exit(1)

    write_repository_v1(sys.argv[1])
    # write_repository_v2(sys.argv[1])

    print("Repository created in %s" % sys.argv[1])
