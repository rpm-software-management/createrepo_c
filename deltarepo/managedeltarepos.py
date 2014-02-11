#!/usr/bin/env  python

from __future__ import print_function

import os
import sys
import os.path
import logging
import argparse
import tempfile
import shutil
import time
import librepo
import hashlib
import deltarepo
import createrepo_c as cr
from deltarepo import DeltaRepoError, DeltaRepoPluginError

LOG_FORMAT = "%(message)s"

def parse_options():
    parser = argparse.ArgumentParser(description="Manage deltarepos directory.",
                usage="%(prog)s --gendeltareposfile [options] <directory>\n"
                "       %(prog)s [options] <old_repo_dir> <new_repo_dir> [deltarepos_dir]")
    parser.add_argument('dirs', nargs='+')
    parser.add_argument('--debug', action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--version", action="store_true",
                      help="Show version number and quit.")
    parser.add_argument("-q", "--quiet", action="store_true",
                      help="Run in quiet mode.")
    parser.add_argument("-v", "--verbose", action="store_true",
                      help="Run in verbose mode.")
    parser.add_argument("--gendeltareposfile", action="store_true",
                     help="Generate the deltarepos.xml file. Walk recursively "
                          "all specified directories.")

    group = parser.add_argument_group("deltarepos.xml file generation (--gendeltareposfile)")
    group.add_argument("-o", "--outputdir", action="store", metavar="DIR",
                      help="Set different output directory for deltarepos.xml")
    group.add_argument("--force", action="store_true",
                       help="Ignore bad repositories")

    args = parser.parse_args()

    # Error checks

    if args.version:
        return args

    if args.gendeltareposfile:
        # --gendeltareposfile
        if not args.dirs or len(args.dirs) != 1:
            parser.error("Exactly one directory must be specified")
    else:
        # default
        for dir in args.dirs:
            if not os.path.isdir(dir):
                parser.error("{0} is not a directory".format(dir))
        for dir in args.dirs[:2]:
            # Fist two arguments must be a repos
            if not os.path.isdir(os.path.join(dir, "repodata")) or \
               not os.path.isfile(os.path.join(dir, "repodata", "repomd.xml")):
                parser.error("Not a repository: %s" % dir)
        if len (args.dirs) > 3:
            parser.error("Too much directories specified")

    if args.quiet and args.verbose:
        parser.error("Cannot use quiet and verbose simultaneously!")

    if args.outputdir and not args.gendeltareposfile:
        parser.error("--outputdir cannot be used")
    elif args.outputdir and not os.path.isdir(args.outputdir):
        parser.error("--outputdir must be a directory: %s" % args.outputdir)

    if args.debug:
        args.verbose = True

    return args

def print_version():
    print("ManageDeltaRepos: {0} (librepo: %s)".format(
            deltarepo.VERBOSE_VERSION, librepo.VERSION))

def setup_logging(quiet, verbose):
    logger = logging.getLogger("managedeltarepos")
    formatter = logging.Formatter(LOG_FORMAT)
    logging.basicConfig(format=LOG_FORMAT)
    if quiet:
        logger.setLevel(logging.ERROR)
    elif verbose:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)
    return logger

def file_checksum(path, type="sha256"):
    """Calculate file checksum"""
    h = hashlib.new(type)
    with open(path, "rb") as f:
        while True:
            chunk = f.read(1024**2)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()

def parse_repomd(path, logger):
    """Get repo data"""

    # TODO: Switch this function to use createrepo.Repomd()
    #       instead of librepo

    h = librepo.Handle()
    r = librepo.Result()
    h.urls = [path]
    h.local = True
    h.repotype = librepo.LR_YUMREPO
    h.perform(r)

    yum_repo_dict = r.yum_repo
    yum_repomd_dict = r.yum_repomd
    if not yum_repo_dict or "repomd" not in yum_repo_dict:
        raise DeltaRepoError("{0} is not a repository".format(path))
    if not yum_repomd_dict or "deltametadata" not in yum_repomd_dict:
        raise DeltaRepoError("{0} is not a delta repository".format(path))

    return yum_repo_dict, yum_repomd_dict

def repo_size(path, logger):
    """Calculate size of the repository's metadata.
    """

    yum_repo_dict, yum_repomd_dict = parse_repomd(path, logger)

    total_size = 0

    # Sum the sizes
    for md_name, details in yum_repomd_dict.items():
        if not "size" in details or not "size_open" in details:
            continue
        if "size" in details:
            total_size += details["size"]
        else:
            total_size += details["size_open"]

    # Get size of the repomd.xml
    repomd_path = yum_repo_dict.get("repomd")
    if repomd_path:
        total_size += os.path.getsize(repomd_path)

    return total_size

def deltareposrecord_from_repopath(path, logger, strip_path_prefix=True):
    # Parse repo's deltametadata.xml

    path = os.path.abspath(path)

    yum_repo_dict, yum_repomd_dict = parse_repomd(path, logger)

    deltametadata_path = os.path.join(path, yum_repomd_dict["deltametadata"]["location_href"])
    dm = deltarepo.DeltaMetadata()
    dm.xmlparse(deltametadata_path)

    if strip_path_prefix and path.startswith(os.getcwd()):
        path = path[len(os.getcwd())+1:]

    rec = deltarepo.DeltaReposRecord()
    rec.location_base = None
    rec.location_href = path
    rec.size_total = repo_size(path, logger)
    rec.revision_src = dm.revision_src
    rec.revision_dst = dm.revision_dst
    rec.contenthash_src = dm.contenthash_src
    rec.contenthash_dst = dm.contenthash_dst
    rec.contenthash_type = dm.contenthash_type
    rec.timestamp_src = dm.timestamp_src
    rec.timestamp_dst = dm.timestamp_dst

    repomd_path = yum_repo_dict["repomd"]
    rec.repomd_timestamp = int(os.path.getmtime(repomd_path))
    rec.repomd_size = os.path.getsize(repomd_path)
    checksumval = file_checksum(repomd_path)
    rec.repomd_checksums = [("sha256", checksumval)]

    return rec

def write_deltarepos_file(path, records, append=False):
     # Add the record to the deltarepos.xml
    deltareposxml_path = os.path.join(path, "deltarepos.xml.xz")
    drs = deltarepo.DeltaRepos()
    if os.path.isfile(deltareposxml_path) and append:
        drs.xmlparse(deltareposxml_path)
    for rec in records:
        drs.add_record(rec)
    new_content = drs.xmldump()

    f = cr.CrFile(deltareposxml_path, cr.MODE_WRITE, cr.XZ)
    f.write(new_content)
    f.close()

def gen_delta(old_repo_dir, new_repo_dir, logger, deltarepos_dir=None):
    # Gen delta to a temporary directory
    prefix = "deltarepo-{0}-".format(int(time.time()))
    tmp_dir = tempfile.mkdtemp(prefix=prefix, dir="/tmp/")
    try:
        dg = deltarepo.DeltaRepoGenerator(old_repo_dir,
                                          new_repo_dir,
                                          out_path=tmp_dir,
                                          logger=logger)
        dg.gen()
    except Exception:
        shutil.rmtree(tmp_dir)
        raise

    if not deltarepos_dir:
        deltarepos_dir = os.getcwd()

    # Move the delta to the deltarepos_dir or to the current working directory
    dst_dir = os.path.join(deltarepos_dir, os.path.basename(tmp_dir))
    shutil.copytree(tmp_dir, dst_dir)
    shutil.rmtree(tmp_dir)

    # Prepare repo's DeltaReposRecord
    rec = deltareposrecord_from_repopath(dst_dir, logger)

    # Add the record to the deltarepos.xml
    write_deltarepos_file(deltarepos_dir, [rec], append=True)

    return dst_dir

def gen_deltarepos_file(workdir, logger, force=False):
    cwd = os.getcwdu()
    logger.debug("Changing work dir to: {0} (old work dir: {1})".format(
                    workdir, cwd))
    os.chdir(workdir)

    deltareposrecords = []

    # Recursivelly walk the directories and search for repositories
    for root, dirs, files in os.walk(workdir):
        dirs.sort()
        if "repodata" in dirs:
            try:
                rec = deltareposrecord_from_repopath(root, logger)
            except DeltaRepoError:
                if not force:
                    os.chdir(cwd)
                    raise
                logger.warning("Bad repository: {0}".format(root))
            deltareposrecords.append(rec)

    write_deltarepos_file(workdir, deltareposrecords, append=False)
    os.chdir(cwd)


def main(args, logger):
    if args.gendeltareposfile:
        workdir = args.dirs[0]
        gen_deltarepos_file(workdir, logger, force=args.force)
    else:
        old_repo_dir = args.dirs[0]
        new_repo_dir = args.dirs[1]
        deltarepos_dir = args.dirs[2] if len(args.dirs) == 3 else None
        gen_delta(old_repo_dir, new_repo_dir, logger, deltarepos_dir=deltarepos_dir)

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

# TODO:
# - Check for contenthash mishmashes
# - Check for duplicated path (links)
