#include "cart.h"
#include "gfx.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A data section: rows of hex digits, `digits` per value. */
typedef struct {
    const char *name;
    uint8_t *mem;
    int rows, row_values, digits;
} Section;

static Section sections[] = {
    {"__gfx__", gfx_sheet, SHEET_H, SHEET_W, 1},
    {"__gff__", gfx_flags, 2, NUM_SPRITES / 2, 2},
    {"__map__", gfx_map, MAP_H, MAP_W, 2},
};
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

char *cart_read(const char *path, size_t *code_len, char *err, size_t errlen) {
    memset(gfx_sheet, 0, sizeof gfx_sheet);
    memset(gfx_flags, 0, sizeof gfx_flags);
    memset(gfx_map, 0, sizeof gfx_map);

    size_t len;
    char *buf = read_file(path, &len);
    if (!buf) {
        snprintf(err, errlen, "cannot open %s", path);
        return NULL;
    }

    *code_len = len;
    Section *sec = NULL;
    int row = 0, lineno = 0;
    for (size_t pos = 0; pos < len; lineno++) {
        size_t start = pos, end = pos;
        while (end < len && buf[end] != '\n') end++;
        pos = end + 1;
        size_t n = end - start;
        if (n > 0 && buf[start + n - 1] == '\r') n--;
        const char *line = buf + start;

        if (is_marker(line, n)) {
            if (!sec) *code_len = start;
            sec = NULL;
            for (int i = 0; i < NUM_SECTIONS; i++)
                if (strlen(sections[i].name) == n && strncmp(line, sections[i].name, n) == 0)
                    sec = &sections[i];
            if (!sec) {
                snprintf(err, errlen, "%s:%d: unknown section %.*s", path, lineno + 1, (int)n, line);
                free(buf);
                return NULL;
            }
            row = 0;
            continue;
        }
        if (!sec) continue;

        if (row >= sec->rows) {
            snprintf(err, errlen, "%s:%d: too many lines in %s", path, lineno + 1, sec->name);
            free(buf);
            return NULL;
        }
        if (n > (size_t)(sec->row_values * sec->digits)) {
            snprintf(err, errlen, "%s:%d: line too long in %s", path, lineno + 1, sec->name);
            free(buf);
            return NULL;
        }
        for (size_t i = 0; i < n; i++) {
            if (hexval(line[i]) < 0) {
                snprintf(err, errlen, "%s:%d: bad hex digit in %s", path, lineno + 1, sec->name);
                free(buf);
                return NULL;
            }
        }
        uint8_t *dst = sec->mem + row * sec->row_values;
        for (size_t i = 0; i + sec->digits <= n; i += sec->digits) {
            int v = 0;
            for (int d = 0; d < sec->digits; d++) v = v * 16 + hexval(line[i + d]);
            dst[i / sec->digits] = (uint8_t)v;
        }
        row++;
    }
    return buf;
}

bool cart_write(const char *path, const char *code, size_t code_len, char *err, size_t errlen) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        snprintf(err, errlen, "cannot write %s", path);
        return false;
    }
    fwrite(code, 1, code_len, f);
    if (code_len > 0 && code[code_len - 1] != '\n') fputc('\n', f);

    for (int s = 0; s < NUM_SECTIONS; s++) {
        Section *sec = &sections[s];
        int last = -1;  /* last row with any non-zero value */
        for (int r = 0; r < sec->rows; r++)
            for (int i = 0; i < sec->row_values; i++)
                if (sec->mem[r * sec->row_values + i]) last = r;
        if (last < 0) continue;

        fprintf(f, "%s\n", sec->name);
        for (int r = 0; r <= last; r++) {
            for (int i = 0; i < sec->row_values; i++)
                fprintf(f, sec->digits == 1 ? "%x" : "%02x", sec->mem[r * sec->row_values + i]);
            fputc('\n', f);
        }
    }

    bool ok = !ferror(f);
    if (fclose(f) != 0) ok = false;
    if (!ok) snprintf(err, errlen, "cannot write %s", path);
    return ok;
}
