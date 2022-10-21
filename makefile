LIBNAME=y2j

CC=gcc
AR=ar
LIBS=-ljansson -lyaml

INSTALL_PREFIX=/usr/local
INSTALL_BIN_FOLDER=$(INSTALL_PREFIX)/bin
INSTALL_LIB_FOLDER=$(INSTALL_PREFIX)/lib
INSTALL_LIB64_FOLDER=$(INSTALL_PREFIX)/lib64
INSTALL_INC_FOLDER=$(INSTALL_PREFIX)/include

# no versioning

# default target
default: liby2j.so liby2j.a

liby2j.so: y2j.so
	mv y2j.so liby2j.so

liby2j.a: y2j.a
	mv y2j.a liby2j.a

install: liby2j.so liby2j.a y2j.h
	install liby2j.so $(INSTALL_LIB64_FOLDER)
	install liby2j.a $(INSTALL_LIB64_FOLDER)
	install y2j.h $(INSTALL_INC_FOLDER)

y2j.so: y2j.pic.o
	$(CC) -o $@ -shared $< $(LIBS)

y2j.pic.o: y2j.c y2j.h
	$(CC) -c -fPIC $< -o $@

y2j.a: y2j.o
	$(AR) rcs $@ $<

y2j.o: y2j.c y2j.h
	$(CC) -o $@ -c $<

y2j-header-only.h: y2j.c
	grep -v 'y2j.h' $< > $@

clean:
	rm -f *.o *.so *.a y2j-test

test: y2j-test
	./y2j-test

y2j-test: y2j.c y2j.h
	$(CC) -o $@ $< -DY2J_TESTER $(LIBS)
