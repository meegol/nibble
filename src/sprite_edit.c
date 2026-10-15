#include "sprite_edit.h"
#include "editor.h"
#include "gfx.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Layout */
#define CANVAS_X   2
#define CANVAS_Y   (EDIT_TOP + 2)
#define CANVAS_W   64
#define PAL_X      70
#define PAL_Y      CANVAS_Y
#define PAL_CW     14
#define PAL_CH     8
#define NUM_Y      (PAL_Y + 4 * PAL_CH + 3)
#define FLAGS_Y    (NUM_Y + 8)
#define FLAG_STEP  7
#define TOOLS_Y    (FLAGS_Y + 8)
#define SHEET_Y    (CANVAS_Y + CANVAS_W + 3)
#define PAGE_ROWS  4               /* sprite rows per sheet page */
#define PAGES_Y    (SHEET_Y + PAGE_ROWS * SPRITE_SIZE + 2)
#define MAX_UNDO   50

#define COL_BG     0
#define COL_PANEL  15
#define COL_TEXT   13
#define COL_HI     12

enum { TOOL_PEN, TOOL_FILL };

static Cart *cart;
static int sprite = 1;   /* selected sprite (top-left of the selection) */
static int size = 1;     /* 1: 8x8, 2: 16x16 */
static int color = 12;
static int tool = TOOL_PEN;
static int page;
static bool painting;
static int hover_x = -1, hover_y = -1;  /* canvas pixel under the mouse */

typedef struct {
    uint8_t sheet[SHEET_W * SHEET_H];
    uint8_t flags[NUM_SPRITES];
} Snapshot;

static Snapshot *undo[MAX_UNDO], *redo[MAX_UNDO];
static int nundo, nredo;
static uint8_t clip[16 * 16];
static int clip_size;  /* 0: empty */

/* ---- undo ---- */

static Snapshot *snap(void) {
    Snapshot *s = malloc(sizeof *s);
    if (!s) abort();
    memcpy(s->sheet, cart->sheet, sizeof s->sheet);
    memcpy(s->flags, cart->flags, sizeof s->flags);
    return s;
}

static void push(Snapshot **stack, int *n, Snapshot *s) {
    if (*n == MAX_UNDO) {
        free(stack[0]);
        memmove(stack, stack + 1, (MAX_UNDO - 1) * sizeof *stack);
        (*n)--;
    }
    stack[(*n)++] = s;
}

static void before_edit(void) {
    push(undo, &nundo, snap());
    while (nredo > 0) free(redo[--nredo]);
}

static void restore(Snapshot **from, int *nfrom, Snapshot **to, int *nto) {
    if (*nfrom == 0) return;
    push(to, nto, snap());
    Snapshot *s = from[--(*nfrom)];
    memcpy(cart->sheet, s->sheet, sizeof s->sheet);
    memcpy(cart->flags, s->flags, sizeof s->flags);
    free(s);
    cart->version++;
}

/* ---- pixels of the selection ---- */

static int sel_px(void) { return sprite % 16 * SPRITE_SIZE; }
static int sel_py(void) { return sprite / 16 * SPRITE_SIZE; }
static int sel_w(void) { return size * SPRITE_SIZE; }

static uint8_t *pixel(int x, int y) {
    return &cart->sheet[(sel_py() + y) * SHEET_W + sel_px() + x];
}

static void fill(int x, int y, int from, int to) {
    if (x < 0 || y < 0 || x >= sel_w() || y >= sel_w() || *pixel(x, y) != from) return;
    *pixel(x, y) = to;
    fill(x + 1, y, from, to);
    fill(x - 1, y, from, to);
    fill(x, y + 1, from, to);
    fill(x, y - 1, from, to);
}

static void select_sprite(int n) {
    /* keep a 2x2 selection inside the sheet */
    if (size == 2) {
        if (n % 16 == 15) n--;
        if (n / 16 == 15) n -= 16;
    }
    sprite = (n + NUM_SPRITES) % NUM_SPRITES;
    page = sprite / 16 / PAGE_ROWS;
}

static void paint_at(float mx, float my, bool start) {
    int zoom = CANVAS_W / sel_w();
    int x = ((int)mx - CANVAS_X) / zoom, y = ((int)my - CANVAS_Y) / zoom;
    if (mx < CANVAS_X || my < CANVAS_Y || x >= sel_w() || y >= sel_w()) return;
    if (tool == TOOL_FILL) {
        if (!start || *pixel(x, y) == color) return;
        before_edit();
        fill(x, y, *pixel(x, y), color);
    } else {
        if (start) before_edit();
        *pixel(x, y) = color;
    }
    cart->version++;
}

static void flip(bool horizontal) {
    before_edit();
    int w = sel_w();
    for (int y = 0; y < (horizontal ? w : w / 2); y++)
        for (int x = 0; x < (horizontal ? w / 2 : w); x++) {
            uint8_t *a = pixel(x, y);
            uint8_t *b = horizontal ? pixel(w - 1 - x, y) : pixel(x, w - 1 - y);
            uint8_t t = *a;
            *a = *b;
            *b = t;
        }
    cart->version++;
}

static void clear_selection(void) {
    before_edit();
    for (int y = 0; y < sel_w(); y++)
        for (int x = 0; x < sel_w(); x++) *pixel(x, y) = 0;
    cart->version++;
}

static void copy(void) {
    clip_size = size;
    for (int y = 0; y < sel_w(); y++)
        for (int x = 0; x < sel_w(); x++) clip[y * 16 + x] = *pixel(x, y);
    editor_status("copied");
}

static void paste(void) {
    if (!clip_size) return;
    before_edit();
    int w = clip_size * SPRITE_SIZE;
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            int px = sel_px() + x, py = sel_py() + y;
            if (px < SHEET_W && py < SHEET_H) cart->sheet[py * SHEET_W + px] = clip[y * 16 + x];
        }
    cart->version++;
}

/* ---- input ---- */

static void click(float mx, float my, Uint8 button) {
    int x = (int)mx, y = (int)my;
    if (ui_in(x, y, CANVAS_X, CANVAS_Y, CANVAS_W, CANVAS_W)) {
        if (button == SDL_BUTTON_RIGHT) {
            int zoom = CANVAS_W / sel_w();
            color = *pixel((x - CANVAS_X) / zoom, (y - CANVAS_Y) / zoom);
        } else {
            painting = true;
            paint_at(mx, my, true);
        }
    } else if (ui_in(x, y, PAL_X, PAL_Y, 4 * PAL_CW, 4 * PAL_CH)) {
        color = (y - PAL_Y) / PAL_CH * 4 + (x - PAL_X) / PAL_CW;
    } else if (ui_in(x, y, PAL_X, FLAGS_Y, 8 * FLAG_STEP, FLAG_STEP)) {
        before_edit();
        cart->flags[sprite] ^= 1 << ((x - PAL_X) / FLAG_STEP);
        cart->version++;
    } else if (ui_in(x, y, PAL_X, TOOLS_Y, 4 * FONT_W + 2, FONT_H + 1)) {
        tool = TOOL_PEN;
    } else if (ui_in(x, y, PAL_X + 20, TOOLS_Y, 4 * FONT_W + 2, FONT_H + 1)) {
        tool = TOOL_FILL;
    } else if (ui_in(x, y, PAL_X + 40, TOOLS_Y, 3 * FONT_W + 2, FONT_H + 1)) {
        size = 3 - size;
        select_sprite(sprite);
    } else if (ui_in(x, y, 0, SHEET_Y, SCREEN_W, PAGE_ROWS * SPRITE_SIZE)) {
        select_sprite((page * PAGE_ROWS + (y - SHEET_Y) / SPRITE_SIZE) * 16 + x / SPRITE_SIZE);
    } else if (ui_in(x, y, CANVAS_X, PAGES_Y, 4 * 10, FONT_H + 1)) {
        page = (x - CANVAS_X) / 10;
    }
}

static void key(const SDL_KeyboardEvent *k) {
    bool ctrl = k->mod & SDL_KMOD_CTRL;
    switch (k->key) {
    case SDLK_LEFTBRACKET: select_sprite(sprite - size); break;
    case SDLK_RIGHTBRACKET: select_sprite(sprite + size); break;
    case SDLK_H: flip(true); break;
    case SDLK_V:
        if (ctrl) paste();
        else flip(false);
        break;
    case SDLK_C:
        if (ctrl) copy();
        break;
    case SDLK_DELETE:
    case SDLK_BACKSPACE: clear_selection(); break;
    case SDLK_P: tool = TOOL_PEN; break;
    case SDLK_F: tool = TOOL_FILL; break;
    case SDLK_Z:
        if (ctrl && (k->mod & SDL_KMOD_SHIFT)) restore(redo, &nredo, undo, &nundo);
        else if (ctrl) restore(undo, &nundo, redo, &nredo);
        break;
    case SDLK_Y:
        if (ctrl) restore(redo, &nredo, undo, &nundo);
        break;
    default:
        /* 1-4 pick a sheet page */
        if (k->key >= SDLK_1 && k->key <= SDLK_4) page = (int)(k->key - SDLK_1);
        break;
    }
}

void sprite_init(Cart *c) {
    cart = c;
}

void sprite_event(const SDL_Event *ev) {
    switch (ev->type) {
    case SDL_EVENT_KEY_DOWN: key(&ev->key); break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: click(ev->button.x, ev->button.y, ev->button.button); break;
    case SDL_EVENT_MOUSE_BUTTON_UP: painting = false; break;
    case SDL_EVENT_MOUSE_MOTION: {
        int zoom = CANVAS_W / sel_w();
        int x = (int)ev->motion.x, y = (int)ev->motion.y;
        bool in = ui_in(x, y, CANVAS_X, CANVAS_Y, CANVAS_W, CANVAS_W);
        hover_x = in ? (x - CANVAS_X) / zoom : -1;
        hover_y = in ? (y - CANVAS_Y) / zoom : -1;
        if (painting && (ev->motion.state & SDL_BUTTON_LMASK) && tool == TOOL_PEN)
            paint_at(ev->motion.x, ev->motion.y, false);
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL:
        select_sprite(sprite + (ev->wheel.y > 0 ? -size : size));
        break;
    default: break;
    }
}

static void button(int x, int y, const char *label, bool on) {
    int w = ui_text_w(label) + 1;
    gfx_rectfill(x, y, x + w, y + FONT_H, on ? COL_HI : COL_BG);
    gfx_print(label, x + 1, y + 1, on ? COL_BG : COL_TEXT);
}

void sprite_draw(void) {
    gfx_rectfill(0, EDIT_TOP, SCREEN_W - 1, EDIT_BOTTOM - 1, COL_PANEL);

    /* canvas */
    int w = sel_w(), zoom = CANVAS_W / w;
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            int c = *pixel(x, y);
            int px = CANVAS_X + x * zoom, py = CANVAS_Y + y * zoom;
            gfx_rectfill(px, py, px + zoom - 1, py + zoom - 1, c);
        }
    if (hover_x >= 0) {
        int px = CANVAS_X + hover_x * zoom, py = CANVAS_Y + hover_y * zoom;
        gfx_rect(px, py, px + zoom - 1, py + zoom - 1, COL_HI);
    }

    /* palette */
    for (int i = 0; i < NUM_COLORS; i++) {
        int x = PAL_X + i % 4 * PAL_CW, y = PAL_Y + i / 4 * PAL_CH;
        gfx_rectfill(x, y, x + PAL_CW - 1, y + PAL_CH - 1, i);
        if (i == color) {
            gfx_rect(x, y, x + PAL_CW - 1, y + PAL_CH - 1, COL_BG);
            gfx_rect(x + 1, y + 1, x + PAL_CW - 2, y + PAL_CH - 2, COL_HI);
        }
    }

    /* sprite number and flags */
    char num[16];
    snprintf(num, sizeof num, "#%03d", sprite);
    gfx_print(num, PAL_X, NUM_Y, COL_HI);
    for (int f = 0; f < 8; f++) {
        int x = PAL_X + f * FLAG_STEP;
        if (cart->flags[sprite] & (1 << f)) gfx_rectfill(x, FLAGS_Y, x + 4, FLAGS_Y + 4, 2 + f);
        else gfx_rect(x, FLAGS_Y, x + 4, FLAGS_Y + 4, COL_TEXT);
    }

    /* tools */
    button(PAL_X, TOOLS_Y, "PEN", tool == TOOL_PEN);
    button(PAL_X + 20, TOOLS_Y, "FILL", tool == TOOL_FILL);
    button(PAL_X + 40, TOOLS_Y, size == 1 ? "8X8" : "16", false);

    /* sheet page */
    gfx_rectfill(0, SHEET_Y - 1, SCREEN_W - 1, SHEET_Y + PAGE_ROWS * SPRITE_SIZE, COL_BG);
    int top = page * PAGE_ROWS * SPRITE_SIZE;
    for (int y = 0; y < PAGE_ROWS * SPRITE_SIZE; y++)
        for (int x = 0; x < SHEET_W; x++) {
            int c = cart->sheet[(top + y) * SHEET_W + x];
            if (c) gfx_pset(x, SHEET_Y + y, c);
        }
    int sx = sel_px(), sy = sel_py() - top;
    if (sy + w > 0 && sy < PAGE_ROWS * SPRITE_SIZE)
        gfx_rect(sx - 1 < 0 ? 0 : sx - 1, SHEET_Y + sy - 1, sx + w, SHEET_Y + sy + w, COL_HI);

    /* page buttons */
    for (int p = 0; p < 4; p++) {
        char label[2] = {(char)('1' + p), '\0'};
        button(CANVAS_X + p * 10, PAGES_Y, label, p == page);
    }
}

void sprite_info(char *buf, size_t n) {
    if (hover_x >= 0)
        snprintf(buf, n, "#%d x%d y%d", sprite, hover_x, hover_y);
    else
        snprintf(buf, n, "#%d  [ ] sprite  h v flip", sprite);
}
