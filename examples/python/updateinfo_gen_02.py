#!/usr/bin/env python

import datetime
import createrepo_c as cr

OUT_FILE = "updateinfo.xml.gz"

def generate():
    pkg = cr.UpdateCollectionPackage()
    pkg.name = "Foo"
    pkg.version = "1.2.3"
    pkg.release = "1"
    pkg.epoch = "0"
    pkg.arch = "noarch"
    pkg.src = "foo.src.rpm"
    pkg.filename = "foo-1.2.3-1.rpm"
    pkg.sum = "123456789"
    pkg.sum_type = cr.MD5
    pkg.reboot_suggested = False

    col = cr.UpdateCollection()
    col.shortname = "Bar-product"
    col.name = "Bar Product"
    col.append(pkg)

    ref = cr.UpdateReference()
    ref.href = "http://foo.bar/foobar"
    ref.id = "123"
    ref.type = "self"
    ref.title = "Foo Update"

    rec = cr.UpdateRecord()
    rec.fromstr = "security@foo.bar"
    rec.status = "final"
    rec.type = "enhancement"
    rec.version = "1"
    rec.id = "UPDATE-1"
    rec.title = "Bar Product Update"
    rec.issued_date = datetime.datetime(2014, 8, 14)
    rec.updated_date = datetime.datetime(2014, 8, 14)
    rec.rights = "Copyright 2014 Bar Inc"
    rec.summary = "An update for Bar"
    rec.description = "Fixes a bug"
    rec.append_collection(col)
    rec.append_reference(ref)

    chunk = cr.xml_dump_updaterecord(rec)

    f = cr.UpdateInfoXmlFile(OUT_FILE)
    f.add_chunk(chunk)
    f.close()

    print("See the %s" % OUT_FILE)


if __name__ == "__main__":
    generate()
