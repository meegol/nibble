#include "cart.h"
#include "gfx.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A data section: rows of hex digits, `digits` per value. */
typedef struct {
    const char *name;
    size_t offset;  /* of its array in Cart */
    int rows, row_values, digits;
} Section;

static const Section sections[] = {
    {"__gfx__", offsetof(Cart, sheet), SHEET_H, SHEET_W, 1},
    {"__gff__", offsetof(Cart, flags), 2, NUM_SPRITES / 2, 2},
    {"__map__", offsetof(Cart, map), MAP_H, MAP_W, 2},
};

static uint8_t *mem(Cart *c, const Section *sec) {
    return (uint8_t *)c + sec->offset;
}
#define NUM_SECTIONS (int)(sizeof sections / sizeof sections[0])

static char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *buf = NULL;
    size_t cap = 0, n = 0;
    for (;;) {
        if (n == cap) {
            char *grown = realloc(buf, cap = cap ? cap * 2 : 4096);
            if (!grown) { free(buf); fclose(f); return NULL; }
            buf = grown;
        }
        size_t got = fread(buf + n, 1, cap - n, f);
        n += got;
        if (got == 0) break;
    }
    fclose(f);
    *len = n;
    return buf;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* A line like __name__ (letters only between the underscores). */
static bool is_marker(const char *s, size_t n) {
    if (n < 5 || strncmp(s, "__", 2) != 0 || strncmp(s + n - 2, "__", 2) != 0) return false;
    for (size_t i = 2; i < n - 2; i++)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z'))) return false;
    return true;
}

/* Parses buf into c (whose data must start zeroed). Returns false with err set. */
static bool parse(const char *path, const char *buf, size_t len, Cart *c,
                  char *err, size_t errlen) {
    c->code_len = len;
    const Section *sec = NULL;
    int row = 0, lineno = 0;
    for (size_t pos = 0; pos < len; lineno++) {
        size_t start = pos, end = pos;
        while (end < len && buf[end] != '\n') end++;
        pos = end + 1;
        size_t n = end - start;
        if (n > 0 && buf[start + n - 1] == '\r') n--;
        const char *line = buf + start;

        if (is_marker(line, n)) {
            if (!sec) c->code_len = start;
            sec = NULL;
            for (int i = 0; i < NUM_SECTIONS; i++)
                if (strlen(sections[i].name) == n && strncmp(line, sections[i].name, n) == 0)
                    sec = &sections[i];
            if (!sec) {
                snprintf(err, errlen, "%s:%d: unknown section %.*s", path, lineno + 1, (int)n, line);
                return false;
            }
            row = 0;
            continue;
        }
        if (!sec) continue;

        if (row >= sec->rows) {
            snprintf(err, errlen, "%s:%d: too many lines in %s", path, lineno + 1, sec->name);
            return false;
        }
        if (n > (size_t)(sec->row_values * sec->digits)) {
            snprintf(err, errlen, "%s:%d: line too long in %s", path, lineno + 1, sec->name);
            return false;
        }
        for (size_t i = 0; i < n; i++) {
            if (hexval(line[i]) < 0) {
                snprintf(err, errlen, "%s:%d: bad hex digit in %s", path, lineno + 1, sec->name);
                return false;
            }
        }
        uint8_t *dst = mem(c, sec) + row * sec->row_values;
        for (size_t i = 0; i + sec->digits <= n; i += sec->digits) {
            int v = 0;
            for (int d = 0; d < sec->digits; d++) v = v * 16 + hexval(line[i + d]);
            dst[i / sec->digits] = (uint8_t)v;
        }
        row++;
    }
    return true;
}

bool cart_read(const char *path, Cart *c, char *err, size_t errlen) {
    size_t len;
    char *buf = read_file(path, &len);
    if (!buf) {
        snprintf(err, errlen, "cannot open %s", path);
        return false;
    }
    Cart *tmp = calloc(1, sizeof *tmp);
    if (!tmp || !parse(path, buf, len, tmp, err, errlen)) {
        if (!tmp) snprintf(err, errlen, "out of memory");
        free(tmp);
        free(buf);
        return false;
    }
    buf[tmp->code_len] = '\0';  /* buf has room: code_len <= len < capacity */
    char *code = realloc(buf, tmp->code_len + 1);
    tmp->code = code ? code : buf;
    tmp->version = c->version;
    free(c->code);
    *c = *tmp;
    free(tmp);
    return true;
}

bool cart_set_code(Cart *c, const char *code, size_t len) {
    char *copy = malloc(len + 1);
    if (!copy) return false;
    memcpy(copy, code, len);
    copy[len] = '\0';
    free(c->code);
    c->code = copy;
    c->code_len = len;
    return true;
}

void cart_to_gfx(const Cart *c) {
    memcpy(gfx_sheet, c->sheet, sizeof gfx_sheet);
    memcpy(gfx_flags, c->flags, sizeof gfx_flags);
    memcpy(gfx_map, c->map, sizeof gfx_map);
}

void cart_free(Cart *c) {
    free(c->code);
    c->code = NULL;
    c->code_len = 0;
}

bool cart_write(const char *path, const Cart *c, char *err, size_t errlen) {
    const char *code = c->code ? c->code : "";
    size_t code_len = c->code_len;
    FILE *f = fopen(path, "wb");
    if (!f) {
        snprintf(err, errlen, "cannot write %s", path);
        return false;
    }
    fwrite(code, 1, code_len, f);
    if (code_len > 0 && code[code_len - 1] != '\n') fputc('\n', f);

    for (int s = 0; s < NUM_SECTIONS; s++) {
        const Section *sec = &sections[s];
        const uint8_t *m = (const uint8_t *)c + sec->offset;
        int last = -1;  /* last row with any non-zero value */
        for (int r = 0; r < sec->rows; r++)
            for (int i = 0; i < sec->row_values; i++)
                if (m[r * sec->row_values + i]) last = r;
        if (last < 0) continue;

        fprintf(f, "%s\n", sec->name);
        for (int r = 0; r <= last; r++) {
            for (int i = 0; i < sec->row_values; i++)
                fprintf(f, sec->digits == 1 ? "%x" : "%02x", m[r * sec->row_values + i]);
            fputc('\n', f);
        }
    }

    bool ok = !ferror(f);
    if (fclose(f) != 0) ok = false;
    if (!ok) snprintf(err, errlen, "cannot write %s", path);
    return ok;
}
