#ifndef NIBBLE_CART_H
#define NIBBLE_CART_H

#include "gfx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A cart as stored in its file: code plus sprite sheet, flags and map. */
typedef struct {
    char *code;      /* malloc'd, NUL-terminated */
    size_t code_len;
    uint8_t sheet[SHEET_W * SHEET_H];
    uint8_t flags[NUM_SPRITES];
    uint8_t map[MAP_W * MAP_H];
    unsigned version;  /* bumped on every edit */
} Cart;

/* Reads a cart file into c, replacing its contents (version is kept).
   On failure c is unchanged and err is "path:line: message". */
bool cart_read(const char *path, Cart *c, char *err, size_t errlen);

/* Writes code followed by data sections. All-zero sections and trailing
   all-zero rows are left out. */
bool cart_write(const char *path, const Cart *c, char *err, size_t errlen);

/* Replaces the code with a copy of code[0..len). */
bool cart_set_code(Cart *c, const char *code, size_t len);

/* Copies the sprite sheet, flags and map into the live gfx memory. */
void cart_to_gfx(const Cart *c);

void cart_free(Cart *c);

#endif
