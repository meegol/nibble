#include "cart.h"
#include "gfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed, failed;
static const char *TMP = "build/cart_test.tmp";

#define CHECK(cond) do { \
    if (cond) passed++; \
    else { failed++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

static void put(const char *s) {
    FILE *f = fopen(TMP, "wb");
    fputs(s, f);
    fclose(f);
}

static char *slurp(void) {
    FILE *f = fopen(TMP, "rb");
    static char buf[1 << 16];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

/* Reads TMP into cart, returns its code or NULL with err set. */
static char err[256];
static Cart cart;
static const char *read_code(void) {
    if (!cart_read(TMP, &cart, err, sizeof err)) return NULL;
    return cart.code;
}

static void expect_error(const char *file, const char *msg) {
    put(file);
    const char *code = read_code();
    char want[256];
    snprintf(want, sizeof want, "%s:%s", TMP, msg);
    CHECK(code == NULL);
    if (code == NULL && strcmp(err, want) != 0) {
        failed++;
        printf("FAIL error\n  want: %s\n  got:  %s\n", want, err);
    }
}

int main(void) {
    const char *code;

    /* no sections */
    put("x = 1\n__gfx__\n5\n");
    read_code();
    put("x = 1\ny = 2\n");
    code = read_code();
    CHECK(code && strcmp(code, "x = 1\ny = 2\n") == 0);
    CHECK(cart.sheet[0] == 0);

    /* sections */
    put("x = 1\n__gfx__\n0123\n\nf\n__gff__\n0001ff\n__map__\n0a\n00000b\n");
    code = read_code();
    CHECK(code && strcmp(code, "x = 1\n") == 0);
    CHECK(cart.sheet[0] == 0 && cart.sheet[1] == 1 && cart.sheet[2] == 2 && cart.sheet[3] == 3);
    CHECK(cart.sheet[4] == 0);
    CHECK(cart.sheet[1 * SHEET_W] == 0);
    CHECK(cart.sheet[2 * SHEET_W] == 15);
    CHECK(cart.flags[0] == 0 && cart.flags[1] == 1 && cart.flags[2] == 255);
    CHECK(cart.map[0] == 10);
    CHECK(cart.map[MAP_W + 2] == 11);

    /* second gff line is sprites 128..255 */
    put("__gff__\n00\n07\n");
    code = read_code();
    CHECK(code && strcmp(code, "") == 0);
    CHECK(cart.flags[128] == 7);

    /* CRLF */
    put("x = 1\r\n__gfx__\r\n7\r\n");
    code = read_code();
    CHECK(code && strcmp(code, "x = 1\r\n") == 0);
    CHECK(cart.sheet[0] == 7);

    /* write then read round trip */
    memset(cart.sheet, 0, sizeof cart.sheet);
    memset(cart.flags, 0, sizeof cart.flags);
    memset(cart.map, 0, sizeof cart.map);
    cart.sheet[5] = 9;
    cart.sheet[10 * SHEET_W + 127] = 15;
    cart.flags[200] = 0x81;
    cart.map[63 * MAP_W + 127] = 0xfe;
    cart_set_code(&cart, "def f():\n  x()", 14);
    CHECK(cart_write(TMP, &cart, err, sizeof err));
    code = read_code();
    CHECK(code && strcmp(code, "def f():\n  x()\n") == 0);
    CHECK(cart.sheet[5] == 9 && cart.sheet[10 * SHEET_W + 127] == 15);
    CHECK(cart.flags[200] == 0x81);
    CHECK(cart.map[63 * MAP_W + 127] == 0xfe);
    {
        /* trailing zero rows are omitted: gfx has 11 rows */
        const char *s = slurp();
        const char *g = strstr(s, "__gfx__\n"), *f = strstr(s, "__gff__\n");
        int rows = 0;
        for (const char *p = g; p && p < f; p++) rows += *p == '\n';
        CHECK(rows == 1 + 11);
    }

    /* all-zero memory writes no sections */
    memset(cart.sheet, 0, sizeof cart.sheet);
    memset(cart.flags, 0, sizeof cart.flags);
    memset(cart.map, 0, sizeof cart.map);
    cart_set_code(&cart, "x = 1\n", 6);
    CHECK(cart_write(TMP, &cart, err, sizeof err));
    CHECK(strcmp(slurp(), "x = 1\n") == 0);

    /* errors */
    expect_error("x\n__gfx__\n01g\n", "3: bad hex digit in __gfx__");
    expect_error("__gff__\n0\n0\n0\n", "4: too many lines in __gff__");
    expect_error("__map__\n" "0000000000000000000000000000000000000000000000000000000000000000"
                 "0000000000000000000000000000000000000000000000000000000000000000"
                 "0000000000000000000000000000000000000000000000000000000000000000"
                 "0000000000000000000000000000000000000000000000000000000000000000" "00\n",
                 "2: line too long in __map__");
    expect_error("x\n__foo__\n", "2: unknown section __foo__");
    put("x\n");
    remove(TMP);
    CHECK(read_code() == NULL);

    /* a failed read leaves the cart unchanged */
    put("y = 2\n__gfx__\n5\n");
    read_code();
    expect_error("z\n__gfx__\nq\n", "3: bad hex digit in __gfx__");
    CHECK(strcmp(cart.code, "y = 2\n") == 0 && cart.sheet[0] == 5);

    /* version survives a read; cart_to_gfx copies data */
    cart.version = 7;
    put("w\n__gfx__\n6\n");
    read_code();
    CHECK(cart.version == 7);
    cart_to_gfx(&cart);
    CHECK(gfx_sheet[0] == 6);
    cart_free(&cart);

    printf("ok %d, failed %d\n", passed, failed);
    return failed ? 1 : 0;
}
