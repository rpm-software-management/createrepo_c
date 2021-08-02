ADD_EXECUTABLE(test_checksum test_checksum.c)
TARGET_LINK_LIBRARIES(test_checksum libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_checksum)

ADD_EXECUTABLE(test_compression_wrapper test_compression_wrapper.c)
TARGET_LINK_LIBRARIES(test_compression_wrapper libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_compression_wrapper)

ADD_EXECUTABLE(test_load_metadata test_load_metadata.c)
TARGET_LINK_LIBRARIES(test_load_metadata libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_load_metadata)

ADD_EXECUTABLE(test_locate_metadata test_locate_metadata.c)
TARGET_LINK_LIBRARIES(test_locate_metadata libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_locate_metadata)

ADD_EXECUTABLE(test_misc test_misc.c)
TARGET_LINK_LIBRARIES(test_misc libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_misc)

ADD_EXECUTABLE(test_sqlite test_sqlite.c)
TARGET_LINK_LIBRARIES(test_sqlite libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_sqlite)

ADD_EXECUTABLE(test_xml_file test_xml_file.c)
TARGET_LINK_LIBRARIES(test_xml_file libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_xml_file)

ADD_EXECUTABLE(test_xml_parser_filelists test_xml_parser_filelists.c)
TARGET_LINK_LIBRARIES(test_xml_parser_filelists libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_xml_parser_filelists)

ADD_EXECUTABLE(test_xml_parser_repomd test_xml_parser_repomd.c)
TARGET_LINK_LIBRARIES(test_xml_parser_repomd libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_xml_parser_repomd)

ADD_EXECUTABLE(test_xml_parser_updateinfo test_xml_parser_updateinfo.c)
TARGET_LINK_LIBRARIES(test_xml_parser_updateinfo libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_xml_parser_updateinfo)

ADD_EXECUTABLE(test_xml_parser_main_metadata_together test_xml_parser_main_metadata_together.c)
TARGET_LINK_LIBRARIES(test_xml_parser_main_metadata_together libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_xml_parser_main_metadata_together)

ADD_EXECUTABLE(test_xml_dump test_xml_dump.c)
TARGET_LINK_LIBRARIES(test_xml_dump libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_xml_dump)

ADD_EXECUTABLE(test_xml_dump_primary test_xml_dump_primary.c)
TARGET_LINK_LIBRARIES(test_xml_dump_primary libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_xml_dump_primary)

ADD_EXECUTABLE(test_koji test_koji.c)
TARGET_LINK_LIBRARIES(test_koji libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_koji)

ADD_EXECUTABLE(test_modifyrepo_shared test_modifyrepo_shared.c)
TARGET_LINK_LIBRARIES(test_modifyrepo_shared libcreaterepo_c ${GLIB2_LIBRARIES})
ADD_DEPENDENCIES(tests test_modifyrepo_shared)

CONFIGURE_FILE("run_tests.sh.in"  "${CMAKE_BINARY_DIR}/tests/run_tests.sh")
ADD_TEST(test_main run_tests.sh)

IF (ENABLE_PYTHON)
ADD_SUBDIRECTORY(python)
ENDIF (ENABLE_PYTHON)
