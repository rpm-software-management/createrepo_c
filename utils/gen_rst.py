#!/usr/bin/python

import sys
import re
import datetime
from optparse import OptionParser


class Info(object):
    def __init__(self, name, description=None, synopsis=None, copyright=None, options=None):
        self.name = name
        self.description = description
        self.synopsis = synopsis
        self.copyright = copyright
        self.options = options  # list of dictionaries with keys ["long_name", "short_name", "description", "arg_description"]

    def gen_rst(self):
        rst  = ".. -*- coding: utf-8 -*-\n\n"
        rst += "%s\n" % ("=" * len(self.name),)
        rst += "%s\n" % self.name
        rst += "%s\n\n" % ("=" * len(self.name),)

        # Add synopsis
        #if self.synopsis:
        #    rst += ":Synopsis: %s\n\n" % self.synopsis

        # Add description
        if self.description:
            rst += ":Subtitle: %s\n\n" % self.description

        # Add copyright
        if self.copyright:
            rst += ":Copyright: %s\n" % self.copyright

        # Add date
        rst += ":Date: $Date: %s $\n\n" % datetime.datetime.strftime(datetime.datetime.utcnow(), format="%F %X")

        # Add options
        rst += "-------\n"
        rst += "OPTIONS\n"
        rst += "-------\n"

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
        print "Error: Cannot open file %s" % filename
        return args

    re_cmd_entries = re.compile(r"static[ ]+GOptionEntry[^{]*{(?P<entries>.*)\s*NULL\s*}\s*};", re.MULTILINE|re.DOTALL)
    match = re_cmd_entries.search(content)
    if not match:
        print "Warning: Cannot find GOptionEntry section in %s" % filename
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
        if short_name and (short_name == "NULL" or short_name == "0"):
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

    print >> sys.stderr, "Loaded %2d arguments" % (i,)
    return args



if __name__ == "__main__":
    parser = OptionParser('usage: %prog [options] <filename> [--mergerepo]')
    parser.add_option('-m', '--mergerepo', action="store_true", help="Gen rst for mergerepo")
    options, args = parser.parse_args()

    if len(args) < 1:
        print >> sys.stderr, "Error: Must specify a input filename. (Example: ../src/cmd_parser.c)"
        sys.exit(1)

    args = parse_arguments_from_c_file(args[0])

    if not options.mergerepo:
        NAME = "createrepo_c"
        info = Info(NAME,
                description="C implementation of createrepo",
                synopsis="%s [options] <directory>" % (NAME,),
                options=args)
    else:
        NAME = "mergerepo_c"
        info = Info(NAME,
                description="C implementation of mergerepo",
                synopsis="%s [options] <directory>" % (NAME,),
                options=args)

    ret = info.gen_rst()
    if not ret:
        print >> sys.stderr, "Error: Rst has not been generated"
        sys.exit(1)

    print ret
