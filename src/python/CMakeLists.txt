MACRO(PYTHON_UNSET)
    unset(PYTHON_LIBRARY)
    unset(PYTHON_INCLUDE_DIR)
    unset(PYTHON_EXECUTABLE)
    unset(PYTHON_LIBRARY CACHE)
    unset(PYTHON_INCLUDE_DIR CACHE)
    unset(PYTHON_EXECUTABLE CACHE)
ENDMACRO(PYTHON_UNSET)

if (NOT SKBUILD)
    PYTHON_UNSET()
    SET(Python_ADDITIONAL_VERSIONS 3.0 CACHE INTERNAL "")
    FIND_PACKAGE(PythonInterp 3 REQUIRED)
    FIND_PACKAGE(PythonLibs 3 REQUIRED)
endif (NOT SKBUILD)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-strict-aliasing")
set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing")

SET (createrepo_cmodule_SRCS
     checksum-py.c
     compression_wrapper-py.c
     contentstat-py.c
     createrepo_cmodule.c
     exception-py.c
     load_metadata-py.c
     locate_metadata-py.c
     misc-py.c
     package-py.c
     parsepkg-py.c
     repomd-py.c
     repomdrecord-py.c
     sqlite-py.c
     typeconversion.c
     updatecollection-py.c
     updatecollectionmodule-py.c
     updatecollectionpackage-py.c
     updateinfo-py.c
     updaterecord-py.c
     updatereference-py.c
     xml_dump-py.c
     xml_file-py.c
     xml_parser-py.c
    )

IF (SKBUILD)
    find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
    Python_add_library(_createrepo_c MODULE WITH_SOABI ${createrepo_cmodule_SRCS})
ELSE ()
    EXECUTE_PROCESS(COMMAND ${PYTHON_EXECUTABLE} -c "from sys import stdout; from sysconfig import get_path; stdout.write(get_path('platlib'))" OUTPUT_VARIABLE PYTHON_INSTALL_DIR)
    INCLUDE_DIRECTORIES (${PYTHON_INCLUDE_PATH})
    MESSAGE(STATUS "Python install dir is ${PYTHON_INSTALL_DIR}")

    ADD_LIBRARY(_createrepo_c SHARED ${createrepo_cmodule_SRCS})
    SET_TARGET_PROPERTIES(_createrepo_c PROPERTIES PREFIX "")
    TARGET_LINK_LIBRARIES(_createrepo_c PRIVATE ${PYTHON_LIBRARY})
ENDIF ()

SET_TARGET_PROPERTIES(_createrepo_c PROPERTIES LIBRARY_OUTPUT_DIRECTORY "./createrepo_c")
TARGET_LINK_LIBRARIES(_createrepo_c PRIVATE libcreaterepo_c)
TARGET_LINK_LIBRARIES(_createrepo_c PRIVATE
                        ${CURL_LIBRARY}
                     )

IF (NOT SKBUILD)
    FILE(COPY createrepo_c/__init__.py DESTINATION createrepo_c)
ENDIF()

IF (SKBUILD)
    INSTALL(FILES createrepo_c/__init__.py DESTINATION createrepo_c)
    INSTALL(TARGETS _createrepo_c LIBRARY DESTINATION createrepo_c)
ELSE ()
    INSTALL(FILES createrepo_c/__init__.py DESTINATION ${PYTHON_INSTALL_DIR}/createrepo_c)
    INSTALL(TARGETS _createrepo_c LIBRARY DESTINATION ${PYTHON_INSTALL_DIR}/createrepo_c)

    # Install dist-info metadata so distro packaging tools can discover this package
    SET(_DIST_INFO_DIR "${PYTHON_INSTALL_DIR}/createrepo_c-${VERSION}.dist-info")
    INSTALL(CODE "
        file(MAKE_DIRECTORY \$ENV{DESTDIR}/${_DIST_INFO_DIR})
        file(WRITE \$ENV{DESTDIR}/${_DIST_INFO_DIR}/METADATA \"Metadata-Version: 2.1\\nName: createrepo_c\\nVersion: ${VERSION}\\n\")
        file(WRITE \$ENV{DESTDIR}/${_DIST_INFO_DIR}/INSTALLER \"cmake\\n\")
        file(WRITE \$ENV{DESTDIR}/${_DIST_INFO_DIR}/RECORD \"\")
    ")
ENDIF ()
