#ifndef NIBBLE_MENU_H
#define NIBBLE_MENU_H

#include <SDL3/SDL.h>

/* Cart picker: lists .nib and .lua files under dir (recursively) plus a "new cart" entry. */
void menu_open(const char *dir);

/* Handles an event (mouse in screen coordinates). Returns the path of the chosen cart
   (valid until the next menu_open), or NULL. */
const char *menu_event(const SDL_Event *ev);

void menu_draw(void);

#endif
