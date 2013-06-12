#!/usr/bin/env  python

import sys
import os.path
import hashlib
import logging
from optparse import OptionParser, OptionGroup
import deltarepo

LOG_FORMAT = "%(message)s"

def parse_options():
    parser = OptionParser("usage: %prog [options] <first_repo> <second_repo>\n" \
                          "       %prog --apply <repo> <delta_repo>")
    parser.add_option("--version", action="store_true",
                      help="Show version number and quit.")
    parser.add_option("-q", "--quiet", action="store_true",
                      help="Run in quiet mode.")
    parser.add_option("-v", "--verbose", action="store_true",
                      help="Run in verbose mode.")
    #parser.add_option("-l", "--list-datatypes", action="store_true",
    #                  help="List datatypes for which delta is supported.")
    parser.add_option("-o", "--outputdir", action="store", metavar="DIR",
                      help="Output directory.", default="./")

    group = OptionGroup(parser, "Delta generation")
    group.add_option("--skip", action="append", metavar="DATATYPE",
                     help="Skip delta on the DATATYPE. Could be specified "\
                     "multiple times. (E.g., --skip=comps)")
    group.add_option("--do-only", action="append", metavar="DATATYPE",
                     help="Do delta only for the DATATYPE. Could be specified "\
                     "multiple times. (E.g., --do-only=primary)")
    group.add_option("-t", "--id-type", action="store", metavar="HASHTYPE",
                     help="Hash function for the ids (RepoId and DeltaRepoId). " \
                     "Default is sha256.", default="sha256")
    parser.add_option_group(group)

    group = OptionGroup(parser, "Delta application")
    group.add_option("-a", "--apply", action="store_true",
                     help="Enable delta application mode.")
    group.add_option("-d", "--database", action="store_true",
                     help="Gen database.")
    parser.add_option_group(group)

    options, args = parser.parse_args()

    # Error checks

    if options.version:
        return (options.args)

    if len(args) != 2:
        parser.error("Two repository paths have to be specified!")

    if options.id_type not in hashlib.algorithms:
        parser.error("Unsupported hash algorithm %s" % options.id_type)

    if options.quiet and options.verbose:
        parser.error("Cannot use quiet and verbose simultaneously!")

    if not os.path.isdir(args[0]) or \
       not os.path.isdir(os.path.join(args[0], "repodata")) or \
       not os.path.isfile(os.path.join(args[0], "repodata", "repomd.xml")):
        parser.error("Not a repository: %s" % args[0])

    if not os.path.isdir(args[1]) or \
       not os.path.isdir(os.path.join(args[1], "repodata")) or \
       not os.path.isfile(os.path.join(args[1], "repodata", "repomd.xml")):
        parser.error("Not a repository: %s" % args[1])

    if not os.path.isdir(options.outputdir):
        parser.error("Not a directory: %s" % options.outputdir)

    return (options, args)

def print_version():
    print "DeltaRepo: %s" % deltarepo.VERBOSE_VERSION

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

if __name__ == "__main__":
    options, args = parse_options()

    if options.version:
        print_version()
        sys.exit(0)

    logger = setup_logging(options.quiet, options.verbose)

    generator = deltarepo.DeltaRepoGenerator(id_type=options.id_type,
                                             logger=logger)

    # TODO: check if repo is really delta repo (must has a repoid and removed.xml)

    if options.apply:
        # Applying delta
        generator.applydelta(args[0], args[1], out_path=options.outputdir,
                             database=options.database)
    else:
        # Do delta
        generator.gendelta(args[0], args[1], out_path=options.outputdir,
                           do_only=options.do_only, skip=options.skip)
