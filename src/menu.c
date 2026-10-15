#include "menu.h"
#include "gfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CARTS 256
#define TOP       9                 /* first list row */
#define ROWS      ((SCREEN_H - TOP - 8) / FONT_H)

#define COL_BG    0
#define COL_BAR   8
#define COL_TEXT  13
#define COL_SEL   12
#define COL_NEW   5

static char root[256];
static char *paths[MAX_CARTS];  /* entry 0 is "new cart" (NULL) */
static int count;
static int sel, scroll;
static char new_path[300];

static bool is_cart(const char *name) {
    size_t n = strlen(name);
    return (n > 4 && strcmp(name + n - 4, ".nib") == 0)
        || (n > 4 && strcmp(name + n - 4, ".lua") == 0);
}

static SDL_EnumerationResult SDLCALL found(void *userdata, const char *dir, const char *name) {
    (void)userdata;
    if (name[0] == '.' || count == MAX_CARTS) return SDL_ENUM_CONTINUE;
    char path[512];
    size_t n = strlen(dir);
    snprintf(path, sizeof path, "%s%s%s", dir, n && dir[n - 1] == '/' ? "" : "/", name);
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info)) return SDL_ENUM_CONTINUE;
    if (info.type == SDL_PATHTYPE_DIRECTORY) SDL_EnumerateDirectory(path, found, NULL);
    else if (is_cart(name)) paths[count++] = SDL_strdup(path);
    return SDL_ENUM_CONTINUE;
}

static int by_name(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

void menu_open(const char *dir) {
    for (int i = 1; i < count; i++) SDL_free(paths[i]);
    snprintf(root, sizeof root, "%s", dir);
    count = 1;
    paths[0] = NULL;
    SDL_EnumerateDirectory(dir, found, NULL);
    qsort(paths + 1, count - 1, sizeof paths[0], by_name);
    if (sel >= count) sel = count - 1;
}

/* Path shown in the list: relative to the root folder. */
static const char *label(int i) {
    if (i == 0) return "+ new cart";
    size_t n = strlen(root);
    const char *p = paths[i];
    if (strncmp(p, root, n) == 0) p += n;
    while (*p == '/') p++;
    return p;
}

/* First unused cartN.nib in the root folder. */
static const char *next_new_path(void) {
    for (int n = 1;; n++) {
        snprintf(new_path, sizeof new_path, "%s/cart%d.nib", root, n);
        if (!SDL_GetPathInfo(new_path, NULL)) return new_path;
    }
}

static const char *choose(void) {
    return sel == 0 ? next_new_path() : paths[sel];
}

static void move(int d) {
    sel += d;
    if (sel < 0) sel = 0;
    if (sel >= count) sel = count - 1;
    if (sel < scroll) scroll = sel;
    if (sel >= scroll + ROWS) scroll = sel - ROWS + 1;
}

const char *menu_event(const SDL_Event *ev) {
    switch (ev->type) {
    case SDL_EVENT_KEY_DOWN:
        switch (ev->key.key) {
        case SDLK_UP: case SDLK_W: move(-1); break;
        case SDLK_DOWN: case SDLK_S: move(1); break;
        case SDLK_PAGEUP: move(-ROWS); break;
        case SDLK_PAGEDOWN: move(ROWS); break;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_Z: case SDLK_X: return choose();
        default: break;
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        move(ev->wheel.y > 0 ? -1 : 1);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        int row = ((int)ev->button.y - TOP) / FONT_H;
        if (ev->button.y >= TOP && row < ROWS && scroll + row < count) {
            if (sel == scroll + row) return choose();
            sel = scroll + row;  /* first click selects, second opens */
        }
        break;
    }
    default: break;
    }
    return NULL;
}

void menu_draw(void) {
    gfx_reset();
    gfx_cls(COL_BG);
    gfx_rectfill(0, 0, SCREEN_W - 1, 6, COL_BAR);
    gfx_print("NIBBLE", 1, 1, COL_SEL);

    for (int r = 0; r < ROWS && scroll + r < count; r++) {
        int i = scroll + r, y = TOP + r * FONT_H;
        if (i == sel) gfx_rectfill(0, y - 1, SCREEN_W - 1, y + FONT_H - 2, COL_BAR);
        gfx_print(label(i), 3, y, i == sel ? COL_SEL : i == 0 ? COL_NEW : COL_TEXT);
    }
    if (count == 1) gfx_print("no carts yet", 3, TOP + 2 * FONT_H, COL_TEXT);

    gfx_rectfill(0, SCREEN_H - 7, SCREEN_W - 1, SCREEN_H - 1, COL_BAR);
    gfx_print("enter: open   ctrl+o: menu", 1, SCREEN_H - 6, COL_TEXT);
}
