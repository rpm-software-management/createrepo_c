SWIG=/usr/bin/swig
# TODO: add -O2
CFLAGS=-fPIC -I/usr/include/python2.7/ `pkg-config --cflags glib-2.0` `xml2-config --cflags` -pthread
LINKFLAGS=-lz `pkg-config --libs glib-2.0 --libs-only-l gthread-2.0` `xml2-config --libs` -lrpm -lrpmio
LC=-lc

# Profiling
#CFLAGS=-pg -O -fPIC -DDEBUG -I/usr/include/python2.7/ `pkg-config --cflags glib-2.0` `xml2-config --cflags` -pthread
#LINKFLAGS=/lib/gcrt0.o `pkg-config --libs glib-2.0` `xml2-config --libs` -lrpm -lrpmio
LC=-lc


all: package.so xml_dump.so parsehdr.so parsepkg.so load_metadata.so main

ctests: parsepkg_test_01 parsehdr_test_01 parsehdr_test_02 xml_dump_primary_test_01 \
        xml_dump_filelists_test_01 xml_dump_other_test_01 load_metadata_test_01 \
        load_metadata_test_02 load_metadata_test_03 load_metadata_test_04 \
        load_metadata_2_test_01 load_metadata_2_test_02 load_metadata_2_test_03 load_metadata_2_test_04


# Object files + Swit object files

package.o package_wrap.o: package.i package.c package.h
	$(SWIG) -python -Wall package.i
	gcc $(CFLAGS) -c package.c package_wrap.c

#xml_dump.o xml_dump_wrap.o: xml_dump.i xml_dump.c xml_dump.h
#	$(SWIG) -python -Wall xml_dump.i
#	gcc $(CFLAGS) -c xml_dump.c xml_dump_wrap.c

xml_dump_wrap.o: xml_dump.i xml_dump.c xml_dump.h
	$(SWIG) -python -Wall xml_dump.i
	gcc $(CFLAGS) -c xml_dump.c xml_dump_wrap.c

parsehdr.o parsehdr_wrap.o: parsehdr.c parsehdr.h
	$(SWIG) -python -Wall parsehdr.i
	gcc $(CFLAGS) -c parsehdr.c parsehdr_wrap.c

parsepkg.o parsepkg_wrap.o: parsepkg.c parsepkg.h constants.h
	$(SWIG) -python -Wall parsepkg.i
	gcc $(CFLAGS) -c parsepkg.c parsepkg_wrap.c

load_metadata.o load_metadata_wrap.o: load_metadata.c load_metadata.h constants.h
	$(SWIG) -python -Wall load_metadata.i
	gcc $(CFLAGS) -c load_metadata.c load_metadata_wrap.c

# TODO
load_metadata_2.o load_metadata_2_wrap.o: load_metadata_2.c load_metadata.h constants.h
#	$(SWIG) -python -Wall load_metadata_2.i
#	gcc $(CFLAGS) -c load_metadata_2.c load_metadata_2_wrap.c
	gcc $(CFLAGS) -c load_metadata_2.c


# Object files

misc.o: misc.c misc.h
	gcc $(CFLAGS) -c misc.c

xml_dump_primary.o: xml_dump_primary.c xml_dump.h
	gcc $(CFLAGS) -c xml_dump_primary.c

xml_dump_filelists.o: xml_dump_filelists.c xml_dump.h
	gcc $(CFLAGS) -c xml_dump_filelists.c

xml_dump_other.o: xml_dump_other.c xml_dump.h
	gcc $(CFLAGS) -c xml_dump_other.c

package.so: package_wrap.o package.o
	ld $(LINKFLAGS) -shared package.o package_wrap.o -o _package.so $(LC)

xml_dump.so: package.o xml_dump_wrap.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o
	ld $(LINKFLAGS) -shared package.o xml_dump_wrap.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o -o _xml_dump.so $(LC)

parsehdr.so: parsehdr_wrap.o parsehdr.o package.o xml_dump.o misc.o
	ld $(LINKFLAGS) -shared misc.o parsehdr_wrap.o parsehdr.o package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o -o _parsehdr.so $(LC)

parsepkg.so: parsepkg_wrap.o parsepkg.o parsehdr.o package.o xml_dump.o misc.o
	ld $(LINKFLAGS) -shared misc.o parsepkg_wrap.o parsepkg.o parsehdr.o package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o -o _parsepkg.so $(LC)

load_metadata.so: load_metadata_wrap.o load_metadata.o
	ld $(LINKFLAGS) -shared load_metadata_wrap.o load_metadata.o -o _load_metadata.so $(LC)

# TODO
#load_metadata_2.so: load_metadata_wrap.o load_metadata_2.o
#	ld $(LINKFLAGS) -shared load_metadata_2_wrap.o load_metadata_2.o -o _load_metadata_2.so $(LC)


# Tests

parsepkg_test_01: parsepkg.o parsehdr.o package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o
	gcc $(LINKFLAGS) $(CFLAGS) parsepkg.o parsehdr.o package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o ctests/parsepkg_test_01.c -o ctests/parsepkg_test_01

parsehdr_test_01: parsehdr.o package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o
	gcc $(LINKFLAGS) $(CFLAGS) parsehdr.o package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o ctests/parsehdr_test_01.c -o ctests/parsehdr_test_01

parsehdr_test_02: parsehdr.o package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o
	gcc $(LINKFLAGS) $(CFLAGS) parsehdr.o package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o ctests/parsehdr_test_02.c -o ctests/parsehdr_test_02

xml_dump_primary_test_01: package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o
	gcc $(LINKFLAGS) $(CFLAGS) package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o ctests/xml_dump_primary_test_01.c -o ctests/xml_dump_primary_test_01

xml_dump_filelists_test_01: package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o
	gcc $(LINKFLAGS) $(CFLAGS) package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o ctests/xml_dump_filelists_test_01.c -o ctests/xml_dump_filelists_test_01

xml_dump_other_test_01: package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o
	gcc $(LINKFLAGS) $(CFLAGS) package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o ctests/xml_dump_other_test_01.c -o ctests/xml_dump_other_test_01


load_metadata_test_01: load_metadata.o
	gcc $(LINKFLAGS) $(CFLAGS) load_metadata.o ctests/load_metadata_test_01.c -o ctests/load_metadata_test_01

load_metadata_test_02: load_metadata.o
	gcc $(LINKFLAGS) $(CFLAGS) load_metadata.o ctests/load_metadata_test_02.c -o ctests/load_metadata_test_02

load_metadata_test_03: load_metadata.o
	gcc $(LINKFLAGS) $(CFLAGS) load_metadata.o ctests/load_metadata_test_03.c -o ctests/load_metadata_test_03

load_metadata_test_04: load_metadata.o
	gcc $(LINKFLAGS) $(CFLAGS) load_metadata.o ctests/load_metadata_test_04_big.c -o ctests/load_metadata_test_04_big


load_metadata_2_test_01: load_metadata_2.o
	gcc $(LINKFLAGS) $(CFLAGS) load_metadata_2.o ctests/load_metadata_2_test_01.c -o ctests/load_metadata_2_test_01

load_metadata_2_test_02: load_metadata_2.o
	gcc $(LINKFLAGS) $(CFLAGS) load_metadata_2.o ctests/load_metadata_2_test_02.c -o ctests/load_metadata_2_test_02

load_metadata_2_test_03: load_metadata_2.o
	gcc $(LINKFLAGS) $(CFLAGS) load_metadata_2.o ctests/load_metadata_2_test_03.c -o ctests/load_metadata_2_test_03

load_metadata_2_test_04: load_metadata_2.o
	gcc $(LINKFLAGS) $(CFLAGS) load_metadata_2.o ctests/load_metadata_2_test_04_big.c -o ctests/load_metadata_2_test_04_big


# Main

main: parsepkg.o parsehdr.o package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o load_metadata_2.o
	gcc $(LINKFLAGS) $(CFLAGS) parsepkg.o parsehdr.o package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o load_metadata_2.o main.c -o main


clean:
	rm -f *.o *.so *_wrap.* main *.pyc
