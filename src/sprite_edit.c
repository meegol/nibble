#include "sprite_edit.h"
#include "editor.h"
#include "gfx.h"

#include <stdio.h>

static Cart *cart;

void sprite_init(Cart *c) {
    cart = c;
}

void sprite_event(const SDL_Event *ev) {
    (void)ev;
}

void sprite_draw(void) {
    gfx_rectfill(0, EDIT_TOP, SCREEN_W - 1, EDIT_BOTTOM - 1, 0);
    gfx_print("SPRITE EDITOR", 4, EDIT_TOP + 4, 13);
}

void sprite_info(char *buf, size_t n) {
    (void)cart;
    snprintf(buf, n, "sprites");
}
