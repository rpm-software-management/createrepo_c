if (NOT SKBUILD)
    FIND_PACKAGE(Python3 REQUIRED Interpreter Development)
endif (NOT SKBUILD)

INCLUDE_DIRECTORIES (${Python3_INCLUDE_DIRS})

MESSAGE(STATUS "Python install dir is ${Python3_SITELIB}")

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

ADD_LIBRARY(_createrepo_c SHARED ${createrepo_cmodule_SRCS})
SET_TARGET_PROPERTIES(_createrepo_c PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(_createrepo_c PROPERTIES LIBRARY_OUTPUT_DIRECTORY "./createrepo_c")
TARGET_LINK_LIBRARIES(_createrepo_c libcreaterepo_c)
TARGET_LINK_LIBRARIES(_createrepo_c
                        ${CURL_LIBRARY}
                     )

IF (SKBUILD)
    find_package(PythonExtensions REQUIRED)
    python_extension_module(_createrepo_c)
ELSE ()
    TARGET_LINK_LIBRARIES(_createrepo_c ${PYTHON_LIBRARY})
ENDIF ()

IF (NOT SKBUILD)
    FILE(COPY createrepo_c/__init__.py DESTINATION createrepo_c)
ENDIF()

IF (SKBUILD)
    INSTALL(FILES createrepo_c/__init__.py DESTINATION src/python/createrepo_c)
    INSTALL(TARGETS _createrepo_c LIBRARY DESTINATION src/python/createrepo_c)
ELSE ()
    INSTALL(FILES createrepo_c/__init__.py DESTINATION ${Python3_SITELIB}/createrepo_c)
    INSTALL(TARGETS _createrepo_c LIBRARY DESTINATION ${Python3_SITELIB}/createrepo_c)

    # Version has to be passed as last argument.
    INSTALL(CODE "EXECUTE_PROCESS(COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/utils/setup_for_python_metadata.py install_egg_info --install-dir \$ENV{DESTDIR}/${Python3_SITELIB} ${VERSION})")
ENDIF ()
