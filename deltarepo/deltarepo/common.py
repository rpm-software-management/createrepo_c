import os
import tempfile
import logging
import xml.dom.minidom
import createrepo_c as cr
from lxml import etree
from .errors import DeltaRepoError

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

class AdditionalXmlData(object):
    """Interface to store/load additional data to/from xml.
    """

    ADDITIONAL_XML_DATA = True

    def __init__(self):
        self._data = {}
        self._lists = {}

    def set(self, key, value):
        """Store a single key-value pair to the XML.
        Both key and value have to be a string.
        Each key could have only single string value.
        No multiple keys with same name are allowed."""
        if not isinstance(key, basestring):
            raise TypeError("expected string as key")
        if not isinstance(value, basestring):
            raise TypeError("expected string as value")
        self._data[key] = value

    def update(self, dictionary):
        """Store multiple key-value pairs to the XML.
        All keys and values have to be a strings.
        Each key could have only single string value.
        No multiple keys with same name are allowed."""
        if not isinstance(dictionary, dict):
            raise TypeError("expected dictionary")

        for key, val in dictionary.items():
            self.set(key, val)

    def append(self, listname, dictionary):
        """Append a multiple key-value pairs to the XML.
        One list/key could have multiple dictionaries."""
        if not isinstance(listname, basestring):
            raise TypeError("expected string")
        if not isinstance(dictionary, dict):
            raise TypeError("expected dict")

        if not listname in self._lists:
            self._lists[listname] = []

        # Validate items first
        for key, val in dictionary.items():
            if not isinstance(key, basestring) or not isinstance(val, basestring):
                raise TypeError("Dict's keys and values must be string")

        self._lists[listname].append(dictionary)

    def get(self, key, default=None):
        """Return a single valued key from the XML"""
        return self._data.get(key, default)

    def get_list(self, key, default=None):
        """Return list (a key with multiple values) of dictionaries"""
        return self._lists.get(key, default)

    def _subelement(self, parent, name, in_attrs=None):
        """Generate an XML element from the content of the object.

        :param parent: Parent xml.dom.Node object
        :param name: Name of the XML element
        :param in_attrs: Dictionary with element attributes.
                         Both keys and values have to be strings."""
        attrs = {}
        attrs.update(self._data)
        if in_attrs:
            attrs.update(in_attrs)
        elem = etree.SubElement(parent, name, attrs)

        for listname, listvalues in self._lists.items():
            for val in listvalues:
                etree.SubElement(elem, listname, val)

        return elem

class PluginBundle(AdditionalXmlData):
    """Object that persistently stores plugin configuration
    in deltametadata.xml XML file.
    To access data use the public methods from AdditionalXmlData object."""
    def __init__(self, name, version):
        AdditionalXmlData.__init__(self)

        if not isinstance(name, basestring):
            raise TypeError("string expected")
        if not isinstance(version, int):
            raise TypeError("integer expected")

        self.name = name        # Plugin name (string)
        self.version = version  # Plugin version (integer)

class DeltaMetadata(AdditionalXmlData):
    """Object that represents deltametadata.xml file in deltarepository.
    The deltametadata.xml persistently stores plugin configuration.
    """

    def __init__(self):
        AdditionalXmlData.__init__(self)

        self.usedplugins = {}

    def add_pluginbundle(self, pluginbundle):
        """Add new pluginbundle to the object"""
        if not isinstance(pluginbundle, PluginBundle):
            raise TypeError("PluginBundle object expected")
        self.usedplugins[pluginbundle.name] = pluginbundle

    def get_pluginbundle(self, name):
        """Get associate PluginBundle object"""
        return self.usedplugins.get(name, None)

    def xmldump(self):
        """Get XML dump"""
        xmltree = etree.Element("deltametadata")

        usedplugins = etree.SubElement(xmltree, "usedplugins")
        for plugin in self.usedplugins.values():
            attrs = {"name": plugin.name, "version": str(plugin.version)}
            plugin._subelement(usedplugins, "plugin", attrs)
        return etree.tostring(xmltree,
                              pretty_print=True,
                              encoding="UTF-8",
                              xml_declaration=True)

    def xmlparse(self, path):
        """Parse data from an xml file"""
        _, tmp_path = tempfile.mkstemp()
        cr.decompress_file(path, tmp_path, cr.AUTO_DETECT_COMPRESSION)
        dom = xml.dom.minidom.parse(tmp_path)
        os.remove(tmp_path)

        deltametadata = dom.getElementsByTagName("deltametadata")
        if not deltametadata or not deltametadata[0]:
            raise DeltaRepoError("Cannot parse {0}".fromat(path))

        usedplugins = dom.getElementsByTagName("plugin")
        for plugin_node in usedplugins:
            name = None
            version = None
            other = {}

            # Parse attributes
            for x in xrange(plugin_node.attributes.length):
                attr = plugin_node.attributes.item(x)
                if attr.name == "name":
                    name = attr.value
                elif attr.name == "version":
                    version = attr.value
                else:
                    other[attr.name] = attr.value

            if not name or not version:
                raise DeltaRepoError("Bad XML: name or version attribute "
                                     "of plugin element is missing")

            try:
                version_int = int(version)
            except ValueError:
                raise DeltaRepoError("Version {0} cannot be converted to "
                                     "integer number".format(version))

            bp = PluginBundle(name, version_int)
            bp.update(other)

            # Parse subelements
            for list_item_node in plugin_node.childNodes:
                if list_item_node.nodeType != xml.dom.minidom.Node.ELEMENT_NODE:
                    continue;

                dictionary = {}
                listname = list_item_node.nodeName
                for x in xrange(list_item_node.attributes.length):
                    attr = list_item_node.attributes.item(x)
                    dictionary[attr.name] = attr.value

                bp.append(listname, dictionary)

            self.usedplugins[bp.name] = bp

class Metadata(object):
    """Metadata file"""

    def __init__(self, metadata_type):

        self.metadata_type = metadata_type

        # Output directory
        self.out_dir = None

        # Repomd records (if available in corresponding repomd.xml)
        self.old_rec = None
        self.delta_rec = None
        self.new_rec = None

        # Paths
        # If record is available in corresponding repomd.xml
        self.old_fn = None      # in old (source) repository
        self.delta_fn = None    # in delta repository
        self.new_fn = None      # in new (target) repository

        # Exists the file on the filesystem?
        self.old_fn_exists = False
        self.delta_fn_exists = False
        self.new_fn_exists = False

        # Settings
        self.checksum_type = None
        self.compression_type = None
