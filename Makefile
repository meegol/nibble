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

test: build/cart_test build/gfx_test build/shot
	lua5.4 tests/nib_test.lua
	build/cart_test
	build/gfx_test
	tests/editor_test.sh

build/cart_test: tests/cart_test.c build/cart.o build/gfx.o
	$(CC) $(CFLAGS) -Isrc -o $@ $^ $(LDLIBS)

# Headless editor screenshots: build/shot cart out.png [steps...]
EDITOR_OBJ := build/editor.o build/code_edit.o build/sprite_edit.o build/help.o build/cart.o build/gfx.o build/api.o
build/shot: tests/shot.c $(EDITOR_OBJ)
	$(CC) $(CFLAGS) -Isrc -o $@ $^ $(LDLIBS)

build/gfx_test: tests/gfx_test.c build/gfx.o
	$(CC) $(CFLAGS) -Isrc -o $@ $^ $(LDLIBS)

clean:
	rm -rf build nibble

.PHONY: run test clean
