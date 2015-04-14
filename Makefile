CC = gcc
CFLAGS = $(shell pkg-config --cflags sdb) -I./lib/include -Wall -g
LIBFLAGS = -shared -Wl,-soname,libsiol.so
LOBJ = io.o map.o desc.o

lib: $(LOBJ)
	$(CC) $(LIBFLAGS) $(LOBJ) -o libsiol.so

io.o: lib/io/io.c
	$(CC) -fPIC $(CFLAGS) -c lib/io/io.c

desc.o: lib/io/desc.c
	$(CC) -fPIC $(CFLAGS) -c lib/io/desc.c

map.o: lib/io/map.c
	$(CC) -fPIC $(CFLAGS) -c lib/io/map.c
