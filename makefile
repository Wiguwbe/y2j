LIBNAME=y2j

CC=gcc
AR=ar
LIBS=-ljansson -lyaml

# no versioning

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
