PREFIX=/usr/local

CFLAGS=-O3 -std=c99 -Wall -Wextra -g -fPIC -I.

all: libobuparse.so

clean:
	@rm -fv *.so *.o
	@rm -fv tools/obudump tools/*.o

libobuparse.so: obuparse.o
	$(CC) -Wl,--version-script,obuparse.v -shared $^ -o $@

install: all
	@install -v obuparse.h $(PREFIX)/include
	@install -v libobuparse.so $(PREFIX)/lib/libobuparse.so.1
	@ln -sv libobuparse.so.1 $(PREFIX)/lib/libobuparse.so

uninstall:
	@rm -fv $(PREFIX)/include/obuparse.h
	@rm -fv $(PREFIX)/lib/libobuparse.so.1
	@rm -fv $(PREFIX)/lib/libobuparse.so

tools: tools/obudump

tools/obudump: obuparse.o tools/obudump.o tools/json.o
	$(CC) -o tools/obudump $^ -o $@

install-tools: tools
	@install -v tools/obudump $(PREFIX)/bin

uninstall-tools:
	@rm -fv $(PREFIX)/bin/obudump
