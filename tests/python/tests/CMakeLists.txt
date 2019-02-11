# Detect nosetest version suffix
EXECUTE_PROCESS(COMMAND ${PYTHON_EXECUTABLE} -c "import sys; sys.stdout.write('%s.%s' % (sys.version_info[0], sys.version_info[1]))" OUTPUT_VARIABLE PYTHON_MAJOR_DOT_MINOR_VERSION)
SET(NOSETEST_VERSION_SUFFIX "-${PYTHON_MAJOR_DOT_MINOR_VERSION}")
message("-- nosetests program is nosetests${NOSETEST_VERSION_SUFFIX}")

execute_process(COMMAND nosetests${NOSETEST_VERSION_SUFFIX} --help
                OUTPUT_QUIET ERROR_QUIET
                RESULT_VARIABLE NOSE_CHECK_RESULT)
IF (NOT NOSE_CHECK_RESULT STREQUAL "0")
    MESSAGE("Command 'nosetests${NOSETEST_VERSION_SUFFIX}' doesn't exist! Using only 'nosetests' instead")
    SET(NOSETEST_VERSION_SUFFIX "")
ENDIF()

CONFIGURE_FILE("run_nosetests.sh.in"  "${CMAKE_BINARY_DIR}/tests/python/tests/run_nosetests.sh")
ADD_TEST(test_python run_nosetests.sh -s ${CMAKE_CURRENT_SOURCE_DIR})
