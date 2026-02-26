CMAKE_MINIMUM_REQUIRED (VERSION 3.7)
PROJECT (createrepo_c C)

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
find_package(PkgConfig)
find_package(BZip2 REQUIRED)
find_package(CURL REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)
pkg_check_modules(GTHREAD2 REQUIRED IMPORTED_TARGET gthread-2.0)
pkg_check_modules(LZMA REQUIRED IMPORTED_TARGET liblzma)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3>=3.6.18)
pkg_check_modules(RPM REQUIRED IMPORTED_TARGET rpm)
pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)

# Add include dirs

include_directories(${BZIP2_INCLUDE_DIRS})
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${GLIB2_INCLUDE_DIRS})
include_directories(${GIO_INCLUDE_DIRS})
include_directories(${LIBXML2_INCLUDE_DIR})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${ZLIB_INCLUDE_DIR})
include_directories(${ZSTD_INCLUDE_DIRS})

# SuSE/Mageia/Mandriva legacy weak deps support
OPTION (ENABLE_LEGACY_WEAKDEPS "Enable legacy SUSE/Mageia/Mandriva weakdeps support?" ON)
IF (ENABLE_LEGACY_WEAKDEPS)
    ADD_DEFINITIONS("-DENABLE_LEGACY_WEAKDEPS=1")
ENDIF (ENABLE_LEGACY_WEAKDEPS)

# Legacy hash functions
OPTION (WITH_LEGACY_HASHES "Build with SHA-1 and MD5 support" OFF)
IF (WITH_LEGACY_HASHES)
    ADD_DEFINITIONS("-DWITH_LEGACY_HASHES=1")
ENDIF (WITH_LEGACY_HASHES)

# drpm
OPTION (ENABLE_DRPM "Enable delta RPM support?" OFF)
IF (ENABLE_DRPM)
    pkg_check_modules(DRPM REQUIRED IMPORTED_TARGET drpm>=0.4.0)
    include_directories (${DRPM_INCLUDE_DIRS})
    ADD_DEFINITIONS("-DCR_DELTA_RPM_SUPPORT")
ENDIF (ENABLE_DRPM)
IF (ENABLE_DRPM)
    SET (ENABLE_DRPM_BASH 1)
ELSE ()
    SET (ENABLE_DRPM_BASH 0)
ENDIF ()

# option to enable/disable python support
OPTION (ENABLE_PYTHON "Enable python support?" ON)

OPTION (WITH_ZCHUNK "Build with zchunk support" ON)
IF (WITH_ZCHUNK)
    pkg_check_modules(ZCK REQUIRED IMPORTED_TARGET zck)
    include_directories(${ZCK_INCLUDE_DIRS})
    SET (CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -DWITH_ZCHUNK")
    SET (CMAKE_C_FLAGS_DEBUG    "${CMAKE_C_FLAGS_DEBUG} -DWITH_ZCHUNK")
ENDIF (WITH_ZCHUNK)

OPTION (WITH_LIBMODULEMD "Build with libmodulemd support" ON)
IF (WITH_LIBMODULEMD)
    pkg_check_modules(LIBMODULEMD REQUIRED IMPORTED_TARGET modulemd-2.0)
    include_directories(${LIBMODULEMD_INCLUDE_DIRS})
    SET (CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -DWITH_LIBMODULEMD")
    SET (CMAKE_C_FLAGS_DEBUG    "${CMAKE_C_FLAGS_DEBUG} -DWITH_LIBMODULEMD")
ENDIF (WITH_LIBMODULEMD)


if (WITH_SANITIZERS)
    message(WARNING "Building with sanitizers enabled!")
    add_compile_options(-fsanitize=address -fsanitize=undefined -fsanitize=leak)
    link_libraries(asan ubsan)
endif()

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

# Default compression method
SET(DEFAULT_COMPRESSION_METHOD "zstd" CACHE STRING "Default compression method (zstd, gz, bz2, xz, zck)")
SET_PROPERTY(CACHE DEFAULT_COMPRESSION_METHOD PROPERTY STRINGS "zstd" "gz" "bz2" "xz" "zck")
STRING(TOLOWER "${DEFAULT_COMPRESSION_METHOD}" DEFAULT_COMPRESSION_METHOD_LOWER)
IF (DEFAULT_COMPRESSION_METHOD_LOWER STREQUAL "zstd")
    SET(CR_DEFAULT_COMPRESSION_VALUE "CR_CW_ZSTD_COMPRESSION")
ELSEIF (DEFAULT_COMPRESSION_METHOD_LOWER STREQUAL "gz")
    SET(CR_DEFAULT_COMPRESSION_VALUE "CR_CW_GZ_COMPRESSION")
ELSEIF (DEFAULT_COMPRESSION_METHOD_LOWER STREQUAL "bz2")
    SET(CR_DEFAULT_COMPRESSION_VALUE "CR_CW_BZ2_COMPRESSION")
ELSEIF (DEFAULT_COMPRESSION_METHOD_LOWER STREQUAL "xz")
    SET(CR_DEFAULT_COMPRESSION_VALUE "CR_CW_XZ_COMPRESSION")
ELSEIF (DEFAULT_COMPRESSION_METHOD_LOWER STREQUAL "zck")
    IF (WITH_ZCHUNK)
        SET(CR_DEFAULT_COMPRESSION_VALUE "CR_CW_ZCK_COMPRESSION")
    ELSE (WITH_ZCHUNK)
        MESSAGE(FATAL_ERROR "zck compression requires WITH_ZCHUNK to be enabled")
    ENDIF (WITH_ZCHUNK)
ELSE (DEFAULT_COMPRESSION_METHOD_LOWER STREQUAL "zstd")
    MESSAGE(FATAL_ERROR "Invalid DEFAULT_COMPRESSION_METHOD: ${DEFAULT_COMPRESSION_METHOD}. Valid options are: zstd, gz, bz2, xz, zck")
ENDIF (DEFAULT_COMPRESSION_METHOD_LOWER STREQUAL "zstd")
ADD_DEFINITIONS("-DCR_DEFAULT_COMPRESSION=${CR_DEFAULT_COMPRESSION_VALUE}")
MESSAGE(STATUS "Default compression method: ${DEFAULT_COMPRESSION_METHOD} (${CR_DEFAULT_COMPRESSION_VALUE})")

# Get package version
INCLUDE (${CMAKE_SOURCE_DIR}/VERSION.cmake)
SET (VERSION "${CR_MAJOR}.${CR_MINOR}.${CR_PATCH}")
message("Package version: ${VERSION}")

# Other files

OPTION(ENABLE_BASHCOMP "Install Bash autocompletions?" ON)
IF (ENABLE_BASHCOMP)
    CONFIGURE_FILE(createrepo_c.bash.in ${CMAKE_BINARY_DIR}/createrepo_c.bash @ONLY)
    pkg_check_modules(BASHCOMP bash-completion)
    IF (BASHCOMP_FOUND)
        execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=completionsdir bash-completion OUTPUT_VARIABLE BASHCOMP_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
        message("Bash completion directory: ${BASHCOMP_DIR}")
        INSTALL(FILES ${CMAKE_BINARY_DIR}/createrepo_c.bash DESTINATION ${BASHCOMP_DIR} RENAME createrepo_c)
        INSTALL(CODE "
            execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink createrepo_c \$ENV{DESTDIR}${BASHCOMP_DIR}/mergerepo_c)
            execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink createrepo_c \$ENV{DESTDIR}${BASHCOMP_DIR}/modifyrepo_c)
            execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink createrepo_c \$ENV{DESTDIR}${BASHCOMP_DIR}/sqliterepo_c)
            ")
    ELSEIF (BASHCOMP_FOUND)
        INSTALL(FILES ${CMAKE_BINARY_DIR}/createrepo_c.bash DESTINATION "/etc/bash_completion.d")
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
