#include "gfx.h"

#include <stdio.h>
#include <string.h>

static int passed, failed;

#define CHECK(cond) do { \
    if (cond) passed++; \
    else { failed++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

/* Sprite n's pixel (x, y). */
static void sp(int n, int x, int y, int c) {
    gfx_sheet[(n / 16 * 8 + y) * SHEET_W + n % 16 * 8 + x] = c;
}

int main(void) {
    memset(gfx_sheet, 0, sizeof gfx_sheet);
    sp(1, 0, 0, 7);   /* top-left */
    sp(1, 7, 7, 9);   /* bottom-right */
    sp(1, 1, 0, 0);   /* transparent */

    /* plain draw, color 0 transparent */
    gfx_reset();
    gfx_cls(3);
    gfx_sspr(8, 0, 8, 8, 10, 20, 8, 8, false, false);
    CHECK(gfx_pget(10, 20) == 7);
    CHECK(gfx_pget(17, 27) == 9);
    CHECK(gfx_pget(11, 20) == 3);

    /* flips */
    gfx_cls(3);
    gfx_sspr(8, 0, 8, 8, 0, 0, 8, 8, true, false);
    CHECK(gfx_pget(7, 0) == 7);
    CHECK(gfx_pget(0, 7) == 9);
    gfx_cls(3);
    gfx_sspr(8, 0, 8, 8, 0, 0, 8, 8, false, true);
    CHECK(gfx_pget(0, 7) == 7);
    CHECK(gfx_pget(7, 0) == 9);

    /* stretch 2x */
    gfx_cls(3);
    gfx_sspr(8, 0, 8, 8, 0, 0, 16, 16, false, false);
    CHECK(gfx_pget(1, 1) == 7);
    CHECK(gfx_pget(15, 15) == 9);
    CHECK(gfx_pget(14, 14) == 9);
    CHECK(gfx_pget(13, 13) == 0 || gfx_pget(13, 13) == 3);

    /* palt and pal */
    gfx_cls(3);
    gfx_palt(0, false);
    gfx_palt(7, true);
    gfx_pal(9, 12);
    gfx_sspr(8, 0, 8, 8, 0, 0, 8, 8, false, false);
    CHECK(gfx_pget(0, 0) == 3);
    CHECK(gfx_pget(1, 0) == 0);
    CHECK(gfx_pget(7, 7) == 12);
    gfx_pal_reset();
    gfx_cls(3);
    gfx_sspr(8, 0, 8, 8, 0, 0, 8, 8, false, false);
    CHECK(gfx_pget(0, 0) == 7 && gfx_pget(1, 0) == 3);

    /* camera */
    gfx_cls(3);
    gfx_camera(-5, 0);
    gfx_sspr(8, 0, 8, 8, 0, 0, 8, 8, false, false);
    gfx_camera(0, 0);
    CHECK(gfx_pget(5, 0) == 7);

    /* map: cell 0 skipped, layers */
    memset(gfx_map, 0, sizeof gfx_map);
    memset(gfx_flags, 0, sizeof gfx_flags);
    sp(2, 0, 0, 5);
    gfx_map[0] = 1;
    gfx_map[1] = 2;
    gfx_map[MAP_W] = 0;
    gfx_flags[2] = 0x02;
    gfx_cls(3);
    gfx_map_draw(0, 0, 0, 0, 2, 2, 0);
    CHECK(gfx_pget(0, 0) == 7);
    CHECK(gfx_pget(8, 0) == 5);
    CHECK(gfx_pget(0, 8) == 3);
    gfx_cls(3);
    gfx_map_draw(0, 0, 0, 0, 2, 2, 0x02);
    CHECK(gfx_pget(0, 0) == 3);
    CHECK(gfx_pget(8, 0) == 5);
    gfx_cls(3);
    gfx_map_draw(1, 0, 4, 4, 1, 1, 0);
    CHECK(gfx_pget(4, 4) == 5);

    /* off-sheet and off-map reads draw nothing */
    gfx_cls(3);
    gfx_sspr(124, 124, 8, 8, 0, 0, 8, 8, false, false);
    gfx_map_draw(-1, -1, 0, 0, 1, 1, 0);
    CHECK(gfx_pget(6, 6) == 3);

    printf("ok %d, failed %d\n", passed, failed);
    return failed ? 1 : 0;
}
