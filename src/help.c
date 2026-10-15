#include "help.h"
#include "editor.h"
#include "gfx.h"

#include <stdio.h>
#include <string.h>

#define COLS 32
#define ROWS ((EDIT_BOTTOM - EDIT_TOP - 2) / FONT_H)

#define COL_BG      0
#define COL_TEXT    13
#define COL_HEAD    4
#define COL_CODE    11

typedef struct {
    const char *sig, *desc;
} Doc;

static const Doc API[] = {
    {"cls([col])", "clear screen (default 0)"},
    {"pset(x,y,[col])", "set a pixel"},
    {"pget(x,y)", "color of a pixel"},
    {"line(x0,y0,x1,y1,[col])", "line"},
    {"rect(x0,y0,x1,y1,[col])", "rectangle outline"},
    {"rectfill(x0,y0,x1,y1,[col])", "filled rectangle"},
    {"circ(x,y,[r],[col])", "circle outline"},
    {"circfill(x,y,[r],[col])", "filled circle"},
    {"print(s,[x,y],[col])", "text; no x,y: next line"},
    {"color(col)", "set the draw color"},
    {"camera([x,y])", "offset all drawing"},
    {"pal(a,b)", "draw color a as b; pal() resets"},
    {"palt(col,t)", "sprite transparency; palt() resets"},
    {"spr(n,x,y,[w,h],[fx],[fy])", "draw sprite n (w,h in sprites)"},
    {"sspr(sx,sy,sw,sh,dx,dy,[dw,dh],[fx],[fy])", "draw part of the sheet, stretched"},
    {"sget(x,y)", "sheet pixel color"},
    {"sset(x,y,[col])", "set a sheet pixel"},
    {"fget(n,[f])", "sprite flags, or flag f (0-7)"},
    {"fset(n,[f],v)", "set sprite flags, or flag f"},
    {"map([cx,cy,sx,sy,cw,ch,layer])", "draw map cells"},
    {"mget(x,y)", "map cell"},
    {"mset(x,y,n)", "set a map cell"},
    {"btn([i])", "is button i held"},
    {"btnp(i)", "was button i just pressed"},
    {"rnd([x])", "random number in [0, x)"},
    {"flr(x)", "round down"},
    {"mid(a,b,c)", "middle of three (clamp)"},
    {"time()", "seconds since start"},
    {"printh(s)", "print to the terminal"},
};
#define NUM_API (int)(sizeof API / sizeof API[0])

/* Help text: one entry per line. A leading '=' marks a heading, '>' code,
   '@' the palette swatches, '%' the API list. */
static const char *TEXT[] = {
    "=KEYS",
    ">esc       game <-> editor",
    ">ctrl+r    run from the start",
    ">ctrl+s    save",
    ">ctrl+o    cart menu",
    ">ctrl+tab  next tab   f1 help",
    "",
    "=GAME LOOP",
    "define any of these:",
    ">def _init():    once at start",
    ">def _update():  60 times a sec",
    ">def _draw():    after update",
    "",
    "=NIBBLE FLAVOUR",
    ">def f(a):       function",
    ">if a: elif b: else:",
    ">for i = 1, 10:",
    ">for k, v in pairs(t):",
    ">while a:",
    ">if a: x = 1     one line",
    ">x += 1  a != b  # comment",
    "blocks end where the indent",
    "goes back. plain lua works too.",
    "",
    "=API",
    "%",
    "also: math, string, table...",
    "",
    "=BUTTONS",
    ">0 left 1 right 2 up 3 down",
    ">4 o (z/c/n)  5 x (x/v/m)",
    "",
    "=PALETTE",
    "@",
    "@",
    "",
    "=CODE EDITOR",
    "red line: syntax error",
    "capitals are underlined",
    ">tab/shift+tab  indent",
    ">ctrl+z/y       undo/redo",
    "",
    "=SPRITE EDITOR",
    "lclick paint, rclick pick",
    ">[ ]  sprite   1-4 page",
    ">p/f  pen/fill h/v flip",
    ">del  clear    ctrl+c/v",
    "",
    "=LEARN",
    "6 lessons in carts/learn.",
    "ctrl+o opens the cart menu:",
    "start with learn/01_lua.nib",
};
#define NUM_TEXT (int)(sizeof TEXT / sizeof TEXT[0])

typedef struct {
    char text[COLS + 1];
    int color;
    int swatch_row;  /* 1 or 2 for palette lines, else 0 */
} Line;

static Line lines[160];
static int nlines;
static int scroll;

static void add(const char *s, int color) {
    if (nlines == (int)(sizeof lines / sizeof lines[0])) return;
    Line *l = &lines[nlines++];
    snprintf(l->text, sizeof l->text, "%s", s);
    l->color = color;
    l->swatch_row = 0;
}

static void build(void) {
    if (nlines) return;
    int swatch = 0;
    for (int i = 0; i < NUM_TEXT; i++) {
        const char *t = TEXT[i];
        if (t[0] == '=') add(t + 1, COL_HEAD);
        else if (t[0] == '>') add(t + 1, COL_CODE);
        else if (t[0] == '@') {
            add("", COL_TEXT);
            lines[nlines - 1].swatch_row = ++swatch;
        } else if (t[0] == '%') {
            for (int a = 0; a < NUM_API; a++) {
                const char *sig = API[a].sig;
                /* long signatures wrap */
                while ((int)strlen(sig) > COLS) {
                    char part[COLS + 1];
                    snprintf(part, sizeof part, "%.*s", COLS, sig);
                    add(part, COL_CODE);
                    sig += COLS;
                }
                add(sig, COL_CODE);
                char desc[COLS + 1];
                snprintf(desc, sizeof desc, " %s", API[a].desc);
                add(desc, COL_TEXT);
            }
        } else {
            add(t, COL_TEXT);
        }
    }
}

const char *help_signature(const char *name, size_t len) {
    for (int a = 0; a < NUM_API; a++) {
        const char *sig = API[a].sig;
        if (strncmp(sig, name, len) == 0 && sig[len] == '(') return sig;
    }
    return NULL;
}

static void scroll_by(int n) {
    build();
    scroll += n;
    if (scroll > nlines - ROWS) scroll = nlines - ROWS;
    if (scroll < 0) scroll = 0;
}

void help_event(const SDL_Event *ev) {
    if (ev->type == SDL_EVENT_MOUSE_WHEEL) scroll_by(-(int)ev->wheel.y * 3);
    if (ev->type != SDL_EVENT_KEY_DOWN) return;
    switch (ev->key.key) {
    case SDLK_UP: scroll_by(-1); break;
    case SDLK_DOWN: scroll_by(1); break;
    case SDLK_PAGEUP: scroll_by(-ROWS); break;
    case SDLK_PAGEDOWN:
    case SDLK_SPACE: scroll_by(ROWS); break;
    case SDLK_HOME: scroll_by(-nlines); break;
    case SDLK_END: scroll_by(nlines); break;
    default: break;
    }
}

void help_draw(void) {
    build();
    gfx_rectfill(0, EDIT_TOP, SCREEN_W - 1, EDIT_BOTTOM - 1, COL_BG);
    for (int r = 0; r < ROWS && scroll + r < nlines; r++) {
        const Line *l = &lines[scroll + r];
        int y = EDIT_TOP + 2 + r * FONT_H;
        if (l->swatch_row) {
            for (int i = 0; i < 8; i++) {
                int c = (l->swatch_row - 1) * 8 + i;
                int x = 1 + i * 16;
                gfx_rectfill(x, y - 1, x + 14, y + FONT_H - 1, c);
                char num[4];
                snprintf(num, sizeof num, "%d", c);
                gfx_print(num, x + 2, y, c == 4 || c == 5 || c == 11 || c == 12 || c == 13 ? 0 : 12);
            }
        } else {
            gfx_print(l->text, 1, y, l->color);
        }
    }
}

void help_info(char *buf, size_t n) {
    build();
    snprintf(buf, n, "help %d%%  wheel/arrows scroll",
             nlines > ROWS ? scroll * 100 / (nlines - ROWS) : 100);
}
