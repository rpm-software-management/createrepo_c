"""
XML helpers and baseclasses
"""

from .errors import DeltaRepoError

#
# Helper function for manipulation with XML dom objects
#

def getNode(node, name):
    """Return the first node with the specified name or None"""
    subnode = node.getElementsByTagName(name)
    if not subnode or not subnode[0]:
        return None
    return subnode[0]

def getRequiredNode(node, name):
    """Return the first node with the specified name
    or raise DeltaRepoError."""
    subnode = node.getElementsByTagName(name)
    if not subnode or not subnode[0]:
        raise DeltaRepoError("Required element '{1}' in '{2}' is "
                             "missing".format(name, node.nodeName))
    return subnode[0]

def getAttribute(node, name, default=None):
    """Get node attribute or value passed in default param (None by default)"""
    return node.getAttribute(name) or default

def getNumAttribute(node, name, default=None):
    strval = node.getAttribute(name)
    if strval:
        try:
            return long(strval)
        except ValueError:
            raise DeltaRepoError("Expected integral number in attribute "
                                 "'{1}' but got '{2}'".format(name, strval))
    return default

def getRequiredAttribute(node, name):
    if not node.hasAttribute(name):
        raise DeltaRepoError("Required attribute '{1}' of '{2}' is "
                             "missing'".format(name, node.nodeName))
    return node.getAttribute(name)

def getValue(node, default=None):
    if node.firstChild:
        return node.firstChild.nodeValue
    return default
