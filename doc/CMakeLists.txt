ADD_SUBDIRECTORY (python)

ADD_CUSTOM_TARGET (doc)
ADD_DEPENDENCIES (doc doc-python)

option(BUILD_DOC_C "Build C API documentation" ON)
if(BUILD_DOC_C)
    find_package(Doxygen REQUIRED)
    CONFIGURE_FILE("Doxyfile.in.in" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.in" @ONLY)
    add_custom_target(doc-c
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.in
        COMMENT "Building C API documentation with Doxygen" VERBATIM)
    ADD_DEPENDENCIES (doc doc-c)
endif(BUILD_DOC_C)

IF(CREATEREPO_C_INSTALL_MANPAGES)
    INSTALL(FILES createrepo_c.8 mergerepo_c.8 modifyrepo_c.8 sqliterepo_c.8
            DESTINATION "${CMAKE_INSTALL_MANDIR}/man8"
            COMPONENT bin)
ENDIF(CREATEREPO_C_INSTALL_MANPAGES)
