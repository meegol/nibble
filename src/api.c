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
    {"printh", l_printh}, {NULL, NULL},
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

/* ---- nibble flavour (.nib) ---- */

static bool is_nib(const char *path) {
    size_t n = strlen(path);
    return n >= 4 && strcmp(path + n - 4, ".nib") == 0;
}

/* Reads a whole file into a malloc'd buffer. */
static char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *buf = NULL;
    size_t cap = 0, n = 0;
    for (;;) {
        if (n == cap) {
            char *grown = realloc(buf, cap = cap ? cap * 2 : 4096);
            if (!grown) { free(buf); fclose(f); return NULL; }
            buf = grown;
        }
        size_t got = fread(buf + n, 1, cap - n, f);
        n += got;
        if (got == 0) break;
    }
    fclose(f);
    *len = n;
    return buf;
}

/* Calls nib.<fn>(file contents, chunkname) and leaves its results on the stack.
   Returns the number of results, or -1 with err filled. */
static int call_nib(lua_State *L, const char *fn, const char *path, char *err, size_t errlen) {
    size_t len;
    char *src = read_file(path, &len);
    if (!src) {
        snprintf(err, errlen, "cannot open %s", path);
        return -1;
    }
    int top = lua_gettop(L);
    if (luaL_loadbuffer(L, NIB_LUA, sizeof NIB_LUA - 1, "=nib") != LUA_OK
        || lua_pcall(L, 0, 1, 0) != LUA_OK) {
        free(src);
        copy_error(L, err, errlen);
        lua_settop(L, top);
        return -1;
    }
    lua_getfield(L, -1, fn);
    lua_remove(L, -2);
    lua_pushlstring(L, src, len);
    free(src);
    lua_pushfstring(L, "@%s", path);
    if (lua_pcall(L, 2, LUA_MULTRET, 0) != LUA_OK) {
        copy_error(L, err, errlen);
        lua_settop(L, top);
        return -1;
    }
    return lua_gettop(L) - top;
}

/* Formats a (line, msg) pair left on the stack by nib as "path:line: msg". */
static void nib_error(lua_State *L, int line_idx, const char *path, char *err, size_t errlen) {
    snprintf(err, errlen, "%s:%d: %s", path, (int)lua_tointeger(L, line_idx),
             lua_tostring(L, line_idx + 1));
}

/* Pushes the compiled chunk for a .nib cart. */
static bool load_nib(lua_State *L, const char *path, char *err, size_t errlen) {
    int top = lua_gettop(L);
    if (call_nib(L, "translate", path, err, errlen) < 0) return false;
    if (lua_isnil(L, top + 1)) {
        nib_error(L, top + 2, path, err, errlen);
        lua_settop(L, top);
        return false;
    }
    size_t len;
    const char *lua_src = lua_tolstring(L, top + 1, &len);
    char chunkname[512];
    snprintf(chunkname, sizeof chunkname, "@%s", path);
    if (luaL_loadbuffer(L, lua_src, len, chunkname) != LUA_OK) {
        copy_error(L, err, errlen);
        lua_settop(L, top);
        return false;
    }
    lua_replace(L, top + 1);
    lua_settop(L, top + 1);
    return true;
}

bool api_check(const char *path, char *err, size_t errlen) {
    lua_State *L = luaL_newstate();
    if (!L) {
        snprintf(err, errlen, "out of memory");
        return false;
    }
    luaL_openlibs(L);
    bool ok;
    if (is_nib(path)) {
        int n = call_nib(L, "check", path, err, errlen);
        ok = n == 0 || (n > 0 && lua_isnil(L, 1));
        if (n > 0 && !ok) nib_error(L, 1, path, err, errlen);
    } else {
        ok = luaL_loadfile(L, path) == LUA_OK;
        if (!ok) copy_error(L, err, errlen);
    }
    lua_close(L);
    return ok;
}

lua_State *api_load(const char *path, char *err, size_t errlen) {
    lua_State *L = luaL_newstate();
    if (!L) {
        snprintf(err, errlen, "out of memory");
        return NULL;
    }
    luaL_openlibs(L);
    for (const luaL_Reg *r = API; r->name; r++) lua_register(L, r->name, r->func);

    lua_pushcfunction(L, traceback);
    if (is_nib(path)) {
        if (!load_nib(L, path, err, errlen)) {
            lua_close(L);
            return NULL;
        }
    } else if (luaL_loadfile(L, path) != LUA_OK) {
        copy_error(L, err, errlen);
        lua_close(L);
        return NULL;
    }
    if (lua_pcall(L, 0, 0, -2) != LUA_OK) {
        copy_error(L, err, errlen);
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
