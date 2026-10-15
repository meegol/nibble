#include "code_edit.h"
#include "api.h"
#include "editor.h"
#include "gfx.h"
#include "help.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLS        (SCREEN_W / FONT_W)
#define ROWS        ((EDIT_BOTTOM - EDIT_TOP) / FONT_H)
#define TEXT_X      1
#define INDENT      2
#define CHECK_DELAY 20   /* frames after the last edit before checking for errors */
#define MAX_UNDO    200

#define COL_BG       0
#define COL_TEXT     12
#define COL_KEYWORD  10
#define COL_API      11
#define COL_NUMBER   3
#define COL_STRING   4
#define COL_COMMENT  14
#define COL_SELECT   8
#define COL_ERR_BG   1
#define COL_ERR_MARK 2
#define COL_CURSOR   4

static Cart *cart;
static const char *path;

/* The text, NUL-terminated, without '\r'. */
static char *buf;
static size_t len, cap;

/* Line starts, and the lexer state at each line start (0, or long bracket: level+1,
   negative for comments). */
static size_t *lstart;
static int *lstate;
static int nlines;

static size_t cur, anchor;  /* selection is [min, max) of the two */
static int want_col;        /* column kept while moving up and down */
static int scroll_x, scroll_y;
static int blink;
static bool dragging;

static int err_line;        /* 0: no error */
static char err_msg[128];
static int check_timer;

typedef struct {
    char *text;
    size_t len, cur;
} Snapshot;

static Snapshot undo[MAX_UNDO], redo[MAX_UNDO];
static int nundo, nredo;
static int last_kind;  /* edit kind of the last undo snapshot, so typing groups together */

enum { EDIT_OTHER, EDIT_TYPE, EDIT_DELETE };

/* ---- buffer ---- */

static void reserve(size_t n) {
    if (n + 1 <= cap) return;
    while (cap < n + 1) cap = cap ? cap * 2 : 4096;
    buf = realloc(buf, cap);
    if (!buf) abort();
}

static int line_of(size_t pos) {
    int lo = 0, hi = nlines - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (lstart[mid] <= pos) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

static size_t line_end(int l) {
    return l + 1 < nlines ? lstart[l + 1] - 1 : len;
}

static int col_of(size_t pos) {
    return (int)(pos - lstart[line_of(pos)]);
}

/* Position at column col of line l, clamped to the line. */
static size_t pos_at(int l, int col) {
    if (l < 0) l = 0;
    if (l >= nlines) l = nlines - 1;
    size_t p = lstart[l] + (col < 0 ? 0 : (size_t)col);
    return p > line_end(l) ? line_end(l) : p;
}

/* Scans s[from..to) in lexer state *st (see lstate), writing colors if non-NULL. */
static void lex(const char *s, size_t from, size_t to, int *st, uint8_t *colors) {
    size_t i = from;
#define PAINT(a, b, c) do { if (colors) for (size_t k_ = (a); k_ < (b); k_++) colors[k_ - from] = (c); } while (0)
    while (i < to) {
        if (*st != 0) {
            int level = abs(*st) - 1;
            int col = *st < 0 ? COL_COMMENT : COL_STRING;
            size_t j = i;
            while (j < to) {
                if (s[j] == ']') {
                    size_t k = j + 1;
                    int eq = 0;
                    while (k < to && s[k] == '=') { eq++; k++; }
                    if (eq == level && k < to && s[k] == ']') {
                        j = k + 1;
                        *st = 0;
                        break;
                    }
                }
                j++;
            }
            PAINT(i, j, col);
            i = j;
            continue;
        }
        char c = s[i];
        size_t j = i + 1;
        int col = COL_TEXT;
        if (c == '-' && i + 1 < to && s[i + 1] == '-') {
            size_t k = i + 2;
            int eq = 0;
            if (k < to && s[k] == '[') {
                k++;
                while (k < to && s[k] == '=') { eq++; k++; }
                if (k < to && s[k] == '[') {
                    *st = -(eq + 1);
                    PAINT(i, k + 1, COL_COMMENT);
                    i = k + 1;
                    continue;
                }
            }
            j = to;
            col = COL_COMMENT;
        } else if (c == '#') {
            bool line_start = true;
            for (size_t k = from; k < i; k++)
                if (s[k] != ' ' && s[k] != '\t') line_start = false;
            if (line_start || i + 1 == to || isspace((unsigned char)s[i + 1])) {
                j = to;
                col = COL_COMMENT;
            }
        } else if (c == '"' || c == '\'') {
            while (j < to && s[j] != c) j += s[j] == '\\' ? 2 : 1;
            if (j < to) j++;
            if (j > to) j = to;
            col = COL_STRING;
        } else if (c == '[') {
            size_t k = i + 1;
            int eq = 0;
            while (k < to && s[k] == '=') { eq++; k++; }
            if (k < to && s[k] == '[') {
                *st = eq + 1;
                PAINT(i, k + 1, COL_STRING);
                i = k + 1;
                continue;
            }
        } else if (isdigit((unsigned char)c) || (c == '.' && i + 1 < to && isdigit((unsigned char)s[i + 1]))) {
            while (j < to && (isalnum((unsigned char)s[j]) || s[j] == '.' || s[j] == '_')) j++;
            col = COL_NUMBER;
        } else if (isalpha((unsigned char)c) || c == '_') {
            while (j < to && (isalnum((unsigned char)s[j]) || s[j] == '_')) j++;
            static const char *KEYWORDS[] = {
                "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
                "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then",
                "true", "until", "while", "def", "elif", NULL,
            };
            size_t n = j - i;
            for (const char **k = KEYWORDS; *k; k++)
                if (strlen(*k) == n && strncmp(*k, s + i, n) == 0) col = COL_KEYWORD;
            if (col == COL_TEXT && api_is_function(s + i, n)) col = COL_API;
        }
        PAINT(i, j, col);
        i = j;
    }
#undef PAINT
}

static void reindex(void) {
    int n = 1;
    for (size_t i = 0; i < len; i++) n += buf[i] == '\n';
    lstart = realloc(lstart, n * sizeof *lstart);
    lstate = realloc(lstate, n * sizeof *lstate);
    if (!lstart || !lstate) abort();
    nlines = 0;
    lstart[nlines++] = 0;
    for (size_t i = 0; i < len; i++)
        if (buf[i] == '\n') lstart[nlines++] = i + 1;
    int st = 0;
    for (int l = 0; l < nlines; l++) {
        lstate[l] = st;
        lex(buf, lstart[l], line_end(l), &st, NULL);
    }
}

static void set_text(const char *text, size_t n) {
    reserve(n);
    len = 0;
    for (size_t i = 0; i < n; i++)
        if (text[i] != '\r') buf[len++] = text[i];
    buf[len] = '\0';
    reindex();
}

/* ---- undo ---- */

static void push(Snapshot *stack, int *n) {
    if (*n == MAX_UNDO) {
        free(stack[0].text);
        memmove(stack, stack + 1, (MAX_UNDO - 1) * sizeof *stack);
        (*n)--;
    }
    Snapshot *s = &stack[(*n)++];
    s->text = malloc(len + 1);
    if (!s->text) abort();
    memcpy(s->text, buf, len + 1);
    s->len = len;
    s->cur = cur;
}

static void clear(Snapshot *stack, int *n) {
    while (*n > 0) free(stack[--(*n)].text);
}

/* Records the text before an edit. Consecutive edits of the same kind share a snapshot. */
static void before_edit(int kind) {
    if (kind == EDIT_OTHER || kind != last_kind) push(undo, &nundo);
    last_kind = kind;
    clear(redo, &nredo);
}

static void restore(Snapshot *from, int *nfrom, Snapshot *to, int *nto);

/* ---- editing ---- */

static void ensure_visible(void) {
    int l = line_of(cur), c = col_of(cur);
    if (l < scroll_y) scroll_y = l;
    if (l >= scroll_y + ROWS) scroll_y = l - ROWS + 1;
    if (c < scroll_x) scroll_x = c;
    if (c >= scroll_x + COLS - 1) scroll_x = c - COLS + 2;
}

static void changed(void) {
    reindex();
    cart_set_code(cart, buf, len);
    cart->version++;
    check_timer = CHECK_DELAY;
    want_col = col_of(cur);
    ensure_visible();
}

static bool has_selection(void) {
    return cur != anchor;
}

static size_t sel_lo(void) { return cur < anchor ? cur : anchor; }
static size_t sel_hi(void) { return cur < anchor ? anchor : cur; }

static void delete_range(size_t a, size_t b) {
    memmove(buf + a, buf + b, len - b + 1);
    len -= b - a;
    cur = anchor = a;
}

static void delete_selection(void) {
    if (has_selection()) delete_range(sel_lo(), sel_hi());
}

static void insert(const char *s, size_t n) {
    delete_selection();
    reserve(len + n);
    memmove(buf + cur + n, buf + cur, len - cur + 1);
    memcpy(buf + cur, s, n);
    len += n;
    cur = anchor = cur + n;
}

static void restore(Snapshot *from, int *nfrom, Snapshot *to, int *nto) {
    if (*nfrom == 0) return;
    push(to, nto);
    Snapshot *s = &from[--(*nfrom)];
    set_text(s->text, s->len);
    cur = anchor = s->cur > len ? len : s->cur;
    free(s->text);
    last_kind = EDIT_OTHER;
    changed();
}

static void move_to(size_t pos, bool select) {
    cur = pos;
    if (!select) anchor = pos;
    last_kind = EDIT_OTHER;
    blink = 0;
    ensure_visible();
}

static int indent_of(int l) {
    int n = 0;
    while (lstart[l] + n < line_end(l) && buf[lstart[l] + n] == ' ') n++;
    return n;
}

static void newline(void) {
    before_edit(EDIT_OTHER);
    size_t at = sel_lo();
    int l = line_of(at);
    int indent = indent_of(l);
    if (indent > (int)(at - lstart[l])) indent = (int)(at - lstart[l]);
    /* a block opener (trailing ':') indents the next line */
    size_t p = at;
    while (p > lstart[l] && buf[p - 1] == ' ') p--;
    if (p > lstart[l] && buf[p - 1] == ':') indent += INDENT;
    char s[256];
    s[0] = '\n';
    if (indent > (int)sizeof s - 1) indent = sizeof s - 1;
    memset(s + 1, ' ', indent);
    insert(s, indent + 1);
    changed();
}

static void backspace(void) {
    if (has_selection()) {
        before_edit(EDIT_DELETE);
        delete_selection();
    } else if (cur > 0) {
        before_edit(EDIT_DELETE);
        int l = line_of(cur), c = col_of(cur);
        /* in leading spaces: back to the previous indent stop */
        if (c > 0 && indent_of(l) >= c) delete_range(cur - ((c - 1) % INDENT + 1), cur);
        else delete_range(cur - 1, cur);
    } else {
        return;
    }
    changed();
}

static void delete_forward(void) {
    if (has_selection()) {
        before_edit(EDIT_DELETE);
        delete_selection();
    } else if (cur < len) {
        before_edit(EDIT_DELETE);
        delete_range(cur, cur + 1);
    } else {
        return;
    }
    changed();
}

/* Indents (dir 1) or dedents (dir -1) every line in the selection. */
static void shift_lines(int dir) {
    before_edit(EDIT_OTHER);
    int a = line_of(sel_lo()), b = line_of(sel_hi());
    if (b > a && sel_hi() == lstart[b]) b--;
    bool cur_first = cur <= anchor;
    size_t lo = lstart[a], hi = line_end(b);
    for (int l = b; l >= a; l--) {
        size_t s = lstart[l];
        if (dir > 0) {
            reserve(len + INDENT);
            memmove(buf + s + INDENT, buf + s, len - s + 1);
            memset(buf + s, ' ', INDENT);
            len += INDENT;
            hi += INDENT;
        } else {
            int n = indent_of(l) < INDENT ? indent_of(l) : INDENT;
            memmove(buf + s, buf + s + n, len - s - n + 1);
            len -= n;
            hi -= n;
        }
    }
    cur = cur_first ? lo : hi;
    anchor = cur_first ? hi : lo;
    changed();
}

static void copy(void) {
    if (!has_selection()) return;
    size_t a = sel_lo(), b = sel_hi();
    char saved = buf[b];
    buf[b] = '\0';
    SDL_SetClipboardText(buf + a);
    buf[b] = saved;
}

static void paste(void) {
    char *text = SDL_GetClipboardText();
    if (!text) return;
    size_t n = 0;
    for (char *s = text; *s; s++)
        if (*s == '\n' || *s == '\t' || (*s >= 32 && *s < 127)) text[n++] = *s == '\t' ? ' ' : *s;
    if (n > 0) {
        before_edit(EDIT_OTHER);
        insert(text, n);
        changed();
    }
    SDL_free(text);
}

static size_t word_left(size_t p) {
    while (p > 0 && isspace((unsigned char)buf[p - 1])) p--;
    while (p > 0 && (isalnum((unsigned char)buf[p - 1]) || buf[p - 1] == '_')) p--;
    if (p > 0 && p == cur) p--;
    return p;
}

static size_t word_right(size_t p) {
    while (p < len && isspace((unsigned char)buf[p])) p++;
    while (p < len && (isalnum((unsigned char)buf[p]) || buf[p] == '_')) p++;
    if (p < len && p == cur) p++;
    return p;
}

/* ---- error checking ---- */

static void check(void) {
    char msg[512];
    err_line = 0;
    if (api_check(path, buf, len, msg, sizeof msg)) return;
    /* msg is "path:LINE: message" */
    const char *p = strstr(msg, path);
    p = p ? p + strlen(path) : msg;
    if (*p == ':') {
        err_line = atoi(p + 1);
        p = strchr(p + 1, ':');
        p = p ? p + 1 : msg;
        while (*p == ' ') p++;
    }
    snprintf(err_msg, sizeof err_msg, "%.*s", (int)sizeof err_msg - 1, p);
    if (err_line == 0) err_line = nlines;
}

/* ---- interface ---- */

void code_init(Cart *c, const char *p) {
    cart = c;
    path = p;
    set_text("", 0);
}

void code_reload(void) {
    set_text(cart->code ? cart->code : "", cart->code_len);
    cur = anchor = 0;
    scroll_x = scroll_y = 0;
    clear(undo, &nundo);
    clear(redo, &nredo);
    last_kind = EDIT_OTHER;
    check();
}

void code_goto_line(int line) {
    int l = line - 1 < nlines ? line - 1 : nlines - 1;
    move_to(lstart[l] + indent_of(l), false);
    scroll_y = l - ROWS / 2 < 0 ? 0 : l - ROWS / 2;
    want_col = col_of(cur);
}

static size_t mouse_pos(float x, float y) {
    int col = ((int)x - TEXT_X + FONT_W / 2) / FONT_W + scroll_x;
    int row = (int)(y - EDIT_TOP) / FONT_H;
    if (y < EDIT_TOP) row = -1;
    return pos_at(scroll_y + row, col);
}

static void key(const SDL_KeyboardEvent *k) {
    bool ctrl = k->mod & SDL_KMOD_CTRL, shift = k->mod & SDL_KMOD_SHIFT;
    int l = line_of(cur);
    switch (k->key) {
    case SDLK_LEFT:
        if (has_selection() && !shift) move_to(sel_lo(), false);
        else move_to(ctrl ? word_left(cur) : cur > 0 ? cur - 1 : 0, shift);
        want_col = col_of(cur);
        break;
    case SDLK_RIGHT:
        if (has_selection() && !shift) move_to(sel_hi(), false);
        else move_to(ctrl ? word_right(cur) : cur < len ? cur + 1 : len, shift);
        want_col = col_of(cur);
        break;
    case SDLK_UP: move_to(l > 0 ? pos_at(l - 1, want_col) : 0, shift); break;
    case SDLK_DOWN: move_to(l + 1 < nlines ? pos_at(l + 1, want_col) : len, shift); break;
    case SDLK_PAGEUP: move_to(pos_at(l - ROWS, want_col), shift); break;
    case SDLK_PAGEDOWN: move_to(pos_at(l + ROWS, want_col), shift); break;
    case SDLK_HOME:
        if (ctrl) move_to(0, shift);
        else {
            size_t first = lstart[l] + indent_of(l);
            move_to(cur == first ? lstart[l] : first, shift);
        }
        want_col = col_of(cur);
        break;
    case SDLK_END:
        move_to(ctrl ? len : line_end(l), shift);
        want_col = col_of(cur);
        break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: newline(); break;
    case SDLK_BACKSPACE: backspace(); break;
    case SDLK_DELETE: delete_forward(); break;
    case SDLK_TAB:
        if (ctrl) break;
        if (shift || line_of(sel_lo()) != line_of(sel_hi())) {
            shift_lines(shift ? -1 : 1);
        } else {
            before_edit(EDIT_TYPE);
            int n = INDENT - col_of(sel_lo()) % INDENT;
            insert("    ", n);
            changed();
        }
        break;
    case SDLK_A:
        if (ctrl) { anchor = 0; cur = len; }
        break;
    case SDLK_C:
        if (ctrl) copy();
        break;
    case SDLK_X:
        if (ctrl && has_selection()) {
            copy();
            before_edit(EDIT_OTHER);
            delete_selection();
            changed();
        }
        break;
    case SDLK_V:
        if (ctrl) paste();
        break;
    case SDLK_Z:
        if (ctrl && shift) restore(redo, &nredo, undo, &nundo);
        else if (ctrl) restore(undo, &nundo, redo, &nredo);
        break;
    case SDLK_Y:
        if (ctrl) restore(redo, &nredo, undo, &nundo);
        break;
    default: break;
    }
}

void code_event(const SDL_Event *ev) {
    switch (ev->type) {
    case SDL_EVENT_KEY_DOWN:
        key(&ev->key);
        break;
    case SDL_EVENT_TEXT_INPUT: {
        char s[64];
        size_t n = 0;
        for (const char *t = ev->text.text; *t && n < sizeof s; t++)
            if (*t >= 32 && *t < 127) s[n++] = *t;
        if (n > 0) {
            before_edit(EDIT_TYPE);
            insert(s, n);
            changed();
        }
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.button == SDL_BUTTON_LEFT) {
            bool shift = SDL_GetModState() & SDL_KMOD_SHIFT;
            move_to(mouse_pos(ev->button.x, ev->button.y), shift);
            want_col = col_of(cur);
            dragging = true;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (ev->button.button == SDL_BUTTON_LEFT) dragging = false;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (dragging && (ev->motion.state & SDL_BUTTON_LMASK)) {
            move_to(mouse_pos(ev->motion.x, ev->motion.y), true);
            want_col = col_of(cur);
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        scroll_y -= (int)ev->wheel.y * 3;
        if (scroll_y > nlines - ROWS) scroll_y = nlines - ROWS;
        if (scroll_y < 0) scroll_y = 0;
        break;
    default: break;
    }
}

void code_draw(void) {
    if (check_timer > 0 && --check_timer == 0) check();
    blink++;

    gfx_rectfill(0, EDIT_TOP, SCREEN_W - 1, EDIT_BOTTOM - 1, COL_BG);
    size_t lo = sel_lo(), hi = sel_hi();
    static uint8_t colors[4096];

    for (int row = 0; row < ROWS; row++) {
        int l = scroll_y + row;
        if (l >= nlines) break;
        int y = EDIT_TOP + row * FONT_H;
        size_t s = lstart[l], e = line_end(l);
        if (l + 1 == err_line) {
            gfx_rectfill(0, y, SCREEN_W - 1, y + FONT_H - 1, COL_ERR_BG);
            gfx_rectfill(0, y, 0, y + FONT_H - 1, COL_ERR_MARK);
        }

        size_t n = e - s < sizeof colors ? e - s : sizeof colors;
        int st = lstate[l];
        lex(buf, s, s + n, &st, colors);

        for (int c = 0; c < COLS; c++) {
            size_t p = s + scroll_x + c;
            int x = TEXT_X + c * FONT_W;
            bool selected = p >= lo && p < hi && p <= e;
            if (selected) gfx_rectfill(x - 1, y, x + FONT_W - 2, y + FONT_H - 1, COL_SELECT);
            if (p >= e) continue;
            char ch[2] = {buf[p] == '\t' ? ' ' : buf[p], '\0'};
            int col = p - s < n ? colors[p - s] : COL_TEXT;
            gfx_print(ch, x, y, col);
            /* the font has one case; underline capitals so True and true look different */
            if (isupper((unsigned char)buf[p])) gfx_line(x, y + FONT_H - 1, x + 2, y + FONT_H - 1, col);
        }
    }

    /* cursor */
    int cl = line_of(cur), cc = col_of(cur) - scroll_x;
    if (blink / 30 % 2 == 0 && cl >= scroll_y && cl < scroll_y + ROWS && cc >= 0 && cc < COLS) {
        int x = TEXT_X + cc * FONT_W - 1, y = EDIT_TOP + (cl - scroll_y) * FONT_H;
        gfx_rectfill(x, y, x, y + FONT_H - 2, COL_CURSOR);
    }
}

/* Signature of the API function whose call the cursor is in, or whose name it's on. */
static const char *signature_at_cursor(void) {
    size_t s = lstart[line_of(cur)];
    size_t p = cur;
    /* inside a call: find the unmatched '(' to the left */
    for (int depth = 0; p > s; p--) {
        char c = buf[p - 1];
        if (c == ')') depth++;
        else if (c == '(' && depth-- == 0) {
            size_t e = p - 1, b = e;
            while (b > s && (isalnum((unsigned char)buf[b - 1]) || buf[b - 1] == '_')) b--;
            const char *sig = help_signature(buf + b, e - b);
            if (sig) return sig;
        }
    }
    /* on a name */
    size_t b = cur, e = cur;
    while (b > s && (isalnum((unsigned char)buf[b - 1]) || buf[b - 1] == '_')) b--;
    while (e < len && (isalnum((unsigned char)buf[e]) || buf[e] == '_')) e++;
    return e > b ? help_signature(buf + b, e - b) : NULL;
}

void code_info(char *out, size_t n) {
    const char *sig;
    if (err_line) snprintf(out, n, "!%d %s", err_line, err_msg);
    else if ((sig = signature_at_cursor())) snprintf(out, n, "%s", sig);
    else snprintf(out, n, "line %d/%d col %d", line_of(cur) + 1, nlines, col_of(cur) + 1);
}
