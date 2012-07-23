#!/usr/bin/python

import re
import sys
import os.path
from optparse import OptionParser

VERSION_H_PATH = "src/version.h"


def parse(root_dir):
    path = os.path.join(root_dir, VERSION_H_PATH)
    if not os.path.exists(path):
        print "File %s doesn't exists" % path
        return None

    content = open(path, "r").read()
    ver = {}
    ver['major'] = re.search('#define\s+CR_MAJOR_VERSION\s+(\d)', content).group(1)
    ver['minor'] = re.search('#define\s+CR_MINOR_VERSION\s+(\d)', content).group(1)
    ver['patch'] = re.search('#define\s+CR_PATCH_VERSION\s+(\d)', content).group(1)
    return ver


if __name__ == "__main__":
    parser = OptionParser("usage: %prog <project_root_dir> [--major|--minor|--patch]")
    parser.add_option("--major", action="store_true", help="Return major version")
    parser.add_option("--minor", action="store_true", help="Return minor version")
    parser.add_option("--patch", action="store_true", help="Return patch version")
    options, args = parser.parse_args()

    if len(args) != 1:
        parser.error("Must specify a project root directory")

    path = args[0]

    if not os.path.isdir(path):
        parser.error("Directory %s doesn't exists" % path)

    ver = parse(path)
    if ver is None:
        sys.exit(1)

    if options.major:
        print ver['major']
    elif options.minor:
        print ver['minor']
    elif options.patch:
        print ver['patch']
    else:
        print "%(major)s.%(minor)s.%(patch)s" % ver

    sys.exit(0)
