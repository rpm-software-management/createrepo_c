import logging
import hashlib
import createrepo_c as cr

DEFAULT_CHECKSUM_NAME = "sha256"
DEFAULT_CHECKSUM_TYPE = cr.SHA256
DEFAULT_COMPRESSION_TYPE = cr.GZ


class LoggingInterface(object):
    """Base class with logging support.
    Other classes inherit this class to obtain
    support of logging methods.
    """

    def __init__(self, logger=None):
        self.logger = None
        self._set_logger(logger)

    def _set_logger(self, logger=None):
        if logger is None:
            logger = logging.getLogger()
            logger.disabled = True
        self.logger = logger

    def _get_logger(self):
        return self.logger

    def _log(self, level, msg):
        self.logger.log(level, msg)

    def _debug(self, msg):
        self._log(logging.DEBUG, msg)

    def _info(self, msg):
        self._log(logging.INFO, msg)

    def _warning(self, msg):
        self._log(logging.WARNING, msg)

    def _error(self, msg):
        self._log(logging.ERROR, msg)

    def _critical(self, msg):
        self._log(logging.CRITICAL, msg)

def calculate_contenthash(primary_xml_path, contenthash_type="sha256"):
    pkgids = []

    def pkgcb(pkg):
        pkgids.append("{0}{1}{2}".format(pkg.pkgId,
                                         pkg.location_href,
                                         pkg.location_base or ''))

    cr.xml_parse_primary(primary_xml_path, pkgcb=pkgcb)

    contenthash = hashlib.new(contenthash_type)
    for pkgid in sorted(pkgids):
        contenthash.update(pkgid)
    return contenthash.hexdigest()

