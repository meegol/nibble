#ifndef NIBBLE_CART_H
#define NIBBLE_CART_H

#include <stdbool.h>
#include <stddef.h>

/* Reads a cart file: fills the sprite sheet, flags and map from its data sections
   (zeroing them first) and returns the code part (malloc'd, not NUL-terminated).
   Returns NULL and fills err with "path:line: message" on failure. */
char *cart_read(const char *path, size_t *code_len, char *err, size_t errlen);

/* Writes code followed by data sections for the current sprite sheet, flags and map.
   All-zero sections and trailing all-zero rows are left out. */
bool cart_write(const char *path, const char *code, size_t code_len, char *err, size_t errlen);

#endif
