#include "carts_dir.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static bool is_dir(const char *path) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

typedef struct {
    const char *from, *to;
} Copy;

/* Copies the tree under c->from to c->to. */
static SDL_EnumerationResult SDLCALL copy_entry(void *userdata, const char *dir, const char *name) {
    const Copy *c = userdata;
    char src[1024], dst[1024];
    size_t n = strlen(dir);
    snprintf(src, sizeof src, "%s%s%s", dir, n && dir[n - 1] == '/' ? "" : "/", name);
    snprintf(dst, sizeof dst, "%s%s", c->to, src + strlen(c->from));
    if (is_dir(src)) {
        SDL_CreateDirectory(dst);
        SDL_EnumerateDirectory(src, copy_entry, userdata);
    } else {
        SDL_CopyFile(src, dst);
    }
    return SDL_ENUM_CONTINUE;
}

const char *carts_dir(void) {
    static char dir[1024];
    if (dir[0]) return dir;

    const char *base = SDL_GetBasePath();  /* ends with '/' */
    if (base) {
        snprintf(dir, sizeof dir, "%scarts", base);
        if (is_dir(dir)) return dir;
    }

    char *pref = SDL_GetPrefPath(NULL, "nibble");
    if (!pref) {
        snprintf(dir, sizeof dir, "carts");
        return dir;
    }
    snprintf(dir, sizeof dir, "%scarts", pref);
    SDL_free(pref);

    if (!is_dir(dir)) {
        SDL_CreateDirectory(dir);
        /* example carts installed by `make install`: <prefix>/share/nibble/examples */
        char bundled[1024];
        snprintf(bundled, sizeof bundled, "%s../share/nibble/examples", base ? base : "");
        if (is_dir(bundled)) {
            Copy c = {bundled, dir};
            SDL_EnumerateDirectory(bundled, copy_entry, &c);
        }
    }
    return dir;
}
