"""
DeltaRepo package for Python.
This is the library for generation, application and handling of
DeltaRepositories.
The library is builded on the Createrepo_c library and its a part of it.

Copyright (C) 2013   Tomas Mlcoch

"""

import createrepo_c as cr
from .common import LoggingInterface, calculate_contenthash
from .plugins_common import Metadata
from .deltarepos import DeltaRepos, DeltaReposRecord
from .deltametadata import DeltaMetadata, PluginBundle
from .applicator import DeltaRepoApplicator
from .generator import DeltaRepoGenerator
from .plugins import PLUGINS
from .plugins import needed_delta_metadata
from .errors import DeltaRepoError, DeltaRepoPluginError

__all__ = ['VERSION', 'VERBOSE_VERSION',
           'LoggingInterface', 'calculate_contenthash',
           'Metadata',
           'DeltaRepos', 'DeltaReposRecord',
           'DeltaMetadata', 'PluginBundle',
           'DeltaRepoApplicator',
           'DeltaRepoGenerator',
           'needed_delta_metadata',
           'DeltaRepoError', 'DeltaRepoPluginError']

VERSION = "0.0.1"
VERBOSE_VERSION = "%s (createrepo_c: %s)" % (VERSION, cr.VERSION)
