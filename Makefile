CC      ?= cc
LUA_PKG ?= lua5.4

CFLAGS  += -std=c11 -O2 -Wall -Wextra $(shell pkg-config --cflags sdl3 $(LUA_PKG))
LDLIBS  += $(shell pkg-config --libs sdl3 $(LUA_PKG)) -lm

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

nibble: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c $(wildcard src/*.h) | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

run: nibble
	./nibble carts/demo.lua

clean:
	rm -rf build nibble

.PHONY: run clean
