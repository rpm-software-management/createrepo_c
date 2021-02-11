PROJECT (createrepo_c C)
CMAKE_MINIMUM_REQUIRED (VERSION 2.8.10)

include(GNUInstallDirs)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -ggdb -g -Wall -Wextra -Og")
IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

IF(CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    MESSAGE("Build type is set do DEBUG! (Used flags: \"${CMAKE_C_FLAGS_DEBUG}\")")
ENDIF()


option(BUILD_LIBCREATEREPO_C_SHARED "Build libcreaterepo_c as a shared library" ON)
if(NOT BUILD_LIBCREATEREPO_C_SHARED)
  set(CMAKE_POSITION_INDEPENDENT_CODE 1)
endif()

option(CREATEREPO_C_INSTALL_DEVELOPMENT "Install createrepo_c development files." ON)
option(CREATEREPO_C_INSTALL_MANPAGES "Install createrepo_c man-pages." ON)

# Add path with own cmake modules

INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

SET(G_LOG_DOMAIN "C_CREATEREPOLIB")

# Find necessary libraries

find_package(BZip2 REQUIRED)
find_package(CURL REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(GTHREAD2 REQUIRED gthread-2.0)
pkg_check_modules(LZMA REQUIRED liblzma)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(RPM REQUIRED rpm)

pkg_check_modules(LIBMAGIC libmagic)
# the pkg-config was only added in F33
IF (NOT LIBMAGIC_FOUND)
    find_path(LIBMAGIC_INCLUDE_DIRS NAMES magic.h PATHS ${CMAKE_INCLUDE_PATH})
    find_library(LIBMAGIC_LIBRARIES NAMES magic PATHS ${CMAKE_LIBRARY_PATH})
ENDIF()

# Add include dirs

include_directories(${BZIP2_INCLUDE_DIRS})
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${GLIB2_INCLUDE_DIRS})
include_directories(${LIBXML2_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${ZLIB_INCLUDE_DIR})

# SuSE/Mageia/Mandriva legacy weak deps support
OPTION (ENABLE_LEGACY_WEAKDEPS "Enable legacy SUSE/Mageia/Mandriva weakdeps support?" ON)
IF (ENABLE_LEGACY_WEAKDEPS)
    ADD_DEFINITIONS("-DENABLE_LEGACY_WEAKDEPS=1")
ENDIF (ENABLE_LEGACY_WEAKDEPS)

# drpm
OPTION (ENABLE_DRPM "Enable delta RPM support?" OFF)
IF (ENABLE_DRPM)
    pkg_check_modules(DRPM REQUIRED drpm>=0.4.0)
    include_directories (${DRPM_INCLUDE_DIRS})
    ADD_DEFINITIONS("-DCR_DELTA_RPM_SUPPORT")
ENDIF (ENABLE_DRPM)

# option to enable/disable python support
OPTION (ENABLE_PYTHON "Enable python support?" ON)

OPTION (WITH_ZCHUNK "Build with zchunk support" ON)
IF (WITH_ZCHUNK)
    pkg_check_modules(ZCK REQUIRED zck)
    include_directories(${ZCK_INCLUDE_DIRS})
    SET (CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -DWITH_ZCHUNK")
    SET (CMAKE_C_FLAGS_DEBUG    "${CMAKE_C_FLAGS_DEBUG} -DWITH_ZCHUNK")
ENDIF (WITH_ZCHUNK)

OPTION (WITH_LIBMODULEMD "Build with libmodulemd support" ON)
IF (WITH_LIBMODULEMD)
    pkg_check_modules(LIBMODULEMD REQUIRED modulemd-2.0)
    include_directories(${LIBMODULEMD_INCLUDE_DIRS})
    SET (CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -DWITH_LIBMODULEMD")
    SET (CMAKE_C_FLAGS_DEBUG    "${CMAKE_C_FLAGS_DEBUG} -DWITH_LIBMODULEMD")
ENDIF (WITH_LIBMODULEMD)

# Threaded XZ Compression
# Note: This option is disabled by default, because Createrepo_c
# parallelize a lot of tasks (including compression) by default, this
# only adds extra threads on XZ library level which causes thread bloat
# and for most usecases doesn't bring any performance boost.
# On regular hardware (e.g. less-or-equal 4 cores) this option may even
# cause degradation of performance.
OPTION(ENABLE_THREADED_XZ_ENCODER "Enable threaded XZ encoder?" OFF)
IF (ENABLE_THREADED_XZ_ENCODER)
    ADD_DEFINITIONS("-DENABLE_THREADED_XZ_ENCODER=1")
ENDIF (ENABLE_THREADED_XZ_ENCODER)

# Get package version
INCLUDE (${CMAKE_SOURCE_DIR}/VERSION.cmake)
SET (VERSION "${CR_MAJOR}.${CR_MINOR}.${CR_PATCH}")
message("Package version: ${VERSION}")

# Other files

OPTION(ENABLE_BASHCOMP "Install Bash autocompletions?" ON)
IF (ENABLE_BASHCOMP)
    pkg_check_modules(BASHCOMP bash-completion)
    IF (BASHCOMP_FOUND)
        execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=completionsdir bash-completion OUTPUT_VARIABLE BASHCOMP_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
        message("Bash completion directory: ${BASHCOMP_DIR}")
        INSTALL(FILES createrepo_c.bash DESTINATION ${BASHCOMP_DIR} RENAME createrepo_c)
        INSTALL(CODE "
            execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink createrepo_c \$ENV{DESTDIR}${BASHCOMP_DIR}/mergerepo_c)
            execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink createrepo_c \$ENV{DESTDIR}${BASHCOMP_DIR}/modifyrepo_c)
            execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink createrepo_c \$ENV{DESTDIR}${BASHCOMP_DIR}/sqliterepo_c)
            ")
    ELSEIF (BASHCOMP_FOUND)
        INSTALL(FILES createrepo_c.bash DESTINATION "/etc/bash_completion.d")
        message("Bash completion directory: /etc/bash_completion.d")
    ENDIF (BASHCOMP_FOUND)
ENDIF (ENABLE_BASHCOMP)


# Gen manpage

#message("Gen manpage")
#execute_process(COMMAND bash gen_manpage.sh src/cmd_parser.c ../doc/ WORKING_DIRECTORY utils/)

# Add custom target for tests

ADD_CUSTOM_TARGET(tests)


# Subdirs

ADD_SUBDIRECTORY (src)
ADD_SUBDIRECTORY (doc)
ENABLE_TESTING()
ADD_SUBDIRECTORY (tests EXCLUDE_FROM_ALL)

