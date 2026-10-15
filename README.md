# nibble

A tiny fantasy console: 128×128 pixels, 16 colors, 6 buttons, Lua carts.
Written in C with SDL3 and Lua 5.4.

## Build & run

```sh
make            # needs sdl3 and lua5.4 (Arch: pacman -S sdl3 lua54)
make run        # opens the cart menu
make test       # tests (needs lua5.4)
./nibble                            # cart menu
./nibble path/to/cart.nib
./nibble --check path/to/cart.nib   # report syntax errors without running
```

`--check` prints `file:line: message` and exits 1 on an error, so it can be used as a linter from another editor.

Without a file, nibble opens a menu of the carts in `carts/` (and its subfolders), with
an entry for a new cart. Ctrl+O gets back to it.

Esc switches between the game and the editor. Saving the cart file from another editor
reloads it automatically, unless there are unsaved changes in nibble. Lua errors show on
screen; Esc from there opens the code at the error line. A cart file that doesn't exist yet
opens as a new, empty cart.

| Key | Action |
|---|---|
| Arrows / WASD | buttons 0–3 (left, right, up, down) |
| Z / C / N | button 4 (O) |
| X / V / M | button 5 (X) |
| Esc | game → editor; editor → game (restarts if anything was edited) |
| Ctrl+R | run from the start |
| Ctrl+S | save |
| Ctrl+O | cart menu |
| Ctrl+Tab | next editor tab |
| F1 | help |
| F11 | fullscreen |

Closing the window with unsaved changes asks once; close again to discard them.

## Learning

`carts/learn/` has six short lessons, each a cart to read, run and change:

1. `01_lua.nib` — variables, tables, functions, `if`
2. `02_loop.nib` — `_init`, `_update`, `_draw`
3. `03_input.nib` — buttons
4. `04_sprites.nib` — `spr`, animation, flipping, stretching
5. `05_collision.nib` — lists of objects, overlap checks
6. `06_game.nib` — a small complete game with title and game over

The `?` tab (F1) is a reference for keys, the dialect, every API function and the palette.
In the code editor, the status bar shows the arguments of the API function the cursor is in.

## Code editor

The `CODE` tab. Syntax errors are checked as you type: the line turns red and the message
shows in the status bar. The font has no lowercase, so capital letters are underlined.

| Key | Action |
|---|---|
| Arrows, Home, End, PgUp, PgDn | move (Ctrl: by word / to start or end of file; Shift: select) |
| Enter | new line, keeping indentation (one more level after a `:`) |
| Tab / Shift+Tab | indent / dedent (whole lines when several are selected) |
| Ctrl+A, Ctrl+C, Ctrl+X, Ctrl+V | select all, copy, cut, paste |
| Ctrl+Z, Ctrl+Y | undo, redo |

Mouse: click to place the cursor, drag to select, wheel to scroll.

## Sprite editor

The `SPR` tab. Zoomed canvas on the left, palette, sprite number, flags and tools on the
right, and one page of the sprite sheet (64 sprites) below.

- Left click / drag on the canvas paints; right click picks the color under the mouse.
- Click a palette color to select it, a sheet sprite to edit it, a flag box to toggle it.
- `8X8` / `16` switches between editing one sprite and a 2×2 block.

| Key | Action |
|---|---|
| `[` `]` / wheel | previous / next sprite |
| 1–4 | sheet page |
| P / F | pen / fill |
| H / V | flip horizontally / vertically |
| Delete | clear |
| Ctrl+C, Ctrl+V | copy, paste |
| Ctrl+Z, Ctrl+Y | undo, redo |

## Writing a cart

A cart is a `.nib` file (nibble flavour, below) or a plain `.lua` file that defines any of these functions:

```python
def _init():      # once at load
    x = 0
def _update():    # 60 times per second
    x = (x + 1) % 128
def _draw():      # after updates, once per rendered frame
    cls()
    circfill(x, 64, 4, 9)
```

## Sprites and map

The sprite sheet is 128×128 pixels: 256 sprites of 8×8, numbered left to right, top to
bottom (sprite 1 is at x 8, y 0). Each sprite has 8 flags. The map is 128×64 cells, each
holding a sprite number. Color 0 is transparent when drawing sprites.

They're stored at the end of the cart file, after the code:

```
...code...
__gfx__
0000000000444400      one hex digit per pixel, up to 128 rows of 128
__gff__
0001                  two hex digits per sprite, 2 rows of 128 sprites
__map__
0102                  two hex digits per cell, up to 64 rows of 128
```

Missing rows and digits are 0. Changes made with `sset`, `fset` and `mset` last until the cart reloads.

## nibble flavour

`.nib` carts are Lua with Python-style blocks. Each line is rewritten in place before
Lua sees it, so error line numbers match the file.

```python
def _update():
    if btn(0): px -= 1
    elif btn(1): px += 1
    for _, b in ipairs(balls):
        b.x += b.dx
        if b.x < 1 or b.x > 126:
            b.dx = -b.dx   # bounce
```

| Write | Lua |
|---|---|
| `if c:` `elif c:` `else:` | `if c then` `elseif c then` `else` |
| `while c:` `for ...:` | `while c do` `for ... do` |
| `def f(a):` `local def f():` | `function f(a)` `local function f()` |
| less indentation | `end` |
| `if c: stmt` | `if c then stmt end` |
| `a != b` | `a ~= b` |
| `x += e` (`-=` `*=` `/=` `..=`) | `x = x + (e)` |
| `# comment` | `-- comment` |

- A block colon is a `:` at the end of the line, or followed by a space for the one-line form. `obj:method()` is unaffected.
- `#t` (length) is still length; `#` starts a comment only at the start of a line or when followed by whitespace.
- Inside `()`, `{}` and `[]`, indentation doesn't matter, so a long header can span lines
  inside brackets: `if (a and` / `    b):`.
- `;` separates statements on one line: `if hit: x -= 1; y = 0`.
- Plain Lua still works inside a `.nib` file, including `function() ... end` and `do ... end` blocks.
- Don't mix tabs and spaces for indentation.

## API

Coordinates are pixels, and fractions are floored. Wherever `col` is optional, passing it also sets the current draw color.

| Function | Description |
|---|---|
| `cls([col])` | clear the screen (default 0), reset print cursor |
| `pset(x, y, [col])` / `pget(x, y)` | set / get a pixel |
| `line(x0, y0, x1, y1, [col])` | line |
| `rect(x0, y0, x1, y1, [col])` / `rectfill(...)` | rectangle outline / filled |
| `circ(x, y, [r], [col])` / `circfill(...)` | circle outline / filled |
| `print(s, [x, y], [col])` | text (4×6 font); without x,y it prints at the cursor |
| `color(col)` | set draw color |
| `camera([x, y])` | offset all drawing |
| `pal(a, b)` / `pal()` | draw color `a` as `b` / reset (also resets `palt`) |
| `palt(col, t)` / `palt()` | sprite color `col` transparent or not / only 0 transparent |
| `spr(n, x, y, [w, h], [fx], [fy])` | draw sprite `n` (`w`×`h` sprites, optional flips) |
| `sspr(sx, sy, sw, sh, dx, dy, [dw, dh], [fx], [fy])` | draw a sheet rectangle, stretched |
| `sget(x, y)` / `sset(x, y, [col])` | get / set a sprite sheet pixel |
| `fget(n, [f])` / `fset(n, [f], v)` | sprite flags: whole byte, or flag `f` (0–7) |
| `map([cx, cy, sx, sy, cw, ch, layer])` | draw map cells at screen `sx, sy`; skips cell 0, and with `layer` sprites without those flags |
| `mget(x, y)` / `mset(x, y, n)` | get / set a map cell |
| `btn([i])` | is button i held (no arg: bitmask) |
| `btnp(i)` | was button i just pressed (repeats while held) |
| `rnd([x])` | random float in [0, x), default x = 1 |
| `flr(x)`, `mid(a, b, c)` | floor, middle of three |
| `time()` | seconds since the cart started |
| `printh(s)` | print to the terminal |

The full Lua standard library (`math`, `string`, `table`, …) is also available.

## Palette (Sweetie 16)

`0` night · `1` plum · `2` red · `3` orange · `4` yellow · `5` lime · `6` green · `7` teal ·
`8` navy · `9` blue · `10` sky · `11` cyan · `12` white · `13` silver · `14` slate · `15` charcoal

## Layout

- `src/main.c` — window, fixed 60 fps loop, run/edit modes, hot reload, error screen
- `src/editor.c` — editor shell: tabs, top and status bars, input routing
- `src/code_edit.c`, `src/sprite_edit.c`, `src/help.c` — the editor tabs
- `src/gfx.c` — indexed framebuffer, drawing primitives, built-in font
- `src/api.c` — Lua bindings and cart lifecycle
- `src/cart.c` — cart file: code plus sprite, flag and map data
- `src/nib.lua` — nibble flavour translator, embedded in the binary at build time
- `tests/` — translator, cart file and drawing tests; `shot.c` renders the editor to a PNG
  from scripted input (`make build/shot`)
- `carts/` — example carts; `carts/learn/` — lessons

## Roadmap

1. ~~Pixels on screen~~
2. ~~Lua hookup~~
3. ~~Drawing API + font~~
4. ~~Sprites & map: `spr`, `sspr`, `map`, `mget`, `mset`, sprite flags~~
5. Audio: synth + `sfx`, `music`
6. Cartridge format (code + gfx + map + sfx in one file)
7. Built-in editors: ~~code~~, ~~sprite~~, map, sfx
8. Gamepads, screenshots/GIFs, web export
