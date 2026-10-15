# nibble

A tiny fantasy console: 128×128 pixels, 16 colors, 6 buttons, Lua carts.
Written in C with SDL3 and Lua 5.4.

## Build & run

```sh
make            # needs sdl3 and lua5.4 (Arch: pacman -S sdl3 lua54)
make run        # runs carts/demo.nib
make test       # translator tests (needs lua5.4)
./nibble path/to/cart.nib
./nibble --check path/to/cart.nib   # report syntax errors without running
```

`--check` prints `file:line: message` and exits 1 on an error, so it can be used as a linter from another editor.

Saving the cart file reloads it automatically. Lua errors show on screen.

| Key | Action |
|---|---|
| Arrows / WASD | buttons 0–3 (left, right, up, down) |
| Z / C / N | button 4 (O) |
| X / V / M | button 5 (X) |
| Ctrl+R | reload cart |
| F11 | fullscreen |
| Esc | quit |

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
- Inside `()`, `{}` and `[]`, indentation doesn't matter.
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
| `pal(a, b)` / `pal()` | draw color `a` as `b` / reset |
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

- `src/main.c` — window, fixed 60 fps loop, input, hot reload, error screen
- `src/gfx.c` — indexed framebuffer, drawing primitives, built-in font
- `src/api.c` — Lua bindings and cart lifecycle
- `src/nib.lua` — nibble flavour translator, embedded in the binary at build time
- `tests/` — translator tests
- `carts/` — example carts

## Roadmap

1. ~~Pixels on screen~~
2. ~~Lua hookup~~
3. ~~Drawing API + font~~
4. Sprites & map: `spr`, `sspr`, `map`, `mget`, `mset`, sprite flags
5. Audio: synth + `sfx`, `music`
6. Cartridge format (code + gfx + map + sfx in one file)
7. Built-in editors (sprite, map, sfx, code)
8. Gamepads, screenshots/GIFs, web export
