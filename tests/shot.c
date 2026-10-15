/* Renders the editor (or, with cart "menu", the cart menu over ./carts) after a scripted list of input steps, saved as a 4x PNG.
   Usage: shot cart.nib out.png [step...]
   Steps: key:NAME  ctrl+key:NAME  shift+key:NAME  text:STRING  click:X,Y  rclick:X,Y
          drag:X0,Y0,X1,Y1  wheel:N  frames:N (draw N frames) */
#include "cart.h"
#include "editor.h"
#include "gfx.h"
#include "menu.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZOOM 4

static void key(SDL_Keycode k, SDL_Keymod mod) {
    SDL_Event ev = {0};
    ev.type = SDL_EVENT_KEY_DOWN;
    ev.key.key = k;
    ev.key.mod = mod;
    ev.key.scancode = SDL_GetScancodeFromKey(k, NULL);
    editor_event(&ev);
    ev.type = SDL_EVENT_KEY_UP;
    editor_event(&ev);
}

static void mouse(Uint32 type, float x, float y, Uint8 button) {
    SDL_Event ev = {0};
    ev.type = type;
    if (type == SDL_EVENT_MOUSE_MOTION) {
        ev.motion.x = x;
        ev.motion.y = y;
        ev.motion.state = button ? SDL_BUTTON_MASK(button) : 0;
    } else {
        ev.button.x = x;
        ev.button.y = y;
        ev.button.button = button;
        ev.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    }
    editor_event(&ev);
}

static void step(const char *s) {
    float a, b, c, d;
    int n;
    if (strncmp(s, "key:", 4) == 0) key(SDL_GetKeyFromName(s + 4), 0);
    else if (strncmp(s, "ctrl+key:", 9) == 0) key(SDL_GetKeyFromName(s + 9), SDL_KMOD_LCTRL);
    else if (strncmp(s, "shift+key:", 10) == 0) key(SDL_GetKeyFromName(s + 10), SDL_KMOD_LSHIFT);
    else if (strncmp(s, "text:", 5) == 0) {
        SDL_Event ev = {0};
        ev.type = SDL_EVENT_TEXT_INPUT;
        ev.text.text = s + 5;
        editor_event(&ev);
    } else if (sscanf(s, "click:%f,%f", &a, &b) == 2) {
        mouse(SDL_EVENT_MOUSE_MOTION, a, b, 0);
        mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, a, b, SDL_BUTTON_LEFT);
        mouse(SDL_EVENT_MOUSE_BUTTON_UP, a, b, SDL_BUTTON_LEFT);
    } else if (sscanf(s, "rclick:%f,%f", &a, &b) == 2) {
        mouse(SDL_EVENT_MOUSE_MOTION, a, b, 0);
        mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, a, b, SDL_BUTTON_RIGHT);
        mouse(SDL_EVENT_MOUSE_BUTTON_UP, a, b, SDL_BUTTON_RIGHT);
    } else if (sscanf(s, "drag:%f,%f,%f,%f", &a, &b, &c, &d) == 4) {
        mouse(SDL_EVENT_MOUSE_MOTION, a, b, 0);
        mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, a, b, SDL_BUTTON_LEFT);
        for (int i = 1; i <= 8; i++)
            mouse(SDL_EVENT_MOUSE_MOTION, a + (c - a) * i / 8, b + (d - b) * i / 8, SDL_BUTTON_LEFT);
        mouse(SDL_EVENT_MOUSE_BUTTON_UP, c, d, SDL_BUTTON_LEFT);
    } else if (sscanf(s, "wheel:%d", &n) == 1) {
        SDL_Event ev = {0};
        ev.type = SDL_EVENT_MOUSE_WHEEL;
        ev.wheel.y = (float)n;
        editor_event(&ev);
    } else if (sscanf(s, "frames:%d", &n) == 1) {
        while (n-- > 0) editor_draw();
    } else {
        fprintf(stderr, "bad step: %s\n", s);
        exit(1);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: shot cart out.png [step...]\n");
        return 1;
    }
    static Cart cart;
    static unsigned saved;
    char err[256];
    cart_set_code(&cart, "", 0);
    if (!cart_read(argv[1], &cart, err, sizeof err)) fprintf(stderr, "%s\n", err);
    if (strcmp(argv[1], "menu") == 0) {
        menu_open("carts");
        for (int i = 3; i < argc; i++) {
            SDL_Event ev = {0};
            ev.type = SDL_EVENT_KEY_DOWN;
            ev.key.key = SDL_GetKeyFromName(argv[i]);
            const char *chosen = menu_event(&ev);
            if (chosen) printf("chose %s\n", chosen);
        }
        menu_draw();
    } else {
        editor_init(&cart, argv[1], NULL, &saved);
        editor_enter();
        editor_cart_changed();
        gfx_reset();
        for (int i = 3; i < argc; i++) step(argv[i]);
        editor_draw();
    }

    static uint32_t px[SCREEN_W * SCREEN_H];
    gfx_to_rgb(px);
    SDL_Surface *s = SDL_CreateSurface(SCREEN_W * ZOOM, SCREEN_H * ZOOM, SDL_PIXELFORMAT_XRGB8888);
    for (int y = 0; y < SCREEN_H * ZOOM; y++)
        for (int x = 0; x < SCREEN_W * ZOOM; x++)
            ((uint32_t *)((uint8_t *)s->pixels + y * s->pitch))[x] = px[y / ZOOM * SCREEN_W + x / ZOOM];
    if (!SDL_SavePNG(s, argv[2])) {
        fprintf(stderr, "SDL_SavePNG: %s\n", SDL_GetError());
        return 1;
    }
    SDL_DestroySurface(s);
    if (getenv("SHOT_CODE")) fputs(cart.code, stdout);
    if (getenv("SHOT_SAVE") && !cart_write(getenv("SHOT_SAVE"), &cart, err, sizeof err))
        fprintf(stderr, "%s\n", err);
    cart_free(&cart);
    return 0;
}
