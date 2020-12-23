execute_process(COMMAND nosetests --help
                OUTPUT_QUIET ERROR_QUIET
                RESULT_VARIABLE NOSE_CHECK_RESULT)

CONFIGURE_FILE("run_nosetests.sh.in"  "${CMAKE_BINARY_DIR}/tests/python/tests/run_nosetests.sh")
ADD_TEST(test_python run_nosetests.sh -s ${CMAKE_CURRENT_SOURCE_DIR})
