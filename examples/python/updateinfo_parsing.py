#!/usr/bin/env python

import os
import os.path
import optparse

import createrepo_c as cr


def parse_updateinfo(path):
    uinfo = cr.UpdateInfo(path)
    for update in uinfo.updates:
        print("From:         %s" % update.fromstr)
        print("Status:       %s" % update.status)
        print("Type:         %s" % update.type)
        print("Version:      %s" % update.version)
        print("Id:           %s" % update.id)
        print("Title:        %s" % update.title)
        print("Issued date:  %s" % update.issued_date)
        print("Updated date: %s" % update.updated_date)
        print("Rights:       %s" % update.rights)
        print("Release:      %s" % update.release)
        print("Pushcount:    %s" % update.pushcount)
        print("Severity:     %s" % update.severity)
        print("Summary:      %s" % update.summary)
        print("Description:  %s" % update.description)
        print("Solution:     %s" % update.solution)
        print("References:")
        for ref in update.references:
            print("  Href:  %s" % ref.href)
            print("  Id:    %s" % ref.id)
            print("  Type:  %s" % ref.type)
            print("  Title: %s" % ref.title)
            print("  ----------------------------")
        print("Pkglist (collections):")
        for col in update.collections:
            print("  Short: %s" % col.shortname)
            print("  name:  %s" % col.name)
            print("  Packages:")
            for pkg in col.packages:
                print("    Name:     %s" % pkg.name)
                print("    Version:  %s" % pkg.version)
                print("    Release:  %s" % pkg.release)
                print("    Epoch:    %s" % pkg.epoch)
                print("    Arch:     %s" % pkg.arch)
                print("    Src:      %s" % pkg.src)
                print("    Filename: %s" % pkg.filename)
                print("    Sum:      %s" % pkg.sum)
                print("    Sum type: %s (%s)" % (pkg.sum_type, cr.checksum_name_str(pkg.sum_type)))
                print("    Reboot suggested: %s" % pkg.reboot_suggested)
            print("  ----------------------------")
        print("==============================")


if __name__ == "__main__":
    parser = optparse.OptionParser(usage="%prog PATH_TO_UPDATEINFO")
    options, args = parser.parse_args()
    if len(args) != 1:
        parser.error("You have to specify exactly one update info")
    parse_updateinfo(args[0])
