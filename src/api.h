#ifndef NIBBLE_API_H
#define NIBBLE_API_H

#include <lua.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Clears draw color, cursor, input and frame counter. */
void api_reset(void);

/* Feeds one frame of button state (bit i = button i held). */
void api_input(uint8_t held);

/* Creates a Lua state with the nibble API and runs the cart file.
   Returns NULL and fills err on failure. */
lua_State *api_load(const char *path, char *err, size_t errlen);

/* Calls a global function if the cart defines it.
   Returns false and fills err (with traceback) on a Lua error. */
bool api_call(lua_State *L, const char *name, char *err, size_t errlen);

#endif
