#ifndef NIBBLE_CARTS_DIR_H
#define NIBBLE_CARTS_DIR_H

/* The folder the cart menu lists. Next to the binary when running from the source tree
   (./carts); otherwise the user's data folder (~/.local/share/nibble/carts), which is
   filled with the bundled example carts on first use. */
const char *carts_dir(void);

#endif
