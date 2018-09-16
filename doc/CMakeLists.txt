ADD_SUBDIRECTORY (python)

find_package(Doxygen)
if(DOXYGEN_FOUND)
    CONFIGURE_FILE("Doxyfile.in.in" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.in" @ONLY)
    add_custom_target(doc-c
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.in
        COMMENT "Building C API documentation with Doxygen" VERBATIM)
endif(DOXYGEN_FOUND)

IF(CREATEREPO_C_INSTALL_MANPAGES)
    INSTALL(FILES createrepo_c.8 mergerepo_c.8 modifyrepo_c.8 sqliterepo_c.8
            DESTINATION "${CMAKE_INSTALL_MANDIR}/man8"
            COMPONENT bin)
ENDIF(CREATEREPO_C_INSTALL_MANPAGES)

ADD_CUSTOM_TARGET (doc)
ADD_DEPENDENCIES (doc doc-python doc-c)
