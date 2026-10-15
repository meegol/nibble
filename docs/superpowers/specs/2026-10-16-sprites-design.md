# Sprites, map and cart data — design

Sub-project 2. Gives the sprite editor (sub-project 5) something to edit and somewhere to save it.

## Memory

| | Size | Notes |
|---|---|---|
| sprite sheet | 128×128 px, one color index (0–15) per pixel | 256 sprites of 8×8, numbered left to right, top to bottom |
| sprite flags | 256 bytes | 8 on/off flags per sprite |
| map | 128×64 cells, one byte each | each cell holds a sprite number |

Lives in `gfx.c`, exposed in `gfx.h` as `gfx_sheet`, `gfx_flags`, `gfx_map`. Reloading the
cart reloads all three from the file, so `sset`/`mset` changes made while running are not kept.

## API

| Function | Description |
|---|---|
| `spr(n, x, y, [w, h], [flip_x], [flip_y])` | draw sprite n; w,h in sprites (default 1) |
| `sspr(sx, sy, sw, sh, dx, dy, [dw, dh], [flip_x], [flip_y])` | draw a sheet rectangle, stretched to dw×dh |
| `sget(x, y)` / `sset(x, y, [col])` | read / write a sheet pixel |
| `fget(n, [f])` / `fset(n, [f], v)` | sprite flags: whole byte, or flag f (0–7) as a boolean |
| `map([cx, cy, sx, sy, cw, ch, layer])` | draw map cells; cell value 0 is not drawn; with `layer`, only sprites with `fget(n) & layer ~= 0` |
| `mget(x, y)` / `mset(x, y, n)` | read / write a map cell; out of range reads 0 |
| `palt(col, t)` / `palt()` | make col transparent (t true) or opaque for sprites; no args: only 0 transparent |

Sprite pixels go through the draw palette (`pal`) and the camera like everything else.
`pal()` also resets `palt`. Defaults for `map`: `0, 0, 0, 0, 128, 64`.

## Cart file data sections

Code comes first. The first line that is exactly a section marker ends the code; the code
handed to Lua (or the nib translator) is everything before it, so line numbers are unchanged.

```
...code...
__gfx__
0000000000000000...   (up to 128 lines of up to 128 hex digits: one pixel each)
__gff__
00010000...           (up to 2 lines of up to 256 hex digits: two per sprite)
__map__
0102...               (up to 64 lines of up to 256 hex digits: two per cell)
```

- Missing lines or a short line are zeros. A trailing `\r` is ignored.
- Writing omits trailing all-zero rows and any section that is all zero, so a cart with no
  sprites has no data at all.
- Errors (`path:line: message`, shown on the error screen like any other):
  `bad hex digit in __gfx__`, `too many lines in __map__`, `line too long in __gff__`,
  `unknown section __foo__`.

## Components

- `src/cart.c` / `cart.h`:
  - `char *cart_read(const char *path, size_t *code_len, char *err, size_t errlen)` reads the
    file, fills `gfx_sheet`/`gfx_flags`/`gfx_map` (zeroing them first) and returns the
    malloc'd code part, or NULL with `err` set.
  - `bool cart_write(const char *path, const char *code, size_t code_len, char *err, size_t errlen)`
    writes the code followed by the sections from memory, adding a newline after the code if it
    doesn't end with one. The sprite editor uses it.
- `api.c`: `api_load` and `api_check` load their code through `cart_read` instead of reading the
  file directly. The nib translator only ever sees the code part.
- `gfx.c`: memory, `gfx_spr`-level drawing (`gfx_sspr` with flips and stretching), `palt` state.

## Testing

- `tests/cart_test.c`, built and run by `make test`:
  - read: code split at the marker, pixels/flags/cells land in the right places, missing and
    short rows are zero, CRLF files work
  - write then read gives the same memory and code; an all-zero cart writes no sections;
    trailing zero rows are omitted
  - each error message and line number
- Manual: `carts/demo.nib` gets a player sprite drawn with `spr` instead of a circle.
