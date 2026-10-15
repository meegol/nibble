#ifndef NIBBLE_HELP_H
#define NIBBLE_HELP_H

#include <SDL3/SDL.h>
#include <stddef.h>

/* Signature of an API function, e.g. "spr(n,x,y,[w,h],[fx],[fy])", or NULL. */
const char *help_signature(const char *name, size_t len);

void help_event(const SDL_Event *ev);
void help_draw(void);
void help_info(char *buf, size_t n);

#endif
