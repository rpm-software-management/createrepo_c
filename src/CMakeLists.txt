SET (createrepo_c_SRCS
     compression_wrapper.c
     load_metadata.c
     locate_metadata.c
     misc.c
     package.c
     parsehdr.c
     parsepkg.c
     repomd.c
     sqlite.c
     xml_dump.c
     xml_dump_filelists.c
     xml_dump_other.c
     xml_dump_primary.c)

SET(headers
    compression_wrapper.h
    constants.h
    createrepo_c.h
    load_metadata.h
    locate_metadata.h
    misc.h
    package.h
    parsehdr.h
    parsepkg.h
    repomd.h
    sqlite.h
    version.h
    xml_dump.h)

ADD_LIBRARY(libcreaterepo_c SHARED ${createrepo_c_SRCS})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${BZIP2_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${CURL_LIBRARY})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${EXPAT_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${GLIB2_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${Libmagic_LIBRARY})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${LIBXML2_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${LZMA_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${RPMDB_LIBRARY})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${SQLITE3_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${ZLIB_LIBRARY})


SET_TARGET_PROPERTIES(libcreaterepo_c PROPERTIES
                      OUTPUT_NAME "createrepo_c"
                      SOVERSION ${CR_MAJOR}
                      VERSION "${VERSION}")

ADD_EXECUTABLE(createrepo_c createrepo_c.c cmd_parser.c)
TARGET_LINK_LIBRARIES(createrepo_c
                        libcreaterepo_c
                        ${GLIB2_LIBRARIES}
                        ${GTHREAD2_LIBRARIES})

ADD_EXECUTABLE(mergerepo_c mergerepo_c.c)
TARGET_LINK_LIBRARIES(mergerepo_c
                        libcreaterepo_c
                        ${GLIB2_LIBRARIES}
                        ${GTHREAD2_LIBRARIES})

CONFIGURE_FILE("createrepo_c.pc.cmake" "${CMAKE_SOURCE_DIR}/src/createrepo_c.pc" @ONLY)
CONFIGURE_FILE("version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/version.h" @ONLY)

IF (CMAKE_SIZEOF_VOID_P MATCHES "8")
    SET (LIB_SUFFIX "64")
ENDIF (CMAKE_SIZEOF_VOID_P MATCHES "8")
SET (LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}")

INSTALL(FILES ${headers} DESTINATION "include/createrepo_c")
INSTALL(FILES "createrepo_c.pc" DESTINATION "${LIB_INSTALL_DIR}/pkgconfig")
INSTALL(TARGETS libcreaterepo_c LIBRARY DESTINATION ${LIB_INSTALL_DIR})
INSTALL(TARGETS createrepo_c DESTINATION bin/)
INSTALL(TARGETS mergerepo_c DESTINATION bin/)

ADD_SUBDIRECTORY(python)
