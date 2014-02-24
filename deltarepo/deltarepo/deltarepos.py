"""
Object representation deltarepos.xml
"""

import os
import tempfile
import createrepo_c as cr
import xml.dom.minidom
from lxml import etree
from .errors import DeltaRepoError
from .xmlcommon import getNode, getRequiredNode
from .xmlcommon import getAttribute, getRequiredAttribute, getNumAttribute
from .xmlcommon import getValue

class DeltaReposRecord(object):
    def __init__(self):
        self.location_base = None
        self.location_href = None
        self.size_total = None
        self.revision_src = None
        self.revision_dst = None
        self.contenthash_src = None
        self.contenthash_dst = None
        self.contenthash_type = None
        self.timestamp_src = None
        self.timestamp_dst = None

        self.data = {}  # { "primary": {"size": 123}, ... }

        self.repomd_timestamp = None
        self.repomd_size = None
        self.repomd_checksums = []  # [('type', 'value'), ...]

        #self.plugins = {}

    #def add_plugin(self, name, attrs=None):
    #    attrs = attrs or {}
    #    attrs['name'] = name
    #    for key, val in attrs.items():
    #        if not isinstance(key, basestring) or not isinstance(val, basestring):
    #            raise TypeError("Strings expected, got ({0}, {1})".format(key, val))
    #    self.plugins[name] = attrs

    def get_data(self, type):
        return self.data.get(type, None)

    def set_data(self, type, size):
        self.data[type] = {"size": int(size)}

    def _subelement(self, parent):
        """Generate <deltarepo> element"""

        attrs = {}

        deltarepo_el = etree.SubElement(parent, "deltarepo", attrs)

        # <location>
        if self.location_href:
            attrs = { "href": self.location_href }
            if self.location_base:
                attrs["base"] = self.location_base
            etree.SubElement(deltarepo_el, "location", attrs)

        # <size>
        if self.size_total:
            attrs = { "total": unicode(self.size_total) }
            etree.SubElement(deltarepo_el, "size", attrs)

        # <revision>
        if self.revision_src and self.revision_dst:
            attrs = { "src": self.revision_src, "dst": self.revision_dst }
            etree.SubElement(deltarepo_el, "revision", attrs)

        # <contenthash>
        if self.contenthash_src and self.contenthash_dst and self.contenthash_type:
            attrs = { "src": unicode(self.contenthash_src),
                      "dst": unicode(self.contenthash_dst),
                      "type": unicode(self.contenthash_type)}
            etree.SubElement(deltarepo_el, "contenthash", attrs)

        # <timestamp>
        if self.timestamp_src and self.timestamp_dst:
            attrs = { "src": unicode(self.timestamp_src),
                      "dst": unicode(self.timestamp_dst) }
            etree.SubElement(deltarepo_el, "timestamp", attrs)

        # <data>
        metadata_types = sorted(self.data.keys())
        for mtype in metadata_types:
            attrs = { "type": unicode(mtype),
                      "size": unicode(self.get_data(mtype).get("size", 0)) }
            etree.SubElement(deltarepo_el, "data", attrs)

        # <repomd>
        repomd_el = etree.SubElement(deltarepo_el, "repomd", {})

        # <repomd> <timestamp>
        if self.repomd_timestamp:
            time_el = etree.SubElement(repomd_el, "timestamp", {})
            time_el.text = str(self.repomd_timestamp)

        # <repomd> <size>
        if self.repomd_size:
            size_el = etree.SubElement(repomd_el, "size", {})
            size_el.text = str(self.repomd_size)

        # <repomd> <checksum>
        for type, value in self.repomd_checksums:
            checksum_el = etree.SubElement(repomd_el, "checksum", {"type": type})
            checksum_el.text = str(value)

        # <repomd> <plugins> elements
        #for plugin_attrs in self.plugins.values():
        #    etree.SubElement(deltarepo_el, "plugin", plugin_attrs)

        return deltarepo_el

class DeltaRepos(object):
    """Object representation of deltarepos.xml"""

    def __init__(self):
        self.records = []

    def add_record(self, rec):
        if not isinstance(rec, DeltaReposRecord):
            raise TypeError("DeltaReposRecord object expected")
        self.records.append(rec)

    def xmlparse(self, path):
        """Parse data from an deltarepos.xml file"""

        _, tmp_path = tempfile.mkstemp()
        cr.decompress_file(path, tmp_path, cr.AUTO_DETECT_COMPRESSION)
        dom = xml.dom.minidom.parse(tmp_path)
        os.remove(tmp_path)

        # <deltarepo> element
        elist_deltarepos = dom.getElementsByTagName("deltarepos")
        if not elist_deltarepos or not elist_deltarepos[0]:
            raise DeltaRepoError("Cannot parse {0}: No <deltarepos> element"
                                 "".format(path))

        # <deltarepo> elements
        for node in elist_deltarepos[0].childNodes:
            if node.nodeName != "deltarepo":
                continue

            # <deltarepo> element
            rec = DeltaReposRecord()

            subnode = getRequiredNode(node, "location")
            rec.location_base = getAttribute(subnode, "base")
            rec.location_href = getRequiredAttribute(subnode, "href")

            subnode = getNode(node, "size")
            if subnode:
                rec.size_total = getNumAttribute(subnode, "total")

            subnode = getNode(node, "revision")
            if subnode:
                rec.revision_src = getAttribute(subnode, "src")
                rec.revision_dst = getAttribute(subnode, "dst")

            subnode = getNode(node, "contenthash")
            if subnode:
                rec.contenthash_src = getAttribute(subnode, "src")
                rec.contenthash_dst = getAttribute(subnode, "dst")
                rec.contenthash_type = getAttribute(subnode, "type")

            subnode = getNode(node, "timestamp")
            if subnode:
                rec.timestamp_src = getNumAttribute(subnode, "src")
                rec.timestamp_dst = getNumAttribute(subnode, "dst")

            subnodes = node.getElementsByTagName("data") or []
            for subnode in subnodes:
                type = getAttribute(subnode, "type")
                size= getNumAttribute(subnode, "size")
                rec.set_data(type, size)

            # <repomd>
            repomdnode = getNode(node, "repomd")
            if repomdnode:
                subnode = getNode(repomdnode, "timestamp")
                if subnode and getValue(subnode):
                    rec.repomd_timestamp = int(getValue(subnode))

                subnode = getNode(repomdnode, "size")
                if subnode and getValue(subnode):
                    rec.repomd_size = int(getValue(subnode))

                checksumnodes = repomdnode.getElementsByTagName("checksum")
                if checksumnodes:
                    for subnode in checksumnodes:
                        type = getAttribute(subnode, "type")
                        val = getValue(subnode)
                        if type and val:
                            rec.repomd_checksums.append((type, val))

            # <plugin> elements
            #subnodes = node.getElementsByTagName("plugin") or []
            #for subnode in subnodes:
            #    attrs = {}
            #    name = getRequiredAttribute(subnode, "name")
            #    for i in xrange(subnode.attributes.length):
            #        attr = subnode.attributes.item(i)
            #        attrs[attr.name] = attr.value
            #    rec.add_plugin(name, attrs)

            self.records.append(rec)

    def xmldump(self):
        """Generate XML"""
        xmltree = etree.Element("deltarepos")
        for rec in self.records:
            rec._subelement(xmltree)
        return etree.tostring(xmltree, pretty_print=True,
                              encoding="UTF-8", xml_declaration=True)
