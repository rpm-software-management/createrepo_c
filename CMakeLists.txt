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

ADD_SUBDIRECTORY (src)
