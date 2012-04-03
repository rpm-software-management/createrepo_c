#
# Find libmagic.so (part of the 'file' package)
# (C) 2009 by Lorenzo Villani. Licensed under LGPL license.
#

include(LibFindMacros)


# Include dir
find_path(Libmagic_INCLUDE_DIR
  NAMES magic.h
  PATHS ${CMAKE_INCLUDE_PATH}
)

# Finally the library itself
find_library(Libmagic_LIBRARY
  NAMES magic
  PATHS ${CMAKE_LIBRARY_PATH}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(Libmagic_PROCESS_INCLUDES Libmagic_INCLUDE_DIR Libmagic_INCLUDE_DIRS)
set(Libmagic_PROCESS_LIBS Libmagic_LIBRARY Libmagic_LIBRARIES)
libfind_process(Libmagic)
