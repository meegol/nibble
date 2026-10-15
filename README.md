# nibble

A tiny fantasy console: 128×128 pixels, 16 colors, 6 buttons, Lua carts.
Written in C with SDL3 and Lua 5.4.

## Build & run

```sh
make            # needs sdl3 and lua5.4 (Arch: pacman -S sdl3 lua54)
make run        # runs carts/demo.lua
./nibble path/to/cart.lua
```

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

A cart is a Lua file that defines any of these functions:

```lua
function _init()   end  -- once at load
function _update() end  -- 60 times per second
function _draw()   end  -- after updates, once per rendered frame
```

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
