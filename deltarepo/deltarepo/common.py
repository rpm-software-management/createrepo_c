import os
import tempfile
import logging
import xml.dom.minidom
import createrepo_c as cr
from lxml import etree
from deltarepo.errors import DeltaRepoError

class LoggingInterface(object):
    def __init__(self, logger=None):
        if logger is None:
            logger = logging.getLogger()
            logger.disabled = True
        self.logger = logger

    def _debug(self, msg):
        self.logger.debug(msg)

    def _info(self, msg):
        self.logger.info(msg)

    def _warning(self, msg):
        self.logger.warning(msg)

    def _error(self, msg):
        self.logger.error(msg)

    def _critical(self, msg):
        self.logger.critical(msg)

class AdditionalXmlData(object):
    """
    Interface to store/load additional data to/from xml.
    """

    ADDITIONAL_XML_DATA = True

    def __init__(self):
        self._data = {}
        self._lists = {}

    def set(self, key, value):
        if not isinstance(key, basestring):
            raise TypeError("expected string as key")
        if not isinstance(value, basestring):
            raise TypeError("expected string as value")
        self._data[key] = value

    def update(self, dictionary):
        if not isinstance(dictionary, dict):
            raise TypeError("expected dictionary")

        for key, val in dictionary.items():
            self.set(key, val)

    def append(self, listname, dictionary):
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
        return self._data.get(key, default)

    def get_list(self, key, default=None):
        return self._lists.get(key, default)

    def subelement(self, parent, name, in_attrs=None):
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
    def __init__(self, name, version):
        AdditionalXmlData.__init__(self)

        if not isinstance(name, basestring):
            raise TypeError("string expected")
        if not isinstance(version, int):
            raise TypeError("integer expected")

        self.name = name
        self.version = version

class DeltaMetadata(AdditionalXmlData):
    """Object that represents bundle.xml file in deltarepository.
    """

    def __init__(self):
        AdditionalXmlData.__init__(self)

        self.usedplugins = {}

    def add_pluginbundle(self, pluginbundle):
        self.usedplugins[pluginbundle.name] = pluginbundle

    def get_pluginbundle(self, name):
        return self.usedplugins.get(name, None)


    def xmldump(self):
        xmltree = etree.Element("deltametadata")

        usedplugins = etree.SubElement(xmltree, "usedplugins")
        for plugin in self.usedplugins.values():
            attrs = {"name": plugin.name, "version": str(plugin.version)}
            plugin.subelement(usedplugins, "plugin", attrs)
        return etree.tostring(xmltree,
                              pretty_print=True,
                              encoding="UTF-8",
                              xml_declaration=True)

    def xmlparse(self, path):
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

        # Paths
        self.old_fn = None
        self.delta_fn = None
        self.new_fn = None

        self.out_dir = None

        # Settings
        self.checksum_type = None
        self.compression_type = None

        # Records

        # List of new repomd records related to this delta file
        # List because some deltafiles could lead to more output files.
        # E.g.: delta of primary.xml generate new primary.xml and
        # primary.sqlite as well.
        self.generated_repomd_records = []
