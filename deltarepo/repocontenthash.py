#!/usr/bin/env  python

from __future__ import print_function

import os
import sys
import hashlib
import logging
import argparse
import deltarepo
from deltarepo import DeltaRepoError
from deltarepo.updater_common import LocalRepo

LOG_FORMAT = "%(message)s"

# TODO:
# Options to show and compare contenthash in repomd.xml


def parse_options():
    parser = argparse.ArgumentParser(description="Get content hash of local repository",
                usage="%(prog)s [options] <repo>")
    parser.add_argument('path', help="Repository")
    parser.add_argument('--debug', action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--version", action="store_true",
                      help="Show version number and quit.")
    parser.add_argument("-q", "--quiet", action="store_true",
                      help="Run in quiet mode.")
    parser.add_argument("-v", "--verbose", action="store_true",
                      help="Run in verbose mode.")
    # TODO
    parser.add_argument("-t", "--id-type", action="store", metavar="HASHTYPE",
                      help="Hash function for the ids (Contenthash). " \
                      "Default is sha256.", default="sha256")

    args = parser.parse_args()

    # Error checks

    if args.version:
        return args

    #if len(args) != 2:
    #    parser.error("Two repository paths have to be specified!")

    if args.id_type not in hashlib.algorithms:
        parser.error("Unsupported hash algorithm %s" % args.id_type)

    if args.quiet and args.verbose:
        parser.error("Cannot use quiet and verbose simultaneously!")

    if not os.path.isdir(args.path) or \
       not os.path.isdir(os.path.join(args.path, "repodata")) or \
       not os.path.isfile(os.path.join(args.path, "repodata", "repomd.xml")):
        parser.error("Not a repository: %s" % args.path)

    if args.debug:
        args.verbose = True

    return args

def print_version():
    print("RepoContentHash: {0}".format(deltarepo.VERBOSE_VERSION))

def setup_logging(quiet, verbose):
    logger = logging.getLogger("deltarepo_logger")
    formatter = logging.Formatter(LOG_FORMAT)
    logging.basicConfig(format=LOG_FORMAT)
    if quiet:
        logger.setLevel(logging.ERROR)
    elif verbose:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)
    return logger

def main(args, logger):
    localrepo = LocalRepo.from_path(args.path)
    print("{0} {1}".format(localrepo.contenthash_type, localrepo.contenthash))

if __name__ == "__main__":
    args = parse_options()

    if args.version:
        print_version()
        sys.exit(0)

    logger = setup_logging(args.quiet, args.verbose)

    try:
        main(args, logger)
    except Exception as err:
        if args.debug:
            raise
        print("Error: {0}".format(err), file=sys.stderr)
        sys.exit(1)

    sys.exit(0)