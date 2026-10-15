#include "api.h"
#include "gfx.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <lua.h>
#include <stdio.h>
#include <stdlib.h>

#define SCALE        5
#define STEP_NS      (SDL_NS_PER_SECOND / 60)
#define MAX_CATCHUP  4
#define ERR_COLS     (SCREEN_W / FONT_W - 1)

static const char *cart_path;
static lua_State *L;
static bool failed;
static char err[4096];
static SDL_Time cart_mtime;

static SDL_Time file_mtime(const char *path) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) ? info.modify_time : 0;
}

static void fail(void) {
    failed = true;
    fprintf(stderr, "%s\n", err);
}

static void cart_load(void) {
    if (L) lua_close(L);
    gfx_reset();
    api_reset();
    failed = false;
    cart_mtime = file_mtime(cart_path);

    L = api_load(cart_path, err, sizeof err);
    if (!L || !api_call(L, "_init", err, sizeof err)) fail();
}

/* Shows the error message hard-wrapped to the screen width. */
static void draw_error(void) {
    char wrapped[sizeof err * 2];
    size_t n = 0;
    int col = 0;
    for (const char *s = err; *s && n < sizeof wrapped - 2; s++) {
        char c = *s == '\t' ? ' ' : *s;
        if (c == '\n' || col == ERR_COLS) {
            wrapped[n++] = '\n';
            col = 0;
            if (c == '\n') continue;
        }
        wrapped[n++] = c;
        col++;
    }
    wrapped[n] = '\0';

    gfx_reset();
    gfx_cls(0);
    gfx_print("ERROR - SAVE OR CTRL+R TO RELOAD", 0, 0, 2);
    gfx_print(wrapped, 0, FONT_H * 2, 12);
}

/* Buttons: 0 left, 1 right, 2 up, 3 down, 4 O, 5 X */
static uint8_t read_buttons(void) {
    const bool *k = SDL_GetKeyboardState(NULL);
    uint8_t b = 0;
    if (k[SDL_SCANCODE_LEFT]  || k[SDL_SCANCODE_A]) b |= 1 << 0;
    if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) b |= 1 << 1;
    if (k[SDL_SCANCODE_UP]    || k[SDL_SCANCODE_W]) b |= 1 << 2;
    if (k[SDL_SCANCODE_DOWN]  || k[SDL_SCANCODE_S]) b |= 1 << 3;
    if (k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_C] || k[SDL_SCANCODE_N]) b |= 1 << 4;
    if (k[SDL_SCANCODE_X] || k[SDL_SCANCODE_V] || k[SDL_SCANCODE_M]) b |= 1 << 5;
    return b;
}

int main(int argc, char **argv) {
    cart_path = argc > 1 ? argv[1] : "carts/demo.lua";

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    char title[256];
    snprintf(title, sizeof title, "nibble - %s", cart_path);

    SDL_Window *win;
    SDL_Renderer *ren;
    if (!SDL_CreateWindowAndRenderer(title, SCREEN_W * SCALE, SCREEN_H * SCALE,
                                     SDL_WINDOW_RESIZABLE, &win, &ren)) {
        fprintf(stderr, "SDL_CreateWindowAndRenderer: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(ren, 1);
    SDL_SetRenderLogicalPresentation(ren, SCREEN_W, SCREEN_H,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888,
                                         SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);

    srand((unsigned)SDL_GetTicksNS());
    cart_load();

    static uint32_t pixels[SCREEN_W * SCREEN_H];
    Uint64 prev = SDL_GetTicksNS(), acc = STEP_NS, last_check = 0;
    bool running = true;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) running = false;
            if (ev.type != SDL_EVENT_KEY_DOWN || ev.key.repeat) continue;
            if (ev.key.key == SDLK_ESCAPE) running = false;
            if (ev.key.key == SDLK_R && (ev.key.mod & SDL_KMOD_CTRL)) cart_load();
            if (ev.key.key == SDLK_F11)
                SDL_SetWindowFullscreen(win, !(SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN));
        }

        Uint64 now = SDL_GetTicksNS();
        acc += now - prev;
        prev = now;
        if (acc > STEP_NS * MAX_CATCHUP) acc = STEP_NS * MAX_CATCHUP;

        /* Hot reload when the cart file changes on disk. */
        if (now - last_check > SDL_NS_PER_SECOND / 2) {
            last_check = now;
            if (file_mtime(cart_path) != cart_mtime) cart_load();
        }

        bool ticked = false;
        while (acc >= STEP_NS) {
            acc -= STEP_NS;
            ticked = true;
            if (failed) continue;
            api_input(read_buttons());
            if (!api_call(L, "_update", err, sizeof err)) fail();
        }
        if (!ticked) {
            SDL_DelayNS(SDL_NS_PER_MS);
            continue;
        }

        if (!failed && !api_call(L, "_draw", err, sizeof err)) fail();
        if (failed) draw_error();

        gfx_to_rgb(pixels);
        SDL_UpdateTexture(tex, NULL, pixels, SCREEN_W * sizeof pixels[0]);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderTexture(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }

    if (L) lua_close(L);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
