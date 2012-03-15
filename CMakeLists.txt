PROJECT (createrepo_c)
CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

# LibXml2 workaround:
execute_process(COMMAND xml2-config --cflags
    OUTPUT_VARIABLE LIBXML2_FLAGS)
string(REPLACE "\n" "" LIBXML2_FLAGS ${LIBXML2_FLAGS})
message("LIBXML2_FLAGS: ${LIBXML2_FLAGS}")

execute_process(COMMAND xml2-config --libs
    OUTPUT_VARIABLE LIBXML2_LDFLAGS)
string(REPLACE "\n" "" LIBXML2_LDFLAGS ${LIBXML2_LDFLAGS})
message("LIBXML2_LFLAGS: ${LIBXML2_LDFLAGS}")


set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wl,--as-needed ${LIBXML2_FLAGS}")
set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -g -O3")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -ggdb -O0 -Wall -Wextra")
set (CMAKE_BUILD_TYPE DEBUG)

INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(BZip2 REQUIRED)
find_package(GLIB2 REQUIRED)
find_package(GTHREAD2 REQUIRED)
find_package(ZLIB REQUIRED)
#find_package(LibXml2 REQUIRED) # Doesn't work!

include_directories(${GLIB2_INCLUDE_DIRS})

# libmagic:
FIND_LIBRARY(LIBMAGIC magic)
IF(LIBMAGIC)
    SET(LIBMAGIC_LDFLAGS "-lmagic")
ELSE()
  MESSAGE(FATAL_ERROR "
  The libmagic library is required to build squid-ecap-av. Either get it
  from here http://www.darwinsys.com/file/ or install the file development
  package (i.e 'pkcon install file-devel' on fedora)."
  )
ENDIF()

# rpm:
FIND_LIBRARY (RPMDB_LIBRARY NAMES rpmdb)
IF (NOT RPMDB_LIBRARY)
    FIND_LIBRARY (RPMDB_LIBRARY NAMES rpm)
ENDIF (NOT RPMDB_LIBRARY)

FIND_LIBRARY (RPMIO_LIBRARY NAMES rpmio)
IF (RPMIO_LIBRARY)
    SET(RPMDB_LIBRARY ${RPMIO_LIBRARY} ${RPMDB_LIBRARY})
ENDIF (RPMIO_LIBRARY)


# CPack stuff

INCLUDE(InstallRequiredSystemLibraries)

SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "C implementation of createrepo")
SET(CPACK_PACKAGE_VENDOR "Fedora Project")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
SET(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README")
SET(CPACK_PACKAGE_VERSION_MAJOR "0")
SET(CPACK_PACKAGE_VERSION_MINOR "1")
SET(CPACK_PACKAGE_VERSION_PATCH "0")
SET(CPACK_PACKAGE_INSTALL_DIRECTORY "CMake ${CMake_VERSION_MAJOR}.${CMake_VERSION_MINOR}")
SET(CPACK_STRIP_FILES "usr/bin/createrepo_c")
SET(CPACK_SOURCE_STRIP_FILES "")
SET(CPACK_PACKAGE_EXECUTABLES "createrepo_c" "createrepo_c")

SET(CPACK_GENERATOR "TGZ")

# Not useful now because cpack doesn't support generate SRPM packages
#SET(CPACK_GENERATOR "TGZ;RPM")
#SET(CPACK_RPM_PACKAGE_RELEASE "1")
#SET(CPACK_RPM_PACKAGE_LICENSE "GPLv2")
#SET(CPACK_RPM_PACKAGE_GROUP "System Environment/Base")
#SET(CPACK_RPM_PACKAGE_REQUIRES "glib2 >= 2.26.0, file-libs, zlib, bzip2, rpm-libs >= 4.8.1, libxml2")

INCLUDE(CPack)


# Other files

SET(CMAKE_INSTALL_PREFIX "/")

INSTALL_FILES(/usr/share/doc/createrepo_c-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH} FILES README COPYING COPYING.lib)


# Gen manpage

message("Gen manpage")
execute_process(COMMAND bash gen_manpage.sh src/cmd_parser.c ../doc/ WORKING_DIRECTORY utils/)


# Subdirs

ADD_SUBDIRECTORY (src)
ADD_SUBDIRECTORY (doc)
