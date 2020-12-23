#!/usr/bin/env python3

import sys
import re
import datetime
from optparse import OptionParser


class Info(object):
    def __init__(self, name, summary=None, description=None, synopsis=None, copyright=None, options=None):
        self.name = name
        self.summary = summary
        self.description = description
        self.synopsis = synopsis
        self.copyright = copyright
        self.options = options  # list of dictionaries with keys ["long_name", "short_name", "description", "arg_description"]

    def gen_rst(self):
        rst  = ".. -*- coding: utf-8 -*-\n\n"
        rst += "%s\n" % ("=" * len(self.name),)
        rst += "%s\n" % self.name
        rst += "%s\n\n" % ("=" * len(self.name),)

        # Add summary
        if self.summary:
            rst += "%s\n" % ("-" * len(self.summary))
            rst += "%s\n" % self.summary
            rst += "%s\n" % ("-" * len(self.summary))
            rst += "\n"

        # Add copyright
        if self.copyright:
            rst += ":Copyright: %s\n" % self.copyright

        # Add manual page section
        rst += ":Manual section: 8\n"

        # Add date
        rst += ":Date: $Date: %s $\n\n" % datetime.datetime.strftime(datetime.datetime.utcnow(), format="%F %X")

        # Add synopsis
        if self.synopsis:
            rst += "SYNOPSIS\n"
            rst += "========\n\n"
            for line in self.synopsis:
                rst += "%s\n\n" % line
            rst += "\n"

        # Add description
        if self.description:
            rst += "DESCRIPTION\n"
            rst += "===========\n\n"
            for line in self.description:
                rst += "%s\n\n" % line
            rst += "\n"

        # Add options
        rst += "OPTIONS\n"
        rst += "=======\n"

        for command in self.options:
            cmd  = ""
            if command["short_name"]:
                cmd += "-%s " % command["short_name"]
            cmd += "--%s" % command["long_name"]
            if command["arg_description"]:
                cmd += " %s" % command["arg_description"]

            rst += "%s\n" % cmd
            rst += "%s\n" % ("-" * len(cmd))
            rst += "%s\n" % command["description"]

            rst += "\n"

        return rst


def parse_arguments_from_c_file(filename):
    args = []

    try:
        content = open(filename, "r").read()
    except IOError:
        print("Error: Cannot open file %s" % filename)
        return args

    re_cmd_entries = re.compile(r"\s*(static|const)[ ]+GOptionEntry[^{]*{(?P<entries>.*)\s*NULL\s*}[,]?\s*};", re.MULTILINE|re.DOTALL)
    match = re_cmd_entries.search(content)
    if not match:
        print("Warning: Cannot find GOptionEntry section in %s" % filename)
        return args

    re_single_entry = re.compile(r"""{\s*"(?P<long_name>[^"]*)"\s*,        # long name
                                     \s*'?(?P<short_name>[^',]*)'?\s*,     # short name
                                     \s*[^,]*\s*,                          # flags
                                     \s*[^,]*\s*,                          # arg type
                                     \s*[^,]*\s*,                          # arg data pointer
                                     \s*("(?P<description>.*?)")\s*,       # description
                                     \s*"?(?P<arg_description>[^"}]*)"?    # arg description
                                  \s*},""", re.MULTILINE|re.DOTALL|re.VERBOSE)
    raw_entries_str = match.group("entries")

    start = 0
    entry_match = re_single_entry.search(raw_entries_str)
    i = 1
    while entry_match:
        long_name = entry_match.group("long_name").strip()
        short_name = entry_match.group("short_name").strip()
        description = entry_match.group("description").strip()
        arg_description = entry_match.group("arg_description").strip()

        # Normalize short name
        if short_name in ("NULL", "0", "\\0"):
            short_name = None

        # Normalize description
        description = description.replace('\\"', "\\\\'")
        description = description.replace("\\\\'", '"')
        description = description.replace('"', "")
        description = description.replace('\n', '')
        description = description.replace('\t', '')

        # Remove multiple whitespaces from description
        while True:
            new_description = description.replace("  ", " ")
            if new_description == description:
                break
            description = new_description
        description = description.strip()

        # Normalize arg_description
        if arg_description and (arg_description == "NULL" or arg_description == "0"):
            arg_description = None

        # Store option into list
        args.append({'long_name': long_name,
                     'short_name': short_name,
                     'description': description,
                     'arg_description': arg_description
                     })

        # Continue to next option
        i += 1
        start += entry_match.end(0)
        entry_match = re_single_entry.search(raw_entries_str[start:])
    # End while

    print("Loaded %2d arguments" % (i,), file=sys.stderr)
    return args



if __name__ == "__main__":
    parser = OptionParser('usage: %prog [options] <filename> [--mergerepo|--modifyrepo|--sqliterepo]')
    parser.add_option('-m', '--mergerepo', action="store_true", help="Gen rst for mergerepo")
    parser.add_option('-r', '--modifyrepo', action="store_true", help="Gen rst for modifyrepo")
    parser.add_option('-s', '--sqliterepo', action="store_true", help="Gen rst for sqliterepo")
    options, args = parser.parse_args()

    if len(args) < 1:
        print("Error: Must specify a input filename. (Example: ../src/cmd_parser.c)", file=sys.stderr)
        sys.exit(1)

    args = parse_arguments_from_c_file(args[0])

    if options.mergerepo:
        NAME = "mergerepo_c"
        info = Info(NAME,
                summary="Merge multiple rpm-md format repositories together",
                synopsis=["%s [options] --repo repo1 --repo repo2" % (NAME,)],
                options=args)
    elif options.modifyrepo:
        NAME = "modifyrepo_c"
        info = Info(NAME,
                summary="Modify a repomd.xml of rpm-md format repository",
                synopsis=["%s [options] <input metadata> <output repodata>" % (NAME,),
                          "%s --remove <metadata type> <output repodata>" % (NAME,),
                          "%s [options] --batchfile <batch file> <output repodata>" % (NAME,) ],
                options=args)
    elif options.sqliterepo:
        NAME = "sqliterepo_c"
        info = Info(NAME,
                summary="Generate sqlite db files for a repository in rpm-md format",
                synopsis=["%s [options] <repo_directory>" % (NAME,) ],
                options=args)
    else:
        NAME = "createrepo_c"
        info = Info(NAME,
                summary="Create rpm-md format (xml-rpm-metadata) repository",
                synopsis=["%s [options] <directory>" % (NAME,)],
                description=["Uses rpm packages from <directory> to create repodata.",
                             "If compiled with libmodulemd support modular metadata inside <directory> identified by the patterns below are automatically collected, merged and added to the repodata.",
                             "The patterns are:",
                             " - \*.modulemd.yaml (recommended file name: N:S:V:C:A.modulemd.yaml)",
                             " - \*.modulemd-defaults.yaml (recommended file name: N.modulemd-defaults.yaml)",
                             " - modules.yaml (recommended way of importing multiple documents at once)"],
                options=args)


    ret = info.gen_rst()
    if not ret:
        print("Error: Rst has not been generated", file=sys.stderr)
        sys.exit(1)

    print(ret)
