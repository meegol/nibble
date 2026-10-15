#include "api.h"
#include "gfx.h"
#include "nib_lua.h"

#include <lauxlib.h>
#include <lualib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_BUTTONS 6
#define FPS         60

static int draw_color = 12;
static int cursor_x, cursor_y;
static int held_frames[NUM_BUTTONS];
static uint64_t frame_count;

void api_reset(void) {
    draw_color = 12;
    cursor_x = cursor_y = 0;
    memset(held_frames, 0, sizeof held_frames);
    frame_count = 0;
}

void api_input(uint8_t held) {
    for (int i = 0; i < NUM_BUTTONS; i++)
        held_frames[i] = (held >> i) & 1 ? held_frames[i] + 1 : 0;
    frame_count++;
}

/* ---- argument helpers ---- */

static int argi(lua_State *L, int i) {
    return (int)floor(luaL_checknumber(L, i));
}

static int opti(lua_State *L, int i, int def) {
    return lua_isnoneornil(L, i) ? def : argi(L, i);
}

/* An explicit color argument also becomes the current draw color. */
static int optcol(lua_State *L, int i) {
    if (!lua_isnoneornil(L, i)) draw_color = argi(L, i) & 15;
    return draw_color;
}

/* ---- graphics ---- */

static int l_cls(lua_State *L) {
    gfx_cls(opti(L, 1, 0));
    cursor_x = cursor_y = 0;
    return 0;
}

static int l_pset(lua_State *L) {
    gfx_pset(argi(L, 1), argi(L, 2), optcol(L, 3));
    return 0;
}

static int l_pget(lua_State *L) {
    lua_pushinteger(L, gfx_pget(argi(L, 1), argi(L, 2)));
    return 1;
}

static int l_line(lua_State *L) {
    gfx_line(argi(L, 1), argi(L, 2), argi(L, 3), argi(L, 4), optcol(L, 5));
    return 0;
}

static int l_rect(lua_State *L) {
    gfx_rect(argi(L, 1), argi(L, 2), argi(L, 3), argi(L, 4), optcol(L, 5));
    return 0;
}

static int l_rectfill(lua_State *L) {
    gfx_rectfill(argi(L, 1), argi(L, 2), argi(L, 3), argi(L, 4), optcol(L, 5));
    return 0;
}

static int l_circ(lua_State *L) {
    gfx_circ(argi(L, 1), argi(L, 2), opti(L, 3, 4), optcol(L, 4));
    return 0;
}

static int l_circfill(lua_State *L) {
    gfx_circfill(argi(L, 1), argi(L, 2), opti(L, 3, 4), optcol(L, 4));
    return 0;
}

/* print(s, [x, y], [col]) -- without x,y prints at the cursor. */
static int l_print(lua_State *L) {
    const char *s = luaL_tolstring(L, 1, NULL);
    if (!lua_isnoneornil(L, 2)) {
        cursor_x = argi(L, 2);
        cursor_y = opti(L, 3, cursor_y);
    }
    cursor_y = gfx_print(s, cursor_x, cursor_y, optcol(L, 4));
    return 0;
}

static int l_color(lua_State *L) {
    draw_color = opti(L, 1, 12) & 15;
    return 0;
}

static int l_camera(lua_State *L) {
    gfx_camera(opti(L, 1, 0), opti(L, 2, 0));
    return 0;
}

static int l_pal(lua_State *L) {
    if (lua_isnoneornil(L, 1)) gfx_pal_reset();
    else gfx_pal(argi(L, 1), argi(L, 2));
    return 0;
}

static int l_palt(lua_State *L) {
    if (lua_isnoneornil(L, 1)) {
        for (int i = 0; i < NUM_COLORS; i++) gfx_palt(i, i == 0);
    } else {
        gfx_palt(argi(L, 1), lua_toboolean(L, 2));
    }
    return 0;
}

/* ---- sprites & map ---- */

/* spr(n, x, y, [w, h], [flip_x], [flip_y]) -- w, h in sprites */
static int l_spr(lua_State *L) {
    int n = argi(L, 1) & 255;
    int w = (int)(luaL_optnumber(L, 4, 1) * SPRITE_SIZE);
    int h = (int)(luaL_optnumber(L, 5, 1) * SPRITE_SIZE);
    gfx_sspr(n % 16 * SPRITE_SIZE, n / 16 * SPRITE_SIZE, w, h, argi(L, 2), argi(L, 3), w, h,
             lua_toboolean(L, 6), lua_toboolean(L, 7));
    return 0;
}

/* sspr(sx, sy, sw, sh, dx, dy, [dw, dh], [flip_x], [flip_y]) */
static int l_sspr(lua_State *L) {
    int sw = argi(L, 3), sh = argi(L, 4);
    gfx_sspr(argi(L, 1), argi(L, 2), sw, sh, argi(L, 5), argi(L, 6),
             opti(L, 7, sw), opti(L, 8, sh), lua_toboolean(L, 9), lua_toboolean(L, 10));
    return 0;
}

static int l_sget(lua_State *L) {
    int x = argi(L, 1), y = argi(L, 2);
    bool in = x >= 0 && y >= 0 && x < SHEET_W && y < SHEET_H;
    lua_pushinteger(L, in ? gfx_sheet[y * SHEET_W + x] : 0);
    return 1;
}

static int l_sset(lua_State *L) {
    int x = argi(L, 1), y = argi(L, 2), c = optcol(L, 3);
    if (x >= 0 && y >= 0 && x < SHEET_W && y < SHEET_H) gfx_sheet[y * SHEET_W + x] = c;
    return 0;
}

/* fget(n, [f]) -- whole byte, or flag f as a boolean */
static int l_fget(lua_State *L) {
    int flags = gfx_flags[argi(L, 1) & 255];
    if (lua_isnoneornil(L, 2)) lua_pushinteger(L, flags);
    else lua_pushboolean(L, (flags >> (argi(L, 2) & 7)) & 1);
    return 1;
}

/* fset(n, [f], v) -- whole byte, or flag f from a boolean */
static int l_fset(lua_State *L) {
    uint8_t *flags = &gfx_flags[argi(L, 1) & 255];
    if (lua_gettop(L) < 3) {
        *flags = argi(L, 2) & 255;
    } else {
        int bit = 1 << (argi(L, 2) & 7);
        *flags = lua_toboolean(L, 3) ? *flags | bit : *flags & ~bit;
    }
    return 0;
}

/* map([cx, cy, sx, sy, cw, ch, layer]) */
static int l_map(lua_State *L) {
    gfx_map_draw(opti(L, 1, 0), opti(L, 2, 0), opti(L, 3, 0), opti(L, 4, 0),
                 opti(L, 5, MAP_W), opti(L, 6, MAP_H), opti(L, 7, 0));
    return 0;
}

static int l_mget(lua_State *L) {
    int x = argi(L, 1), y = argi(L, 2);
    bool in = x >= 0 && y >= 0 && x < MAP_W && y < MAP_H;
    lua_pushinteger(L, in ? gfx_map[y * MAP_W + x] : 0);
    return 1;
}

static int l_mset(lua_State *L) {
    int x = argi(L, 1), y = argi(L, 2);
    if (x >= 0 && y >= 0 && x < MAP_W && y < MAP_H) gfx_map[y * MAP_W + x] = argi(L, 3) & 255;
    return 0;
}

/* ---- input ---- */

static int l_btn(lua_State *L) {
    if (lua_isnoneornil(L, 1)) {
        int mask = 0;
        for (int i = 0; i < NUM_BUTTONS; i++)
            if (held_frames[i]) mask |= 1 << i;
        lua_pushinteger(L, mask);
        return 1;
    }
    int i = argi(L, 1);
    lua_pushboolean(L, i >= 0 && i < NUM_BUTTONS && held_frames[i] > 0);
    return 1;
}

/* True on the first frame, then repeats every 4 frames after 15. */
static int l_btnp(lua_State *L) {
    int i = argi(L, 1);
    int n = (i >= 0 && i < NUM_BUTTONS) ? held_frames[i] : 0;
    lua_pushboolean(L, n == 1 || (n > 15 && (n - 16) % 4 == 0));
    return 1;
}

/* ---- math & misc ---- */

static int l_rnd(lua_State *L) {
    double max = luaL_optnumber(L, 1, 1.0);
    lua_pushnumber(L, (double)rand() / ((double)RAND_MAX + 1.0) * max);
    return 1;
}

static int l_flr(lua_State *L) {
    lua_pushinteger(L, (lua_Integer)floor(luaL_optnumber(L, 1, 0)));
    return 1;
}

static int l_mid(lua_State *L) {
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    double c = luaL_optnumber(L, 3, 0);
    double lo = fmin(a, b), hi = fmax(a, b);
    lua_pushnumber(L, fmax(lo, fmin(hi, c)));
    return 1;
}

static int l_time(lua_State *L) {
    lua_pushnumber(L, (double)frame_count / FPS);
    return 1;
}

static int l_printh(lua_State *L) {
    puts(luaL_tolstring(L, 1, NULL));
    fflush(stdout);
    return 0;
}

static const luaL_Reg API[] = {
    {"cls", l_cls},       {"pset", l_pset},         {"pget", l_pget},
    {"line", l_line},     {"rect", l_rect},         {"rectfill", l_rectfill},
    {"circ", l_circ},     {"circfill", l_circfill}, {"print", l_print},
    {"color", l_color},   {"camera", l_camera},     {"pal", l_pal},
    {"btn", l_btn},       {"btnp", l_btnp},         {"rnd", l_rnd},
    {"flr", l_flr},       {"mid", l_mid},           {"time", l_time},
    {"printh", l_printh}, {"palt", l_palt},         {"spr", l_spr},
    {"sspr", l_sspr},     {"sget", l_sget},         {"sset", l_sset},
    {"fget", l_fget},     {"fset", l_fset},         {"map", l_map},
    {"mget", l_mget},     {"mset", l_mset},         {NULL, NULL},
};

/* ---- cart lifecycle ---- */

static int traceback(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    luaL_traceback(L, L, msg ? msg : "(non-string error)", 1);
    return 1;
}

static void copy_error(lua_State *L, char *err, size_t errlen) {
    const char *msg = lua_tostring(L, -1);
    snprintf(err, errlen, "%s", msg ? msg : "unknown error");
}

bool api_is_function(const char *name, size_t len) {
    for (const luaL_Reg *r = API; r->name; r++)
        if (strlen(r->name) == len && strncmp(r->name, name, len) == 0) return true;
    return false;
}

/* ---- loading ---- */

static bool is_nib(const char *path) {
    size_t n = strlen(path);
    return n >= 4 && strcmp(path + n - 4, ".nib") == 0;
}

/* Runs nib.translate on the code at the top of the stack, replacing it with the Lua
   source. On a translate error fills err with "path:line: message". */
static bool translate_nib(lua_State *L, const char *path, char *err, size_t errlen) {
    int top = lua_gettop(L);
    if (luaL_loadbuffer(L, NIB_LUA, sizeof NIB_LUA - 1, "=nib") != LUA_OK
        || lua_pcall(L, 0, 1, 0) != LUA_OK) {
        copy_error(L, err, errlen);
        lua_settop(L, top - 1);
        return false;
    }
    lua_getfield(L, -1, "translate");
    lua_pushvalue(L, top);
    if (lua_pcall(L, 1, 3, 0) != LUA_OK) {
        copy_error(L, err, errlen);
        lua_settop(L, top - 1);
        return false;
    }
    if (lua_isnil(L, -3)) {
        snprintf(err, errlen, "%s:%d: %s", path, (int)lua_tointeger(L, -2), lua_tostring(L, -1));
        lua_settop(L, top - 1);
        return false;
    }
    lua_pop(L, 2);
    lua_replace(L, top);
    lua_settop(L, top);
    return true;
}

/* Pushes the compiled code; path names the chunk and picks .nib translation. */
static bool load_code(lua_State *L, const char *path, const char *code, size_t len,
                      char *err, size_t errlen) {
    lua_pushlstring(L, code, len);
    if (is_nib(path) && !translate_nib(L, path, err, errlen)) return false;

    size_t lua_len;
    const char *lua_src = lua_tolstring(L, -1, &lua_len);
    lua_pushfstring(L, "@%s", path);
    int status = luaL_loadbuffer(L, lua_src, lua_len, lua_tostring(L, -1));
    if (status != LUA_OK) copy_error(L, err, errlen);
    lua_replace(L, -3);
    lua_pop(L, 1);
    if (status != LUA_OK) lua_pop(L, 1);
    return status == LUA_OK;
}

bool api_check(const char *path, const char *code, size_t len, char *err, size_t errlen) {
    lua_State *L = luaL_newstate();
    if (!L) {
        snprintf(err, errlen, "out of memory");
        return false;
    }
    luaL_openlibs(L);
    bool ok = load_code(L, path, code, len, err, errlen);
    lua_close(L);
    return ok;
}

lua_State *api_load(const char *path, const char *code, size_t len, char *err, size_t errlen) {
    lua_State *L = luaL_newstate();
    if (!L) {
        snprintf(err, errlen, "out of memory");
        return NULL;
    }
    luaL_openlibs(L);
    for (const luaL_Reg *r = API; r->name; r++) lua_register(L, r->name, r->func);

    lua_pushcfunction(L, traceback);
    if (!load_code(L, path, code, len, err, errlen) || lua_pcall(L, 0, 0, -2) != LUA_OK) {
        if (lua_gettop(L) > 1) copy_error(L, err, errlen);
        lua_close(L);
        return NULL;
    }
    lua_pop(L, 1);
    return L;
}

bool api_call(lua_State *L, const char *name, char *err, size_t errlen) {
    lua_pushcfunction(L, traceback);
    if (lua_getglobal(L, name) != LUA_TFUNCTION) {
        lua_pop(L, 2);
        return true;
    }
    if (lua_pcall(L, 0, 0, -2) != LUA_OK) {
        copy_error(L, err, errlen);
        lua_pop(L, 2);
        return false;
    }
    lua_pop(L, 1);
    return true;
}
