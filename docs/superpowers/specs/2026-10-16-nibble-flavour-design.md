# nibble flavour — design

Sub-project 1 of the editors/onboarding effort. Roadmap after this one:
sprites + storage → editor shell → code editor → sprite editor → help screen + lesson carts.

## Goal

Let carts be written in a Python-like dialect of Lua ("nibble flavour", `.nib` files):
indentation instead of `end`, `def`, `elif`, `+=`, `!=`, `#` comments. It is a
line-preserving source rewrite to Lua 5.4, so Lua's speed, standard library and error
messages are kept, and error line numbers match the `.nib` file exactly.

Also provide a compile-only checker so tools (later the built-in code editor, now the
command line) can report errors before the cart runs.

## Who it's for

The project owner, returning to Lua and game dev after a long break. Lesson carts
(sub-project 6) will be written in this dialect.

## Syntax

| Write | Becomes |
|---|---|
| `if c:` / `elif c:` / `else:` | `if c then` / `elseif c then` / `else` |
| `while c:` | `while c do` |
| `for i = 1, 10:` / `for k, v in pairs(t):` | `for ... do` |
| `def f(a):` / `local def f(a):` | `function f(a)` / `local function f(a)` |
| `def obj.f(a):` / `def obj:f(a):` | `function obj.f(a)` / `function obj:f(a)` |
| block ends (dedent) | `end`, appended to the block's last code line |
| `a != b` | `a ~= b` |
| `x += e` (also `-=`, `*=`, `/=`, `..=`) | `x = x + (e)` |
| `# note` (`#` at line start or followed by whitespace) | `-- note` |
| `if c: stmt` (one-line form) | `if c then stmt end` |

Everything else is plain Lua and passes through unchanged: `local`, tables, `and`/`or`/`not`,
`true`/`false`/`nil`, `#t` (length), `--` comments, `repeat ... until`, and
`function ... end` (e.g. anonymous functions passed as arguments).

### Rules

- **Block colon.** On a line starting with `if`, `elif`, `else`, `while`, `for`, `def`
  or `local def`, the block colon is the first `:` that is outside strings/comments, at
  bracket depth 0, and followed by whitespace or end of line. `obj:move()` is never a
  block colon (no whitespace after it). If text follows the colon, it's the one-line form
  and the block is closed on the same line.
- **Blocks.** After a line that opens a block (trailing colon), the next code line must be
  indented deeper; that depth is the block's body indent. A block closes when a code line
  appears at an indent less than its body indent. All blocks closed by one line get their
  `end`s appended to the last code line before it, inserted before any trailing comment.
  `elif`/`else` close the previous branch without emitting `end`; the `if` gets one `end`
  when the whole chain closes. End of file closes every open block.
- **Plain-Lua indentation.** Extra indentation not introduced by a block colon (e.g. a
  Lua `for ... do ... end` written inside a `.nib` file) is allowed and ignored.
- **Brackets and long strings.** While `(`, `{` or `[` is open, or inside a `[[ ]]`
  string / `--[[ ]]` comment, line breaks and indentation are ignored. Strings and
  comments are never rewritten.
- **Compound assignment.** Recognised at statement level, bracket depth 0, outside strings:
  `<lhs> op= <rhs>` → `<lhs> = <lhs> op (<rhs>)`. The left side is evaluated twice
  (fine for the variables and field paths carts use).
- **Lines are preserved.** The output has exactly as many lines as the input. No line
  is added or removed.

### Example

```python
def _update():
    if btn(0): px -= 1
    for _, b in ipairs(balls):
        b.x += b.dx
        if b.x < 1 or b.x > 126:
            b.dx = -b.dx   # bounce
```

```lua
function _update()
    if btn(0) then px = px - (1) end
    for _, b in ipairs(balls) do
        b.x = b.x + (b.dx)
        if b.x < 1 or b.x > 126 then
            b.dx = -b.dx end end end   -- bounce
```

## Errors

The rewriter reports `line N: message` for:

- tabs and spaces mixed in one file's indentation
- a dedent to an indent that matches no enclosing block ("unindent does not match")
- a block opener with no indented body ("expected an indented block")
- `elif`/`else` with no `if` at that indent
- an unclosed bracket, string or long comment at end of file

Lua compile errors on the translated code already carry correct `.nib` line numbers
because lines are preserved.

## Components

### `src/nib.lua`

A Lua module, no globals, returning:

- `translate(src) → lua_src` or `nil, line, msg`
- `check(src, chunkname) → nil` (no errors) or `line, msg`. Runs `translate`, then
  `load(lua_src, chunkname)` to compile without running. Lua reports one syntax error at a
  time, so this returns the first error.

Internally: a small line scanner tracking string/comment/bracket state across lines,
an indent stack of open blocks, and per-line keyword/operator rewriting of code
regions only.

### Embedding

The Makefile generates `build/nib_lua.h` from `src/nib.lua` (a `sed` step turning
each line into a C string literal). `api.c` includes it. No runtime file lookup.

### `api_load` changes

If the cart path ends in `.nib`:
1. Read the file.
2. Load the embedded module into the cart's state (kept in the registry, not a global).
3. Call `translate`; on failure, fill `err` with `path:line: msg` and return NULL
   (it shows on the existing error screen).
4. `luaL_loadbuffer` the result with chunk name `@path`, then run it as today.

`.lua` carts load exactly as before. Hot reload, Ctrl+R and the error screen are
unchanged; `main.c` does not change, except as below.

### `--check` flag

`./nibble --check cart.nib` runs `check` without opening a window, prints
`cart.nib:LINE: message` on error and exits 1, otherwise exits 0. This works as an external
editor's linter now, and the built-in code editor (sub-project 4) will call the same
`check` on every edit to highlight the error line.

### Carts

- `carts/demo.nib`: `demo.lua` ported to the dialect. `make run` uses it.
- `carts/demo.lua` stays as the plain-Lua example.

### README

Document the dialect (syntax table and rules in short), `--check`, and `.nib` vs `.lua`.

## Testing

- `tests/nib_test.lua`, run with `make test` using plain `lua5.4` (no window):
  - one translate test (input → expected output) per syntax row and rule above,
    including `obj:method()` lines, `#t` vs `# comment`, `:` and `#` inside strings,
    multi-line tables, long strings/comments, nested and multiple-block dedents, `elif`
    chains, one-line forms, mixed plain-Lua blocks
  - every test asserts line count is preserved
  - each error case → expected line number and message
  - `check` on a cart with a Lua syntax error returns the right `.nib` line
- Manual: `make run` plays `demo.nib` the same as `demo.lua`; introducing an error
  shows it with the right line on the error screen; `--check` exits 0/1 correctly.

## Out of scope

- Syntax highlighting and live error highlighting UI (sub-project 4, code editor).
- Lint warnings beyond syntax errors (e.g. misspelled global names like `btnn`) —
  needs a real parser; candidate for later.
- New API functions.
