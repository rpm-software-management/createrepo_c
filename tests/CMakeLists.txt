ADD_EXECUTABLE(test_compression_wrapper test_compression_wrapper.c)
TARGET_LINK_LIBRARIES(test_compression_wrapper libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_compression_wrapper)

ADD_EXECUTABLE(test_load_metadata test_load_metadata.c)
TARGET_LINK_LIBRARIES(test_load_metadata libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_load_metadata)

ADD_EXECUTABLE(test_misc test_misc.c)
TARGET_LINK_LIBRARIES(test_misc libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_misc)

ADD_EXECUTABLE(test_sqlite test_sqlite.c)
TARGET_LINK_LIBRARIES(test_sqlite libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_sqlite)

#ADD_TEST(test_main gtester "${CMAKE_CURRENT_SOURCE_DIR}/test_data")
CONFIGURE_FILE("run_gtester.sh.in"  "run_gtester.sh")
ADD_TEST(test_main run_gtester.sh)

ADD_SUBDIRECTORY(python)
