#!/usr/bin/env  python

from __future__ import print_function

import os
import sys
import logging
import argparse
import librepo
import deltarepo
from deltarepo import DeltaRepoError, DeltaRepoPluginError

LOG_FORMAT = "%(message)s"

def parse_options():
    parser = argparse.ArgumentParser(description="Update a local repository",
                usage="%(prog)s [options] <localrepo>\n")
    parser.add_argument('localrepo', nargs=1)
    parser.add_argument('--debug', action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--version", action="store_true",
                        help="Show version number and quit.")
    parser.add_argument("-q", "--quiet", action="store_true",
                        help="Run in quiet mode.")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Run in verbose mode.")
    parser.add_argument("--drmirror", action="append",
                        help="Mirror with delta repositories.")
    parser.add_argument("--repo", action="append",
                        help="Repo baseurl")
    parser.add_argument("--repomirrorlist",
                        help="Repo mirrorlist")
    parser.add_argument("--repometalink",
                        help="Repo metalink")

    args = parser.parse_args()

    # Error checks

    if args.version:
        return args

    if not args.localrepo:
        parser.error("Exactly one local repo must be specified")

    if args.quiet and args.verbose:
        parser.error("Cannot use quiet and verbose simultaneously!")

    #if args.outputdir and not args.gendeltareposfile:
    #    parser.error("--outputdir cannot be used")
    #elif args.outputdir and not os.path.isdir(args.outputdir):
    #    parser.error("--outputdir must be a directory: %s" % args.outputdir)

    if not os.path.isdir(args.localrepo) or os.path.isdir(os.path.join(args.localrepo, "repodata")):
        parser.error("{0} is not a repository (a directory containing "
                     "repodata/ dir expected)".format(args.localrepo))

    origin_repo = False
    if args.repo or args.repomirrorlist or args.repometalink:
        origin_repo = True

    if not args.drmirror and not origin_repo:
        parser.error("Nothing to do. No mirror with deltarepos nor origin repo specified.")

    if args.debug:
        args.verbose = True

    return args

def print_version():
    print("RepoUpdater: {0} (librepo: %s)".format(
            deltarepo.VERBOSE_VERSION, librepo.VERSION))

def setup_logging(quiet, verbose):
    logger = logging.getLogger("repoupdater")
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
    pass

if __name__ == "__main__":
    args = parse_options()

    if args.version:
        print_version()
        sys.exit(0)

    logger = setup_logging(args.quiet, args.verbose)

    try:
        main(args, logger)
    except (DeltaRepoError, DeltaRepoPluginError) as err:
        if args.debug:
            raise
        print("Error: {0}".format(err), file=sys.stderr)
        sys.exit(1)

    sys.exit(0)