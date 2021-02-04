CONFIGURE_FILE("run_unittests.sh.in"  "${CMAKE_BINARY_DIR}/tests/python/tests/run_unittests.sh")
ADD_TEST(test_python run_unittests.sh -s ${CMAKE_CURRENT_SOURCE_DIR})
