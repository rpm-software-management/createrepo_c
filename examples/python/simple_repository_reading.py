#!/usr/bin/env python

import os
import sys
import createrepo_c as cr

REPO_PATH = "tests/testdata/repo_with_additional_metadata/"


def parse_repository(path):
    reader = cr.RepositoryReader.from_path(path)

    for package in reader.iter_packages():
        print("Package {}".format(package.nevra()))

    for advisory in reader.advisories():
        print("Advisory {}".format(advisory.id))


if __name__ == "__main__":
    parse_repository(REPO_PATH)