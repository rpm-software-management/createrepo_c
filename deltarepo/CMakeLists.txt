EXECUTE_PROCESS(COMMAND ${PYTHON_EXECUTABLE} -c "from sys import stdout; from distutils import sysconfig; stdout.write(sysconfig.get_python_lib(True))" OUTPUT_VARIABLE PYTHON_INSTALL_DIR)

IF (${CMAKE_VERSION} VERSION_LESS "2.8.0")
    CONFIGURE_FILE( "deltarepo/deltarepo.py" COPYONLY)
    CONFIGURE_FILE( "deltarepo/managedeltarepos.py" COPYONLY)
    CONFIGURE_FILE( "deltarepo/repoupdater.py" COPYONLY)
ELSE()
    FILE(COPY deltarepo.py DESTINATION ./)
    FILE(COPY managedeltarepos.py DESTINATION ./)
    FILE(COPY repoupdater.py DESTINATION ./)
ENDIF()

#INSTALL(PROGRAMS deltarepo.py DESTINATION bin/ RENAME deltarepo)
#INSTALL(PROGRAMS managedeltarepos.py DESTINATION bin/ RENAME managedeltarepos)
#INSTALL(PROGRAMS repoupdater.py DESTINATION bin/ RENAME repoupdater)
#INSTALL(DIRECTORY deltarepo/
#        DESTINATION ${PYTHON_INSTALL_DIR}/deltarepo
#        FILES_MATCHING PATTERN "*.py")
