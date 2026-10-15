#ifndef NIBBLE_SPRITE_EDIT_H
#define NIBBLE_SPRITE_EDIT_H

#include "cart.h"

#include <SDL3/SDL.h>
#include <stddef.h>

void sprite_init(Cart *cart);
void sprite_event(const SDL_Event *ev);
void sprite_draw(void);
void sprite_info(char *buf, size_t n);

#endif
