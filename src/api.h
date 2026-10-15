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

/* Creates a Lua state with the nibble API and runs a cart's code. path names the
   chunk in error messages, and a .nib path means the code is nibble flavour.
   Returns NULL and fills err on failure. */
lua_State *api_load(const char *path, const char *code, size_t len, char *err, size_t errlen);

/* Compiles a cart's code without running it.
   Returns false and fills err with "path:line: message" on an error. */
bool api_check(const char *path, const char *code, size_t len, char *err, size_t errlen);

/* Is name[0..len) one of the built-in API functions (cls, spr, ...)? */
bool api_is_function(const char *name, size_t len);

/* Calls a global function if the cart defines it.
   Returns false and fills err (with traceback) on a Lua error. */
bool api_call(lua_State *L, const char *name, char *err, size_t errlen);

#endif
