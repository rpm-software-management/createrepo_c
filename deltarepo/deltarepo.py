#!/usr/bin/env  python

from __future__ import print_function

import sys
import os.path
import hashlib
import logging
import argparse
import deltarepo

LOG_FORMAT = "%(message)s"

# TODO:
# - Support for type of compression (?)


def parse_options():
    parser = argparse.ArgumentParser(description="Gen/Apply delta on yum repository.",
                usage="%(prog)s [options] <first_repo> <second_repo>\n" \
                      "       %(prog)s --apply <repo> <delta_repo>")
    parser.add_argument('path1', help="First repository")
    parser.add_argument('path2', help="Second repository or delta repository")
    parser.add_argument('--debug', action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--version", action="store_true",
                      help="Show version number and quit.")
    parser.add_argument("-q", "--quiet", action="store_true",
                      help="Run in quiet mode.")
    parser.add_argument("-v", "--verbose", action="store_true",
                      help="Run in verbose mode.")
    #parser.add_option("-l", "--list-datatypes", action="store_true",
    #                  help="List datatypes for which delta is supported.")
    parser.add_argument("-o", "--outputdir", action="store", metavar="DIR",
                      help="Output directory.", default=None)
    parser.add_argument("-d", "--database", action="store_true",
                      help="Force database generation")
    parser.add_argument("--ignore-missing", action="store_true",
                      help="Ignore missing metadata files. (The files that "
                           "are listed in repomd.xml but physically doesn't "
                           "exists)")

    group = parser.add_argument_group("Delta generation")
    #group.add_argument("--skip", action="append", metavar="DATATYPE",
    #                 help="Skip delta on the DATATYPE. Could be specified "\
    #                 "multiple times. (E.g., --skip=comps)")
    #group.add_argument("--do-only", action="append", metavar="DATATYPE",
    #                 help="Do delta only for the DATATYPE. Could be specified "\
    #                 "multiple times. (E.g., --do-only=primary)")
    group.add_argument("-t", "--id-type", action="store", metavar="HASHTYPE",
                     help="Hash function for the ids (Contenthash). " \
                     "Default is sha256.", default="sha256")

    group = parser.add_argument_group("Delta application")
    group.add_argument("-a", "--apply", action="store_true",
                     help="Enable delta application mode.")

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

    if not os.path.isdir(args.path1) or \
       not os.path.isdir(os.path.join(args.path1, "repodata")) or \
       not os.path.isfile(os.path.join(args.path1, "repodata", "repomd.xml")):
        parser.error("Not a repository: %s" % args.path1)

    if not os.path.isdir(args.path2) or \
       not os.path.isdir(os.path.join(args.path2, "repodata")) or \
       not os.path.isfile(os.path.join(args.path2, "repodata", "repomd.xml")):
        parser.error("Not a repository: %s" % args.path2)

    if not os.path.isdir(args.outputdir):
        parser.error("Not a directory: %s" % args.outputdir)

    if args.debug:
        args.verbose = True

    return args

def print_version():
    print("DeltaRepo: {0}".format(deltarepo.VERBOSE_VERSION))

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
    if args.apply:
        # Applying delta
        da = deltarepo.DeltaRepoApplicator(args.path1,
                                           args.path2,
                                           out_path=args.outputdir,
                                           logger=logger,
                                           force_database=args.database,
                                           ignore_missing=args.ignore_missing)
        da.apply()
    else:
        # Do delta
        dg = deltarepo.DeltaRepoGenerator(args.path1,
                                          args.path2,
                                          out_path=args.outputdir,
                                          logger=logger,
                                          contenthash_type=args.id_type,
                                          force_database=args.database,
                                          ignore_missing=args.ignore_missing)
        dg.gen()

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
