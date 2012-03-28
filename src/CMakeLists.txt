SET (createrepo_c_SRCS
    compression_wrapper.c load_metadata.c misc.c package.c parsehdr.c parsepkg.c
    repomd.c xml_dump.c xml_dump_filelists.c xml_dump_other.c xml_dump_primary.c
    locate_metadata.c)

#ADD_LIBRARY(libcreaterepo_c SHARED ${createrepo_c_SRCS})
ADD_LIBRARY(libcreaterepo_c ${createrepo_c_SRCS})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${ZLIB_LIBRARY})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${RPMDB_LIBRARY})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${BZIP2_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${GLIB2_LIBRARIES})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${LIBMAGIC_LDFLAGS})
TARGET_LINK_LIBRARIES(libcreaterepo_c ${LIBXML2_LDFLAGS})
TARGET_LINK_LIBRARIES(libcreaterepo_c "-lexpat")
SET_TARGET_PROPERTIES(libcreaterepo_c PROPERTIES OUTPUT_NAME "libcreaterepo_c")

ADD_EXECUTABLE(../createrepo_c createrepo_c.c cmd_parser.c)
TARGET_LINK_LIBRARIES(../createrepo_c libcreaterepo_c ${GLIB2_LIBRARIES} ${GTHREAD2_LIBRARIES})

INSTALL_PROGRAMS(/usr/bin/ FILES createrepo_c)
