#.rst:
# FindGTHREAD2
# ---------
#
# Try to locate the GThread2 library.
# If found, this will define the following variables:
#
# ``GTHREAD2_FOUND``
#     True if the GThread2 library is available
# ``GTHREAD2_INCLUDE_DIRS``
#     The GThread2 include directories
# ``GTHREAD2_LIBRARIES``
#     The GThread2 libraries for linking
# ``GTHREAD2_INCLUDE_DIR``
#     Deprecated, use ``GTHREAD2_INCLUDE_DIRS``
# ``GTHREAD2_LIBRARY``
#     Deprecated, use ``GTHREAD2_LIBRARIES``
#
# If ``GTHREAD2_FOUND`` is TRUE, it will also define the following
# imported target:
#
# ``GTHREAD2::GTHREAD2``
#     The GTHREAD2 library

#=============================================================================
# Copyright (c) 2008 Laurent Montel, <montel@kde.org>
# Copyright (c) 2020 Dmitry Mikhirev, <dmitry@mikhirev.ru>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

find_package(PkgConfig)
pkg_check_modules(PC_GTHREAD2 QUIET gthread-2.0)

find_path(GTHREAD2_INCLUDE_DIRS
          NAMES gthread.h
          HINTS ${PC_GTHREAD2_INCLUDEDIR}
          PATH_SUFFIXES glib-2.0 glib-2.0/glib)

find_library(GTHREAD2_LIBRARIES
             NAMES gthread-2.0
             HINTS ${PC_GTHREAD2_LIBDIR}
)

get_filename_component(gthread2LibDir "${GTHREAD2_LIBRARIES}" PATH)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GTHREAD2 DEFAULT_MSG GTHREAD2_LIBRARIES GTHREAD2_INCLUDE_DIRS)

if(GTHREAD2_FOUND AND NOT TARGET GTHREAD2::GTHREAD2)
  add_library(GTHREAD2::GTHREAD2 UNKNOWN IMPORTED)
  set_target_properties(GTHREAD2::GTHREAD2 PROPERTIES
                        IMPORTED_LOCATION "${GTHREAD2_LIBRARIES}"
			INTERFACE_INCLUDE_DIRECTORIES "${GTHREAD2_INCLUDE_DIRS}")
endif()

mark_as_advanced(GTHREAD2_INCLUDE_DIRS GTHREAD2_LIBRARIES)
