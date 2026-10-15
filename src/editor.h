#ifndef NIBBLE_EDITOR_H
#define NIBBLE_EDITOR_H

#include "cart.h"

#include <SDL3/SDL.h>

/* Editor area: rows EDIT_TOP..EDIT_BOTTOM-1. Above is the top bar, below the status bar. */
#define EDIT_TOP    7
#define EDIT_BOTTOM 121

/* saved_version: the cart version last written to disk, for the unsaved marker. */
void editor_init(Cart *cart, const char *path, SDL_Window *win, const unsigned *saved_version);

/* Called when switching into / out of edit mode. */
void editor_enter(void);
void editor_leave(void);

/* The cart was replaced (e.g. reloaded from disk); editors drop cached state. */
void editor_cart_changed(void);

/* Handles an event; mouse coordinates are already in screen pixels. */
void editor_event(const SDL_Event *ev);

void editor_draw(void);

/* Shows a message on the status bar for a few seconds. */
void editor_status(const char *fmt, ...);

/* Jumps to the code editor at a line (1-based), e.g. after an error. */
void editor_goto_line(int line);

#endif
