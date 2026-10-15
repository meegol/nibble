CC      ?= cc
LUA_PKG ?= lua5.4

CFLAGS  += -std=c11 -O2 -Wall -Wextra -Ibuild $(shell pkg-config --cflags sdl3 $(LUA_PKG))
LDLIBS  += $(shell pkg-config --libs sdl3 $(LUA_PKG)) -lm

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

nibble: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c $(wildcard src/*.h) | build
	$(CC) $(CFLAGS) -c -o $@ $<

build/api.o: build/nib_lua.h

# Embeds src/nib.lua as a C string.
build/nib_lua.h: src/nib.lua | build
	{ echo 'static const char NIB_LUA[] ='; \
	  sed 's/\\/\\\\/g; s/"/\\"/g; s/^/"/; s/$$/\\n"/' $<; \
	  echo ';'; } > $@

build:
	mkdir -p build

run: nibble
	./nibble carts/demo.nib

test: build/cart_test build/gfx_test
	lua5.4 tests/nib_test.lua
	build/cart_test
	build/gfx_test

build/cart_test: tests/cart_test.c build/cart.o build/gfx.o
	$(CC) $(CFLAGS) -Isrc -o $@ $^ $(LDLIBS)

build/gfx_test: tests/gfx_test.c build/gfx.o
	$(CC) $(CFLAGS) -Isrc -o $@ $^ $(LDLIBS)

clean:
	rm -rf build nibble

.PHONY: run test clean
