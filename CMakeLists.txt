PROJECT (createrepo_c)
CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -Wall -Wextra")
IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

IF(CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    MESSAGE("Build type is set do DEBUG!")
ENDIF()


# Add path with own cmake modules

INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

SET(G_LOG_DOMAIN "C_CREATEREPOLIB")

# Find necessary libraries

find_package(BZip2 REQUIRED)
find_package(CURL REQUIRED)
find_package(EXPAT REQUIRED)
find_package(GLIB2 REQUIRED)
find_package(GTHREAD2 REQUIRED)
find_package(Libmagic REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(LZMA REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Sqlite3 REQUIRED)
find_package(ZLIB REQUIRED)

# Add include dirs

include_directories(${GLIB2_INCLUDE_DIRS})
include_directories(${LIBXML2_INCLUDE_DIR})


# rpm:

FIND_LIBRARY (RPMDB_LIBRARY NAMES rpmdb)
IF (NOT RPMDB_LIBRARY)
    FIND_LIBRARY (RPMDB_LIBRARY NAMES rpm)
    IF (NOT RPMDB_LIBRARY)
        MESSAGE(FATAL_ERROR "No Rpm library installed")
    ENDIF (NOT RPMDB_LIBRARY)
ENDIF (NOT RPMDB_LIBRARY)


# rpmio:

FIND_LIBRARY (RPMIO_LIBRARY NAMES rpmio)
IF (RPMIO_LIBRARY)
    SET(RPMDB_LIBRARY ${RPMIO_LIBRARY} ${RPMDB_LIBRARY})
ELSE (RPMIO_LIBRARY)
    MESSAGE(FATAL_ERROR "No Rpmio library installed")
ENDIF (RPMIO_LIBRARY)


# Get package version
INCLUDE (${CMAKE_SOURCE_DIR}/VERSION.cmake)
SET (VERSION "${CR_MAJOR}.${CR_MINOR}.${CR_PATCH}")
message("Package version: ${VERSION}")


# Other files

INSTALL(FILES createrepo_c.bash
        DESTINATION "/etc/bash_completion.d")


# Gen manpage

#message("Gen manpage")
#execute_process(COMMAND bash gen_manpage.sh src/cmd_parser.c ../doc/ WORKING_DIRECTORY utils/)

# Add custom target for tests

ADD_CUSTOM_TARGET(tests)


# Subdirs

ADD_SUBDIRECTORY (src)
ADD_SUBDIRECTORY (doc)
ADD_SUBDIRECTORY (deltarepo)
ENABLE_TESTING()
ADD_SUBDIRECTORY (tests EXCLUDE_FROM_ALL)
