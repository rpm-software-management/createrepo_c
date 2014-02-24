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
    parser.add_argument("-t", "--id-type", action="append", metavar="HASHTYPE",
                      help="Hash function for the ids (Contenthash). " \
                      "Default is sha256.", default=[])
    parser.add_argument("-c", "--check", action="store_true",
                      help="Check if content hash in repomd match the real one")
    parser.add_argument("--missing-contenthash-in-repomd-is-ok", action="store_true",
                      help="If --check option is used and contenthash is not specified "
                           "the repomd.xml then assume that checksums matches")

    args = parser.parse_args()

    # Sanity checks

    if args.version:
        return args

    for hash_type in args.id_type:
        if hash_type.lower() not in hashlib.algorithms:
            parser.error("Unsupported hash algorithm %s" % hash_type)

    if not args.id_type:
        args.id_type.append("sha256")

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

def print_contenthashes(args, logger):
    # Print content hash from the repomd.xml
    localrepo = LocalRepo.from_path(args.path, calc_contenthash=False)
    if localrepo.repomd_contenthash and localrepo.repomd_contenthash_type:
        print("R {0} {1}".format(localrepo.repomd_contenthash_type, localrepo.repomd_contenthash))

    # Calculate content hashes
    for hash_type in args.id_type:
        localrepo = LocalRepo.from_path(args.path, contenthash_type=hash_type.lower())
        print("C {0} {1}".format(localrepo.contenthash_type, localrepo.contenthash))

    return True

def check(args, logger):
    # Get type and value of content hash in repomd
    localrepo = LocalRepo.from_path(args.path, calc_contenthash=False)
    if not localrepo.repomd_contenthash or not localrepo.repomd_contenthash_type:
        if args.missing_contenthash_in_repomd_is_ok:
            return True
        logger.warning("Content hash is not specified in repomd.xml")
        return False

    repomd_contenhash_type = localrepo.repomd_contenthash_type
    repomd_contenthash = localrepo.repomd_contenthash

    # Calculate real contenthash
    localrepo = LocalRepo.from_path(args.path, contenthash_type=repomd_contenhash_type)

    # Check both contenthashes
    if localrepo.contenthash != repomd_contenthash:
        logger.error("Content hash from the repomd.xml ({0}) {1} doesn't match the real "
                     "one ({2}) {3}".format(repomd_contenhash_type, repomd_contenthash,
                                            localrepo.contenthash_type, localrepo.contenthash))
        return False

    return True

def main(args, logger):
    if args.check:
        return check(args, logger)
    else:
        return print_contenthashes(args, logger)

if __name__ == "__main__":
    args = parse_options()

    if args.version:
        print_version()
        sys.exit(0)

    logger = setup_logging(args.quiet, args.verbose)

    try:
        ret = main(args, logger)
    except Exception as err:
        if args.debug:
            raise
        print("Error: {0}".format(err), file=sys.stderr)
        sys.exit(1)

    if ret:
        sys.exit(0)

    sys.exit(1)