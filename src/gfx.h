#ifndef NIBBLE_GFX_H
#define NIBBLE_GFX_H

#include <stdint.h>

#define SCREEN_W   128
#define SCREEN_H   128
#define NUM_COLORS 16
#define FONT_W     4   /* glyph advance, including 1px spacing */
#define FONT_H     6   /* line height, including 1px spacing */

/* Resets screen, camera and draw palette. */
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

void gfx_camera(int x, int y);
void gfx_pal(int from, int to);
void gfx_pal_reset(void);

/* Converts the indexed framebuffer to XRGB8888 pixels. */
void gfx_to_rgb(uint32_t *out);

#endif
