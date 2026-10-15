#ifndef NIBBLE_UI_H
#define NIBBLE_UI_H

#include "gfx.h"

#include <stdbool.h>
#include <string.h>

/* Width of s in pixels with the built-in font. */
static inline int ui_text_w(const char *s) {
    return (int)strlen(s) * FONT_W;
}

static inline bool ui_in(int px, int py, int x, int y, int w, int h) {
    return px >= x && py >= y && px < x + w && py < y + h;
}

#endif
