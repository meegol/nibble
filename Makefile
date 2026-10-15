CC      ?= cc
PREFIX  ?= $(HOME)/.local
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
	./nibble

test: build/cart_test build/gfx_test build/shot
	lua5.4 tests/nib_test.lua
	build/cart_test
	build/gfx_test
	tests/editor_test.sh

build/cart_test: tests/cart_test.c build/cart.o build/gfx.o
	$(CC) $(CFLAGS) -Isrc -o $@ $^ $(LDLIBS)

# Headless editor screenshots: build/shot cart out.png [steps...]
EDITOR_OBJ := build/editor.o build/code_edit.o build/sprite_edit.o build/help.o build/menu.o build/cart.o build/gfx.o build/api.o
build/shot: tests/shot.c $(EDITOR_OBJ)
	$(CC) $(CFLAGS) -Isrc -o $@ $^ $(LDLIBS)

build/gfx_test: tests/gfx_test.c build/gfx.o
	$(CC) $(CFLAGS) -Isrc -o $@ $^ $(LDLIBS)

# Installs the binary, a launcher entry with icon, and the example carts (copied to
# ~/.local/share/nibble/carts on first run). Never touches that carts folder.
install: nibble
	install -Dm755 nibble $(PREFIX)/bin/nibble
	install -Dm644 assets/nibble.png $(PREFIX)/share/icons/hicolor/256x256/apps/nibble.png
	mkdir -p $(PREFIX)/share/applications
	sed 's|@BIN@|$(PREFIX)/bin/nibble|' assets/nibble.desktop > $(PREFIX)/share/applications/nibble.desktop
	rm -rf $(PREFIX)/share/nibble/examples
	mkdir -p $(PREFIX)/share/nibble
	cp -r carts $(PREFIX)/share/nibble/examples
	update-desktop-database $(PREFIX)/share/applications 2>/dev/null || true
	gtk-update-icon-cache -q $(PREFIX)/share/icons/hicolor 2>/dev/null || true

# Leaves your carts in ~/.local/share/nibble/carts alone.
uninstall:
	rm -f $(PREFIX)/bin/nibble $(PREFIX)/share/applications/nibble.desktop
	rm -f $(PREFIX)/share/icons/hicolor/256x256/apps/nibble.png
	rm -rf $(PREFIX)/share/nibble/examples

clean:
	rm -rf build nibble

.PHONY: run test install uninstall clean
