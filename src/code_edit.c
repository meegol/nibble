#include "code_edit.h"
#include "editor.h"
#include "gfx.h"

#include <stdio.h>

static Cart *cart;

void code_init(Cart *c, const char *path) {
    (void)path;
    cart = c;
}

void code_reload(void) {}

void code_event(const SDL_Event *ev) {
    (void)ev;
}

void code_draw(void) {
    gfx_rectfill(0, EDIT_TOP, SCREEN_W - 1, EDIT_BOTTOM - 1, 0);
    gfx_print("CODE EDITOR", 4, EDIT_TOP + 4, 13);
}

void code_info(char *buf, size_t n) {
    snprintf(buf, n, "%zu bytes", cart->code_len);
}

void code_goto_line(int line) {
    (void)line;
}
