PREFIX ?= /usr/local

CFLAGS  := -O3 -std=c99 -Wall -Wextra -g -fPIC -I. $(CPPFLAGS)
LDFLAGS +=
CC := $(CROSS)gcc
AR := $(CROSS)ar

ifneq (,$(findstring mingw,$(CC)))
	LIBSUF=.dll
	EXESUF=.exe
	SYSTEM=MINGW
else
	LIBSUF=.so
	LDFLAGS=-Wl,--version-script,obuparse.v -Wl,-soname,libobuparse.so.1
endif

all: libobuparse$(LIBSUF) libobuparse.a

clean:
	@rm -fv *.so *.o *.a *.dll
	@rm -fv tools/obudump$(EXESUF) tools/*.o

libobuparse.a: obuparse.o
	$(AR) rcs $@ $^

libobuparse$(LIBSUF): obuparse.o
	$(CC) $(LDFLAGS) -shared $^ -o $@

install: install-shared install-static

install-header:
	@install -d $(PREFIX)
	@install -d $(PREFIX)/include
	@install -v obuparse.h $(PREFIX)/include

install-shared: libobuparse$(LIBSUF) install-header
	@install -d $(PREFIX)/lib
ifneq ($(SYSTEM),MINGW)
	@install -v libobuparse$(LIBSUF) $(PREFIX)/lib/libobuparse$(LIBSUF).1
	@rm -fv $(PREFIX)/lib/libobuparse$(LIBSUF)
	@ln -sv libobuparse$(LIBSUF).1 $(PREFIX)/lib/libobuparse$(LIBSUF)
else
	@install -d $(PREFIX)/bin
	@install -v libobuparse$(LIBSUF) $(PREFIX)/bin/libobuparse$(LIBSUF)
endif

install-static: libobuparse.a install-header
	@install -d $(PREFIX)/lib
	@install -v libobuparse.a $(PREFIX)/lib/libobuparse.a

uninstall:
	@rm -fv $(PREFIX)/include/obuparse.h
	@rm -fv $(PREFIX)/lib/libobuparse.a
ifneq ($(SYSTEM),MINGW)
	@rm -fv $(PREFIX)/lib/libobuparse$(LIBSUF).1
	@rm -fv $(PREFIX)/lib/libobuparse$(LIBSUF)
else
	@rm -fv $(PREFIX)/bin/libobuparse$(LIBSUF)
endif

tools: tools/obudump$(EXESUF)

tools/obudump$(EXESUF): obuparse.o tools/obudump.o tools/json.o
	$(CC) -o tools/obudump$(EXESUF) $^ -o $@

install-tools: tools
	@install -d $(PREFIX)/bin
	@install -v tools/obudump$(EXESUF) $(PREFIX)/bin

uninstall-tools:
	@rm -fv $(PREFIX)/bin/obudump$(EXESUF)
