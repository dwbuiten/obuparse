PREFIX=/usr/local

CFLAGS=-O3 -std=c99 -Wall -Wextra -g -I.

all: libobuparse.so

clean:
	@rm -fv *.so *.o
	@rm -fv tools/obudump tools/*.o

libobuparse.so: obuparse.o
	$(CC) -Wl,--version-script,obuparse.v -shared $^ -o $@

install: all
	@install -v libobuparse.so $(PREFIX)/lib

uninstall:
	@rm -fv $(PREFIX)/lib/libobuparse.so

tools: tools/obudump

tools/obudump: obuparse.o tools/obudump.o tools/json.o
	$(CC) -o tools/obudump $^ -o $@
