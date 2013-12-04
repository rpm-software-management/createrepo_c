ADD_SUBDIRECTORY (python)

find_package(Doxygen)
if(DOXYGEN_FOUND)
    CONFIGURE_FILE("Doxyfile.in.in" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.in" @ONLY)
    add_custom_target(doc-c
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.in
        COMMENT "Building C API documentation with Doxygen" VERBATIM)
endif(DOXYGEN_FOUND)

INSTALL(FILES createrepo_c.8.gz mergerepo_c.8.gz modifyrepo_c.8.gz
        DESTINATION share/man/man8
        COMPONENT bin)

ADD_CUSTOM_TARGET (doc)
ADD_DEPENDENCIES (doc doc-python doc-c)
