SET (createrepo_c_SRCS
     checksum.c
     compression_wrapper.c
     createrepo_shared.c
     deltarpms.c
     dumper_thread.c
     error.c
     helpers.c
     load_metadata.c
     locate_metadata.c
     misc.c
     modifyrepo_shared.c
     package.c
     parsehdr.c
     parsepkg.c
     repomd.c
     sqlite.c
     threads.c
     updateinfo.c
     xml_dump.c
     xml_dump_deltapackage.c
     xml_dump_filelists.c
     xml_dump_other.c
     xml_dump_primary.c
     xml_dump_repomd.c
     xml_dump_updateinfo.c
     xml_file.c
     xml_parser.c
     xml_parser_filelists.c
     xml_parser_other.c
     xml_parser_primary.c
     xml_parser_repomd.c
     xml_parser_updateinfo.c)

SET(headers
    checksum.h
    compression_wrapper.h
    constants.h
    createrepo_c.h
    deltarpms.h
    error.h
    helpers.h
    load_metadata.h
    locate_metadata.h
    misc.h
    modifyrepo_shared.h
    package.h
    parsehdr.h
    parsepkg.h
    repomd.h
    sqlite.h
    threads.h
    updateinfo.h
    version.h
    xml_dump.h
    xml_file.h
    xml_parser.h)

IF (BUILD_LIBCREATEREPO_C_SHARED)
  SET (createrepo_c_library_type SHARED)
ELSE ()
  SET (createrepo_c_library_type STATIC)
ENDIF ()
ADD_LIBRARY(libcreaterepo_c ${createrepo_c_library_type} ${createrepo_c_SRCS})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${BZIP2_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${CURL_LIBRARY})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${EXPAT_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${GLIB2_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${Libmagic_LIBRARY})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${LIBXML2_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${LZMA_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${OPENSSL_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${RPMDB_LIBRARY})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${SQLITE3_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${ZLIB_LIBRARY})
IF (DRPM_LIBRARY)
    TARGET_LINK_LIBRARIES(libcreaterepo_c ${DRPM_LIBRARY})
ENDIF (DRPM_LIBRARY)


SET_TARGET_PROPERTIES(libcreaterepo_c PROPERTIES
                      OUTPUT_NAME "createrepo_c"
                      SOVERSION ${CR_MAJOR}
                      VERSION "${VERSION}"
                      COMPILE_DEFINITIONS "G_LOG_DOMAIN=\"${G_LOG_DOMAIN}\"")

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

ADD_EXECUTABLE(modifyrepo_c modifyrepo_c.c)
TARGET_LINK_LIBRARIES(modifyrepo_c
                        libcreaterepo_c
                        ${GLIB2_LIBRARIES}
                        ${GTHREAD2_LIBRARIES})

ADD_EXECUTABLE(sqliterepo_c sqliterepo_c.c)
TARGET_LINK_LIBRARIES(sqliterepo_c
                        libcreaterepo_c
                        ${GLIB2_LIBRARIES}
                        ${GTHREAD2_LIBRARIES})

CONFIGURE_FILE("createrepo_c.pc.cmake" "${CMAKE_SOURCE_DIR}/src/createrepo_c.pc" @ONLY)
CONFIGURE_FILE("version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/version.h" @ONLY)
CONFIGURE_FILE("deltarpms.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/deltarpms.h" @ONLY)

IF (CMAKE_SIZEOF_VOID_P MATCHES "8")
    SET (LIB_SUFFIX "64")
ENDIF (CMAKE_SIZEOF_VOID_P MATCHES "8")

SET (LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}")
IF (CREATEREPO_C_INSTALL_DEVELOPMENT OR "${createrepo_c_library_type}" STREQUAL "SHARED")
    INSTALL(
        TARGETS libcreaterepo_c
        RUNTIME DESTINATION ${LIB_INSTALL_DIR} COMPONENT RuntimeLibraries
        LIBRARY DESTINATION ${LIB_INSTALL_DIR} COMPONENT RuntimeLibraries
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR} COMPONENT Development
        )
ENDIF (CREATEREPO_C_INSTALL_DEVELOPMENT OR "${createrepo_c_library_type}" STREQUAL "SHARED")

IF (CREATEREPO_C_INSTALL_DEVELOPMENT)
    INSTALL(FILES ${headers} DESTINATION "include/createrepo_c")
    INSTALL(FILES "createrepo_c.pc" DESTINATION "${LIB_INSTALL_DIR}/pkgconfig")
ENDIF (CREATEREPO_C_INSTALL_DEVELOPMENT)

INSTALL(TARGETS createrepo_c DESTINATION bin/)
INSTALL(TARGETS mergerepo_c DESTINATION bin/)
INSTALL(TARGETS modifyrepo_c DESTINATION bin/)
INSTALL(TARGETS sqliterepo_c DESTINATION bin/)

IF (ENABLE_PYTHON)
    ADD_SUBDIRECTORY(python)
ENDIF (ENABLE_PYTHON)
