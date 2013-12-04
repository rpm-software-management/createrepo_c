EXECUTE_PROCESS(COMMAND ${PYTHON_EXECUTABLE} -c "from sys import stdout; from distutils import sysconfig; stdout.write(sysconfig.get_python_lib(True))" OUTPUT_VARIABLE PYTHON_INSTALL_DIR)

IF (${CMAKE_VERSION} VERSION_LESS "2.8.0")
    CONFIGURE_FILE( "deltarepo/deltarepo.py" COPYONLY)
ELSE()
    FILE(COPY deltarepo.py DESTINATION ./)
ENDIF()

INSTALL(PROGRAMS deltarepo.py DESTINATION bin/ RENAME deltarepo)
INSTALL(DIRECTORY deltarepo/
        DESTINATION ${PYTHON_INSTALL_DIR}/deltarepo
        FILES_MATCHING PATTERN "*.py")
