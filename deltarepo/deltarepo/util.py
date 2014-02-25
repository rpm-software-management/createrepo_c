import hashlib
import logging
import createrepo_c as cr


def log(logger, level, msg):
    if not logger:
        return
    logger.log(level, msg)

def pkg_id_str(pkg, logger=None):
    """Return string identifying a package in repodata.
    This strings are used for the RepoId calculation."""
    if not pkg.pkgId:
        log(logger, logging.WARNING, "Missing pkgId in a package!")
    if not pkg.location_href:
        log(logger, logging.WARNING, "Missing location_href at "
                                     "package %s %s" % (pkg.name, pkg.pkgId))

    idstr = "%s%s%s" % (pkg.pkgId or '',
                        pkg.location_href or '',
                        pkg.location_base or '')
    return idstr

def calculate_content_hash(path_to_primary_xml, type="sha256", logger=None):
    pkg_id_strs = []

    def old_pkgcb(pkg):
        pkg_id_strs.append(pkg_id_str(pkg, logger))

    cr.xml_parse_primary(path_to_primary_xml, pkgcb=old_pkgcb, do_files=False)

    pkg_id_strs.sort()

    packages_hash = []
    h = hashlib.new(type)
    for i in pkg_id_strs:
        h.update(i)
    return h.hexdigest()

def size_to_human_readable_str(size_in_bytes):
    if size_in_bytes < 0:
        return "{0}".format(size_in_bytes)

    for x in ['b','KB','MB','GB']:
        if size_in_bytes < 1024.0:
            return "{0:1.3f} {1}".format(size_in_bytes, x)
        size_in_bytes /= 1024.0
    return "{0:1.3f} {1}".format(size_in_bytes, 'TB')