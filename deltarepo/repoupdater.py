#!/usr/bin/env  python

from __future__ import print_function

import os
import sys
import logging
import argparse
import librepo
import deltarepo
from deltarepo import DeltaRepoError, DeltaRepoPluginError
from deltarepo.updater_common import LocalRepo, OriginRepo, DRMirror, UpdateSolver, Updater
from deltarepo import needed_delta_metadata

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
    parser.add_argument("--target-contenthash",
                        help="Target content hash (if no --repo(mirrorlist|metalink)? used)")
    parser.add_argument("--target-contenthash-type", default="sha256",
                        help="Type of target content hash. 'sha256' is default value.")
    parser.add_argument("--update-only-present", action="store_true",
                        help="Update only metadata that are present in current repo. "
                             "(Newly added metadata will not be downloaded, missing "
                             "metadata will be ignored)")

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

    if not os.path.isdir(args.localrepo[0]) or not os.path.isdir(os.path.join(args.localrepo[0], "repodata")):
        parser.error("{0} is not a repository (a directory containing "
                     "repodata/ dir expected)".format(args.localrepo[0]))

    origin_repo = False
    if args.repo or args.repomirrorlist or args.repometalink:
        origin_repo = True

    if not args.drmirror and not origin_repo:
        parser.error("Nothing to do. No mirror with deltarepos nor origin repo specified.")

    if origin_repo and args.target_contenthash:
        parser.error("Origin repo shouldn't be specified if --target-contenthash is used")

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

def update_with_deltas(args, drmirros, localrepo, originrepo, logger):
    whitelisted_metadata = None
    if args.update_only_present:
        whitelisted_metadata = needed_delta_metadata(localrepo.present_metadata)
        logger.debug("Using metadata whitelist")
        logger.debug("Locally available metadata: {0}".format(localrepo.present_metadata))
        logger.debug("Final whitelist: {0}".format(whitelisted_metadata))

    updatesolver = UpdateSolver(drmirros,
                                whitelisted_metadata=whitelisted_metadata,
                                logger=logger)

    # Get source hash
    sch_t, sch = updatesolver.find_repo_contenthash(localrepo)
    source_contenthash = sch
    source_contenthash_type = sch_t

    if not source_contenthash:
        raise DeltaRepoError("No deltas available for {0}".format(localrepo.path))

    # Get target hash
    if originrepo:
        # Get origin repo's contenthash
        tch_t, tch = updatesolver.find_repo_contenthash(originrepo)
        target_contenthash = tch
        target_contenthash_type = tch_t
    else:
        # Use content hash specified by user
        target_contenthash = args.target_contenthash
        target_contenthash_type = args.target_contenthash_type

    if not target_contenthash:
        raise DeltaRepoError("No deltas available - Patch for the current "
                             "version of the remote repo is not available")

    if source_contenthash_type != target_contenthash_type:
        raise DeltaRepoError("Types of contenthashes doesn't match {0} != {1}"
                             "".format(source_contenthash_type, target_contenthash_type))

    # Resolve path
    resolved_path = updatesolver.resolve_path(source_contenthash, target_contenthash)

    # TODO check cost, if bigger then cost of downloading
    #      origin repo then download origin repo

    # Apply the path
    updater = Updater(localrepo, None, logger)
    updater.apply_resolved_path(resolved_path)


def main(args, logger):
    localrepo = LocalRepo.from_path(args.localrepo[0])
    originrepo = None
    drmirrors = []

    # TODO: Update to selected revision
    source_contenthash = None
    source_contenthash_type = None
    target_contenthash = None
    target_contenthash_type = None

    if args.repo or args.repometalink or args.repomirrorlist:
        originrepo = OriginRepo.from_url(urls=args.repo,
                                         mirrorlist=args.repomirrorlist,
                                         metalink=args.repometalink)

    for i in args.drmirror:
        drmirror = DRMirror.from_url(i)
        drmirrors.append(drmirror)

    if drmirrors:
        update_with_deltas(args, drmirrors, localrepo, originrepo, logger)
    else:
        # TODO: Just download origin repo
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