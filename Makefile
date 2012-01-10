SWIG=/usr/bin/swig

CFLAGS=-DDEBUG -I/usr/include/python2.7/ `pkg-config --cflags glib-2.0` `xml2-config --cflags`
LINKFLAGS=`pkg-config --libs glib-2.0` `xml2-config --libs` -lrpm

all: package.so xml_dump.so parsehdr.so

test: main

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
	ld $(LINKFLAGS) -shared package.o package_wrap.o -o _package.so

xml_dump.so: package.o xml_dump_wrap.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o
	ld $(LINKFLAGS) -shared package.o xml_dump_wrap.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o misc.o -o _xml_dump.so

parsehdr.so: parsehdr_wrap.o parsehdr.o package.o xml_dump.o misc.o
	ld $(LINKFLAGS) -shared misc.o parsehdr_wrap.o parsehdr.o package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o -o _parsehdr.so

# Main

main: package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o
	gcc $(LINKFLAGS) $(CFLAGS) package.o xml_dump.o xml_dump_primary.o xml_dump_filelists.o xml_dump_other.o main.c -o main

clean:
	rm -f *.o *.so package_wrap.* main
