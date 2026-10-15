#ifndef NIBBLE_GFX_H
#define NIBBLE_GFX_H

#include <stdbool.h>
#include <stdint.h>

#define SCREEN_W   128
#define SCREEN_H   128
#define NUM_COLORS 16
#define FONT_W     4   /* glyph advance, including 1px spacing */
#define FONT_H     6   /* line height, including 1px spacing */

#define SHEET_W     128
#define SHEET_H     128
#define SPRITE_SIZE 8
#define NUM_SPRITES 256
#define MAP_W       128
#define MAP_H       64

/* Sprite sheet (one color index per pixel), sprite flags and map cells. */
extern uint8_t gfx_sheet[SHEET_W * SHEET_H];
extern uint8_t gfx_flags[NUM_SPRITES];
extern uint8_t gfx_map[MAP_W * MAP_H];

/* Resets screen, camera, draw palette and transparency. */
void gfx_reset(void);

void gfx_cls(int col);
void gfx_pset(int x, int y, int col);
int  gfx_pget(int x, int y);
void gfx_line(int x0, int y0, int x1, int y1, int col);
void gfx_rect(int x0, int y0, int x1, int y1, int col);
void gfx_rectfill(int x0, int y0, int x1, int y1, int col);
void gfx_circ(int cx, int cy, int r, int col);
void gfx_circfill(int cx, int cy, int r, int col);

/* Draws text with the built-in font. Returns the y below the last line. */
int  gfx_print(const char *s, int x, int y, int col);

/* Draws sheet rectangle (sx, sy, sw, sh) stretched to (dx, dy, dw, dh),
   skipping transparent colors. */
void gfx_sspr(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
              bool flip_x, bool flip_y);

/* Draws map cells (cx, cy, cw, ch) at screen (sx, sy). Cells holding 0 are skipped;
   a non-zero layer skips sprites whose flags don't share a bit with it. */
void gfx_map_draw(int cx, int cy, int sx, int sy, int cw, int ch, int layer);

void gfx_camera(int x, int y);
/* Also resets transparency to only color 0. */
void gfx_pal_reset(void);
void gfx_pal(int from, int to);
void gfx_palt(int col, bool transparent);

/* Converts the indexed framebuffer to XRGB8888 pixels. */
void gfx_to_rgb(uint32_t *out);

#endif
