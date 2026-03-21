#!/usr/bin/env python3

import sys
import os.path
from optparse import OptionParser

VERSION_FILE_PATH = "VERSION"


def parse(root_dir):
    path = os.path.join(root_dir, VERSION_FILE_PATH)
    if not os.path.exists(path):
        print("File {path} doesn't exist".format(path=path))
        return None

    content = open(path, "r").read().strip()
    parts = content.split(".")
    ver = {}
    ver['major'] = parts[0]
    ver['minor'] = parts[1]
    ver['patch'] = parts[2]
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
        parser.error("Directory {path} doesn't exist".format(path=path))

    ver = parse(path)
    if ver is None:
        sys.exit(1)

    if options.major:
        print(ver['major'])
    elif options.minor:
        print(ver['minor'])
    elif options.patch:
        print(ver['patch'])
    else:
        print("{major}.{minor}.{patch}".format(**ver))

    sys.exit(0)
