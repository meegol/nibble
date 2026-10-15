#ifndef NIBBLE_CODE_EDIT_H
#define NIBBLE_CODE_EDIT_H

#include "cart.h"

#include <SDL3/SDL.h>
#include <stddef.h>

void code_init(Cart *cart, const char *path);
void code_reload(void);
void code_event(const SDL_Event *ev);
void code_draw(void);
void code_info(char *buf, size_t n);
void code_goto_line(int line);

#endif
