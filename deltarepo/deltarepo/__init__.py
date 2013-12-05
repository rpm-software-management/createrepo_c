"""
DeltaRepo package for Python.
This is the library for generation, application and handling of
DeltaRepositories.
The library is builded on the Createrepo_c library and its a part of it.

Copyright (C) 2013   Tomas Mlcoch

"""

import createrepo_c as cr
from .common import LoggingInterface, Metadata
from .applicator import DeltaRepoApplicator
from .generator import DeltaRepoGenerator
from .delta_plugins import PLUGINS
from .errors import DeltaRepoError, DeltaRepoPluginError

__all__ = ['VERSION', 'VERBOSE_VERSION', 'DeltaRepoError',
           'DeltaRepoPluginError', 'DeltaRepoGenerator'
           'DeltaRepoApplicator']

VERSION = "0.0.1"
VERBOSE_VERSION = "%s (createrepo_c: %s)" % (VERSION, cr.VERSION)
