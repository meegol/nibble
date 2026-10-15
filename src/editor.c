#include "editor.h"
#include "code_edit.h"
#include "gfx.h"
#include "sprite_edit.h"
#include "ui.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define STATUS_FRAMES 180

#define COL_BAR      8   /* navy */
#define COL_BAR_TEXT 13  /* silver */
#define COL_TAB_ON   12  /* white */

typedef struct {
    const char *label;
    void (*event)(const SDL_Event *ev);
    void (*draw)(void);
    void (*info)(char *buf, size_t n);
    bool text_input;
} Tab;

static const Tab TABS[] = {
    {"CODE", code_event, code_draw, code_info, true},
    {"SPR", sprite_event, sprite_draw, sprite_info, false},
};
#define NUM_TABS (int)(sizeof TABS / sizeof TABS[0])

static Cart *cart;
static const unsigned *saved_version;
static const char *name;
static SDL_Window *window;
static int tab;
static bool active;
static char status[64];
static int status_frames;

void editor_init(Cart *c, const char *path, SDL_Window *win, const unsigned *saved) {
    cart = c;
    saved_version = saved;
    window = win;
    const char *slash = strrchr(path, '/');
    name = slash ? slash + 1 : path;
    code_init(c, path);
    sprite_init(c);
}

static void update_text_input(void) {
    if (active && TABS[tab].text_input) SDL_StartTextInput(window);
    else SDL_StopTextInput(window);
}

static void set_tab(int t) {
    tab = (t + NUM_TABS) % NUM_TABS;
    update_text_input();
}

void editor_enter(void) {
    active = true;
    update_text_input();
}

void editor_leave(void) {
    active = false;
    update_text_input();
}

void editor_cart_changed(void) {
    code_reload();
}

void editor_goto_line(int line) {
    set_tab(0);
    code_goto_line(line);
}

void editor_status(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(status, sizeof status, fmt, ap);
    va_end(ap);
    status_frames = STATUS_FRAMES;
}

/* x of tab i; tabs are right-aligned in the top bar. */
static int tab_x(int i) {
    int x = SCREEN_W;
    for (int j = NUM_TABS - 1; j >= i; j--) x -= ui_text_w(TABS[j].label) + 2;
    return x;
}

void editor_event(const SDL_Event *ev) {
    if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.key == SDLK_TAB && (ev->key.mod & SDL_KMOD_CTRL)) {
        set_tab(tab + ((ev->key.mod & SDL_KMOD_SHIFT) ? -1 : 1));
        return;
    }
    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev->button.y < EDIT_TOP) {
        for (int i = 0; i < NUM_TABS; i++) {
            int x = tab_x(i);
            if (ui_in((int)ev->button.x, (int)ev->button.y, x, 0, ui_text_w(TABS[i].label) + 2, EDIT_TOP))
                set_tab(i);
        }
        return;
    }
    TABS[tab].event(ev);
}

void editor_draw(void) {
    GfxState saved;
    gfx_get_state(&saved);
    gfx_camera(0, 0);
    gfx_pal_reset();

    TABS[tab].draw();

    /* top bar */
    gfx_rectfill(0, 0, SCREEN_W - 1, EDIT_TOP - 1, COL_BAR);
    char title[40];
    snprintf(title, sizeof title, "%s%s", name, cart->version != *saved_version ? "*" : "");
    gfx_print(title, 1, 1, COL_BAR_TEXT);
    for (int i = 0; i < NUM_TABS; i++) {
        int x = tab_x(i);
        if (i == tab) gfx_rectfill(x, 0, x + ui_text_w(TABS[i].label), EDIT_TOP - 1, COL_TAB_ON);
        gfx_print(TABS[i].label, x + 1, 1, i == tab ? COL_BAR : COL_BAR_TEXT);
    }

    /* status bar */
    gfx_rectfill(0, EDIT_BOTTOM, SCREEN_W - 1, SCREEN_H - 1, COL_BAR);
    char info[64];
    if (status_frames > 0) {
        status_frames--;
        snprintf(info, sizeof info, "%s", status);
    } else {
        TABS[tab].info(info, sizeof info);
    }
    gfx_print(info, 1, EDIT_BOTTOM + 1, COL_BAR_TEXT);

    gfx_set_state(&saved);
}
