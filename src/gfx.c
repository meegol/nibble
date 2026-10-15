#include "gfx.h"

#include <stdlib.h>
#include <string.h>

/* "Sweetie 16" palette by GrafxKid. */
static const uint32_t PALETTE[NUM_COLORS] = {
    0x1a1c2c, 0x5d275d, 0xb13e53, 0xef7d57,
    0xffcd75, 0xa7f070, 0x38b764, 0x257179,
    0x29366f, 0x3b5dc9, 0x41a6f6, 0x73eff7,
    0xf4f4f4, 0x94b0c2, 0x566c86, 0x333c57,
};

/* 3x5 glyphs, rows concatenated. Lowercase is drawn as uppercase. */
static const char *FONT[96] = {
    [' ' - 32] = "...............",
    ['!' - 32] = ".#..#..#.....#.",
    ['"' - 32] = "#.##.#.........",
    ['#' - 32] = "#.#####.#####.#",
    ['$' - 32] = ".###..###..###.",
    ['%' - 32] = "#.#..#.#.#..#.#",
    ['&' - 32] = ".#.#.#.#.#.#.##",
    ['\'' - 32] = ".#..#..........",
    ['(' - 32] = "..#.#..#..#...#",
    [')' - 32] = "#...#..#..#.#..",
    ['*' - 32] = "#.#.#.###.#.#.#",
    ['+' - 32] = "....#.###.#....",
    [',' - 32] = "..........#.#..",
    ['-' - 32] = "......###......",
    ['.' - 32] = ".............#.",
    ['/' - 32] = "..#..#.#.#..#..",
    ['0' - 32] = "####.##.##.####",
    ['1' - 32] = ".#.##..#..#.###",
    ['2' - 32] = "###..#####..###",
    ['3' - 32] = "###..#.##..####",
    ['4' - 32] = "#.##.####..#..#",
    ['5' - 32] = "####..###..####",
    ['6' - 32] = "#..#..####.####",
    ['7' - 32] = "###..#..#..#..#",
    ['8' - 32] = "####.#####.####",
    ['9' - 32] = "####.####..#..#",
    [':' - 32] = "....#.....#....",
    [';' - 32] = "....#.....#.#..",
    ['<' - 32] = "..#.#.#...#...#",
    ['=' - 32] = "...###...###...",
    ['>' - 32] = "#...#...#.#.#..",
    ['?' - 32] = "###..#.##....#.",
    ['@' - 32] = ".#.#.##.##...##",
    ['A' - 32] = "####.#####.##.#",
    ['B' - 32] = "####.###.#.####",
    ['C' - 32] = ".###..#..#...##",
    ['D' - 32] = "##.#.##.##.####",
    ['E' - 32] = "####..##.#..###",
    ['F' - 32] = "####..##.#..#..",
    ['G' - 32] = ".###..#.##.####",
    ['H' - 32] = "#.##.#####.##.#",
    ['I' - 32] = "###.#..#..#.###",
    ['J' - 32] = "###.#..#..#.##.",
    ['K' - 32] = "#.##.###.#.##.#",
    ['L' - 32] = "#..#..#..#..###",
    ['M' - 32] = "#######.##.##.#",
    ['N' - 32] = "##.#.##.##.##.#",
    ['O' - 32] = ".###.##.##.###.",
    ['P' - 32] = "####.#####..#..",
    ['Q' - 32] = ".#.#.##.###..##",
    ['R' - 32] = "####.###.#.##.#",
    ['S' - 32] = ".###..###..###.",
    ['T' - 32] = "###.#..#..#..#.",
    ['U' - 32] = "#.##.##.##.#.##",
    ['V' - 32] = "#.##.##.####.#.",
    ['W' - 32] = "#.##.##.#######",
    ['X' - 32] = "#.##.#.#.#.##.#",
    ['Y' - 32] = "#.##.####..####",
    ['Z' - 32] = "###..#.#.#..###",
    ['[' - 32] = "##.#..#..#..##.",
    ['\\' - 32] = "#..#...#...#..#",
    [']' - 32] = ".##..#..#..#.##",
    ['^' - 32] = ".#.#.#.........",
    ['_' - 32] = "............###",
    ['`' - 32] = "#...#..........",
    ['{' - 32] = ".##.#.##..#..##",
    ['|' - 32] = ".#..#..#..#..#.",
    ['}' - 32] = "##..#..##.#.##.",
    ['~' - 32] = ".....#####.....",
};

static uint8_t screen[SCREEN_W * SCREEN_H];
static uint8_t draw_pal[NUM_COLORS];
static int cam_x, cam_y;

void gfx_reset(void) {
    gfx_cls(0);
    gfx_camera(0, 0);
    gfx_pal_reset();
}

void gfx_cls(int col) {
    memset(screen, col & 15, sizeof screen);
}

void gfx_pset(int x, int y, int col) {
    x -= cam_x;
    y -= cam_y;
    if (x < 0 || y < 0 || x >= SCREEN_W || y >= SCREEN_H) return;
    screen[y * SCREEN_W + x] = draw_pal[col & 15];
}

int gfx_pget(int x, int y) {
    x -= cam_x;
    y -= cam_y;
    if (x < 0 || y < 0 || x >= SCREEN_W || y >= SCREEN_H) return 0;
    return screen[y * SCREEN_W + x];
}

void gfx_line(int x0, int y0, int x1, int y1, int col) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        gfx_pset(x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void hline(int x0, int x1, int y, int col) {
    for (int x = x0; x <= x1; x++) gfx_pset(x, y, col);
}

void gfx_rect(int x0, int y0, int x1, int y1, int col) {
    gfx_line(x0, y0, x1, y0, col);
    gfx_line(x0, y1, x1, y1, col);
    gfx_line(x0, y0, x0, y1, col);
    gfx_line(x1, y0, x1, y1, col);
}

void gfx_rectfill(int x0, int y0, int x1, int y1, int col) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; y++) hline(x0, x1, y, col);
}

/* Midpoint circle algorithm. */
void gfx_circ(int cx, int cy, int r, int col) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        gfx_pset(cx + x, cy + y, col); gfx_pset(cx - x, cy + y, col);
        gfx_pset(cx + x, cy - y, col); gfx_pset(cx - x, cy - y, col);
        gfx_pset(cx + y, cy + x, col); gfx_pset(cx - y, cy + x, col);
        gfx_pset(cx + y, cy - x, col); gfx_pset(cx - y, cy - x, col);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

void gfx_circfill(int cx, int cy, int r, int col) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        hline(cx - x, cx + x, cy + y, col);
        hline(cx - x, cx + x, cy - y, col);
        hline(cx - y, cx + y, cy + x, col);
        hline(cx - y, cx + y, cy - x, col);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

int gfx_print(const char *s, int x, int y, int col) {
    int start_x = x;
    for (; *s; s++) {
        int c = (unsigned char)*s;
        if (c == '\n') { x = start_x; y += FONT_H; continue; }
        if (c >= 'a' && c <= 'z') c -= 32;
        const char *g = (c >= 32 && c < 128) ? FONT[c - 32] : NULL;
        if (g) {
            for (int i = 0; g[i] && i < 15; i++)
                if (g[i] == '#') gfx_pset(x + i % 3, y + i / 3, col);
        }
        x += FONT_W;
    }
    return y + FONT_H;
}

void gfx_camera(int x, int y) {
    cam_x = x;
    cam_y = y;
}

void gfx_pal(int from, int to) {
    draw_pal[from & 15] = to & 15;
}

void gfx_pal_reset(void) {
    for (int i = 0; i < NUM_COLORS; i++) draw_pal[i] = i;
}

void gfx_to_rgb(uint32_t *out) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) out[i] = PALETTE[screen[i]];
}
