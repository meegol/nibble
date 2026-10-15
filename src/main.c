#include "api.h"
#include "cart.h"
#include "editor.h"
#include "gfx.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <lua.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCALE        5
#define STEP_NS      (SDL_NS_PER_SECOND / 60)
#define MAX_CATCHUP  4
#define ERR_COLS     (SCREEN_W / FONT_W - 1)

enum { MODE_RUN, MODE_EDIT };

static const char *cart_path;
static Cart cart;
static unsigned saved_version;  /* cart.version when last read or written */
static unsigned run_version;    /* cart.version the running game started from */
static int mode;
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

static bool unsaved(void) {
    return cart.version != saved_version;
}

/* Starts the game from the cart in memory. */
static void run(void) {
    if (L) lua_close(L);
    L = NULL;
    gfx_reset();
    api_reset();
    cart_to_gfx(&cart);
    failed = false;
    run_version = cart.version;

    L = api_load(cart_path, cart.code, cart.code_len, err, sizeof err);
    if (!L || !api_call(L, "_init", err, sizeof err)) fail();
}

/* Reads the cart from disk. Returns false (with the error shown) on a bad file. */
static bool load_file(void) {
    cart_mtime = file_mtime(cart_path);
    if (!cart_read(cart_path, &cart, err, sizeof err)) {
        fail();
        return false;
    }
    cart.version++;
    saved_version = cart.version;
    editor_cart_changed();
    return true;
}

static void save(void) {
    char msg[256];
    if (cart_write(cart_path, &cart, msg, sizeof msg)) {
        saved_version = cart.version;
        cart_mtime = file_mtime(cart_path);
        editor_status("saved");
    } else {
        editor_status("%s", msg);
    }
}

static void set_mode(int m) {
    mode = m;
    if (m == MODE_EDIT) editor_enter();
    else editor_leave();
}

/* Line number of the first "path:LINE:" in the error, or 0. */
static int error_line(void) {
    size_t n = strlen(cart_path);
    for (const char *p = strstr(err, cart_path); p; p = strstr(p + 1, cart_path))
        if (p[n] == ':' && p[n + 1] >= '0' && p[n + 1] <= '9') return atoi(p + n + 1);
    return 0;
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
    gfx_print("ERROR - ESC: EDIT  CTRL+R: RUN", 0, 0, 2);
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

/* Handles one event. Returns false to quit. */
static bool handle_event(SDL_Renderer *ren, SDL_Window *win, SDL_Event *ev, bool *quit_warned) {
    if (ev->type == SDL_EVENT_QUIT) {
        if (!unsaved() || *quit_warned) return false;
        *quit_warned = true;
        set_mode(MODE_EDIT);
        editor_status("unsaved! ctrl+s, or close again");
        return true;
    }

    if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
        bool ctrl = ev->key.mod & SDL_KMOD_CTRL;
        if (ev->key.key == SDLK_F11) {
            SDL_SetWindowFullscreen(win, !(SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN));
            return true;
        }
        if (ctrl && ev->key.key == SDLK_S) {
            save();
            return true;
        }
        if (ctrl && ev->key.key == SDLK_R) {
            run();
            set_mode(MODE_RUN);
            return true;
        }
        if (ev->key.key == SDLK_ESCAPE) {
            if (mode == MODE_RUN) {
                set_mode(MODE_EDIT);
                if (failed) {
                    if (error_line() > 0) editor_goto_line(error_line());
                    /* the message without "path:line: " or the traceback */
                    const char *msg = strstr(err, ": ");
                    msg = msg && error_line() > 0 ? msg + 2 : err;
                    editor_status("%.*s", (int)strcspn(msg, "\n"), msg);
                }
            } else {
                if (cart.version != run_version || failed) run();
                set_mode(MODE_RUN);
            }
            return true;
        }
    }

    if (mode == MODE_EDIT) {
        SDL_ConvertEventToRenderCoordinates(ren, ev);
        editor_event(ev);
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2 && strcmp(argv[1], "--check") == 0) {
        Cart c = {0};
        if (cart_read(argv[2], &c, err, sizeof err)
            && api_check(argv[2], c.code, c.code_len, err, sizeof err)) {
            cart_free(&c);
            return 0;
        }
        cart_free(&c);
        puts(err);
        return 1;
    }

    cart_path = argc > 1 ? argv[1] : "carts/demo.nib";

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
    cart_set_code(&cart, "", 0);
    editor_init(&cart, cart_path, win, &saved_version);

    /* A missing file starts a new cart in the editor. */
    if (file_mtime(cart_path) == 0) {
        set_mode(MODE_EDIT);
        editor_status("new cart - ctrl+s to save");
    } else {
        if (load_file()) run();
        set_mode(MODE_RUN);
    }

    static uint32_t pixels[SCREEN_W * SCREEN_H];
    Uint64 prev = SDL_GetTicksNS(), acc = STEP_NS, last_check = 0;
    bool running = true, quit_warned = false;

    while (running) {
        SDL_Event ev;
        while (running && SDL_PollEvent(&ev)) running = handle_event(ren, win, &ev, &quit_warned);

        Uint64 now = SDL_GetTicksNS();
        acc += now - prev;
        prev = now;
        if (acc > STEP_NS * MAX_CATCHUP) acc = STEP_NS * MAX_CATCHUP;

        /* Hot reload when the cart file changes on disk. */
        if (now - last_check > SDL_NS_PER_SECOND / 2) {
            last_check = now;
            SDL_Time mtime = file_mtime(cart_path);
            if (mtime != 0 && mtime != cart_mtime) {
                if (unsaved()) {
                    cart_mtime = mtime;
                    editor_status("file changed on disk; not reloaded");
                } else if (load_file()) {
                    if (mode == MODE_RUN) run();
                    else editor_status("reloaded");
                }
            }
        }

        bool ticked = false;
        while (acc >= STEP_NS) {
            acc -= STEP_NS;
            ticked = true;
            if (mode != MODE_RUN || failed) continue;
            api_input(read_buttons());
            if (!api_call(L, "_update", err, sizeof err)) fail();
        }
        if (!ticked) {
            SDL_DelayNS(SDL_NS_PER_MS);
            continue;
        }

        if (mode == MODE_EDIT) {
            editor_draw();
        } else {
            if (!failed && !api_call(L, "_draw", err, sizeof err)) fail();
            if (failed) draw_error();
        }

        gfx_to_rgb(pixels);
        SDL_UpdateTexture(tex, NULL, pixels, SCREEN_W * sizeof pixels[0]);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderTexture(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }

    if (L) lua_close(L);
    cart_free(&cart);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
