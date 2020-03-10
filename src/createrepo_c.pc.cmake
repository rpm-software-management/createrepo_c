prefix=@CMAKE_INSTALL_PREFIX@
libdir=@LIB_INSTALL_DIR@
includedir=@CMAKE_INSTALL_PREFIX@/include

Name: createrepo_c
Description: Library for manipulation with repodata.
Version: @VERSION@
Requires: glib-2.0 rpm libcurl sqlite3
Requires.private: zlib libxml-2.0
Libs: -L${libdir} -lcreaterepo_c
Libs.private: -lmagic -lbz2 -lzma
Cflags: -I${includedir}
