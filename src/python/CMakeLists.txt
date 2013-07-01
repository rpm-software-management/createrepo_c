FIND_PACKAGE (PythonLibs)
FIND_PACKAGE (PythonInterp REQUIRED)
EXECUTE_PROCESS(COMMAND ${PYTHON_EXECUTABLE} -c "from sys import stdout; from distutils import sysconfig; stdout.write(sysconfig.get_python_lib(True))" OUTPUT_VARIABLE PYTHON_INSTALL_DIR)
INCLUDE_DIRECTORIES (${PYTHON_INCLUDE_PATH})

MESSAGE(STATUS "Python install dir is ${PYTHON_INSTALL_DIR}")

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-strict-aliasing")
set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing")

IF (${CMAKE_VERSION} VERSION_LESS "2.8.0")
    CONFIGURE_FILE(__init__.py ./createrepo_c/__init__.py COPYONLY)
ELSE()
    FILE(COPY __init__.py DESTINATION ./createrepo_c/)
ENDIF()

SET (craeterepo_cmodule_SRCS
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
     xml_dump-py.c
     xml_file-py.c
     xml_parser-py.c
    )

ADD_LIBRARY(_createrepo_cmodule SHARED ${craeterepo_cmodule_SRCS})
SET_TARGET_PROPERTIES(_createrepo_cmodule PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(_createrepo_cmodule PROPERTIES LIBRARY_OUTPUT_DIRECTORY "./createrepo_c")
TARGET_LINK_LIBRARIES(_createrepo_cmodule libcreaterepo_c)
TARGET_LINK_LIBRARIES(_createrepo_cmodule
                        ${EXPAT_LIBRARIES}
                        ${CURL_LIBRARY}
                        ${PYTHON_LIBRARY}
                     )

INSTALL(FILES __init__.py DESTINATION ${PYTHON_INSTALL_DIR}/createrepo_c)
INSTALL(TARGETS _createrepo_cmodule LIBRARY DESTINATION ${PYTHON_INSTALL_DIR}/createrepo_c)
