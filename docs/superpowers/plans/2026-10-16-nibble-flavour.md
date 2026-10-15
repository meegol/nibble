# nibble flavour Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Carts can be written as `.nib` files in a Python-like Lua dialect, with a compile-only checker exposed as `./nibble --check`.

**Architecture:** `src/nib.lua` is a line-preserving source rewriter (`translate`) plus `check`. The Makefile embeds it into the binary as a C string header; `api.c` runs it on `.nib` carts before `luaL_loadbuffer`. `main.c` gains a windowless `--check` mode.

**Tech Stack:** C11, Lua 5.4.9, SDL3, make, sed.

**Spec:** `docs/superpowers/specs/2026-10-16-nibble-flavour-design.md`

## Global Constraints

- Output of `translate` has exactly the same number of lines as its input.
- `nib.lua` defines no globals; it returns a module table `{translate = ..., check = ...}`.
- `.lua` carts load exactly as before.
- Commit messages: plain, no Co-Authored-By or other AI attribution.
- README: terse, same style as the existing one.

## Review Focus

1. CRLF files (edited on Windows): `\r` before the newline must not hide a trailing block colon, and output keeps line count.
2. Blank and comment-only lines inside a block, at any indent, never close or open blocks.
3. `else:`/`elif` at an outer indent after nested blocks closes the inner blocks (with `end`) and continues the outer `if` chain (no `end` for it).
4. Colons and `#` inside strings with escaped quotes (`"a\":"`), and a trailing `:` after a string (`if s == "x:":`).
5. Empty file, and a file whose last line has no trailing newline and ends inside an open block.

Each has a test in Task 1 or 2.

---

### Task 1: `translate` for valid input + test harness

**Files:**
- Create: `src/nib.lua`, `tests/nib_test.lua`
- Modify: `Makefile` (add `test` target)

**Interfaces:**
- Produces: `require("nib").translate(src: string) -> string` on success, or `nil, line: integer, msg: string` on failure (failure cases land in Task 2).

- [ ] **Step 1: Write the harness and the failing tests** in `tests/nib_test.lua`: sets `package.path = "src/?.lua;" .. package.path`, a `t(name, input, expected)` helper that asserts output equals `expected` AND line count equals input's, prints failures, exits 1 if any failed. Cases (input → expected):
  - `if a:\n  x()` → `if a then\n  x() end`
  - `if a:\n  x()\nelif b:\n  y()\nelse:\n  z()` → `if a then\n  x()\nelseif b then\n  y()\nelse\n  z() end`
  - `while a:\n  x()` → `while a do\n  x() end`; `for i = 1, 3:` and `for k, v in pairs(t):` → `... do`
  - `def f(a):\n  return a` → `function f(a)\n  return a end`; `local def f():`, `def o.f():`, `def o:f():` → `local function f()`, `function o.f()`, `function o:f()`
  - nested: `def f():\n  for i=1,2:\n    if a:\n      x()\ny()` → last body line `      x() end end end`
  - one-line: `if btn(0): px -= 1` → `if btn(0) then px = px - (1) end`
  - `a != b` → `a ~= b`; `x += 1`, `x -= y`, `x *= a + b`, `x /= 2`, `s ..= "!"` → `x = x * (a + b)` etc.; `o.p[i] += 1` → `o.p[i] = o.p[i] + (1)`
  - `# hi` → `-- hi`; `x = 1  # hi` → `x = 1  -- hi`; `n = #t` unchanged; `if a:\n  x()  # c` → `if a then\n  x() end  -- c`
  - `obj:move()` unchanged; `if obj:ok():\n  x()` → `if obj:ok() then\n  x() end`
  - strings untouched: `print("a:b # != +=")`, `s = "a\":"`, `if s == "x:":\n  y()` → `if s == "x:" then\n  y() end`
  - multi-line table inside a block (demo's `balls[i] = {...}` shape) → only the block's last code line gets `end`
  - long string/comment: `s = [[\nif a:\n]]` and `--[[\nx:\n]]` unchanged
  - plain Lua inside: `def f():\n  for i=1,2 do\n    x()\n  end` → `function f()\n  for i=1,2 do\n    x()\n  end end`
  - blank/comment lines: `if a:\n  x()\n\n# c\n  y()\nz()` → `y() end` on line 5, comment line 4 becomes `-- c`
  - Review Focus 3: `if a:\n  if b:\n    x()\nelse:\n  y()` → `if a then\n  if b then\n    x() end\nelse\n  y() end`
  - Review Focus 1: `if a:\r\n  x()\r\n` → `if a then\r\n  x() end\r\n` (keep `\r`)
  - Review Focus 5: `""` → `""`; `def f():\n  x()` (no trailing newline) → `function f()\n  x() end`
  - Plain Lua file (`demo.lua` contents) translates to itself.

- [ ] **Step 2: Add `test:` target to Makefile** running `lua5.4 tests/nib_test.lua`; add to `.PHONY`. Run `make test`. Expected: FAIL (module `nib` not found).

- [ ] **Step 3: Implement `translate` in `src/nib.lua`.** Algorithm:
  1. Split into lines keeping terminators (`\n`, and a preceding `\r` stays with the terminator).
  2. Scan each line char by char with state carried across lines: in short string (quote char, backslash escapes), long bracket `[=*[` level (string or comment), bracket depth. Produce per line: `indent` (leading whitespace), the code region (start..end, excluding a trailing comment), the comment start position and whether it is a `#` comment (`#` at line start or followed by whitespace, outside strings), and `depth_at_start`.
  3. A line is "logical start" if `depth_at_start == 0` and it doesn't start inside a long string. Only logical-start code lines take part in block logic; others pass through (still rewritten for `!=`, `#` comments).
  4. Block stack entries `{indent = opener_indent, body = nil|n, kind = "if"|"other"}`. For each logical-start code line with indent `n`: set `body = n` on a pending top opener; then pop every block whose `body > n`, counting `end`s — except a block whose `indent == n` and this line starts with `elif`/`else` and kind is `if`: pop it without counting (the new branch re-pushes it as kind `if`). Append the counted `end`s to the last code line seen before this one (insert ` end` × k at its code-region end, before its comment).
  5. Rewrite the code region: header keyword (`if`/`elif`/`else`/`while`/`for`/`def`/`local def`) + block colon (first `:` at depth 0, outside strings, followed by whitespace or end of code) → keyword mapping per the spec table; text after colon → one-line form (append ` end`, no push); otherwise push. `!=` → `~=` outside strings. Compound op: at depth 0 find `op=` (`+ - * / ..`, not `==`, `<=`, `>=`, `~=`) → `lhs = lhs op (rhs)` with lhs trimmed.
  6. At EOF append `end`s for all remaining blocks to the last code line.
  Only code regions are edited, so strings and comments are untouched.

- [ ] **Step 4: Run `make test`.** Expected: all pass, `ok N tests`.

- [ ] **Step 5: Commit** `git add src/nib.lua tests/nib_test.lua Makefile && git commit -m "Add nibble flavour translator"`

### Task 2: Rewriter errors and `check`

**Files:**
- Modify: `src/nib.lua`, `tests/nib_test.lua`

**Interfaces:**
- Produces: `require("nib").check(src: string, chunkname: string) -> nil` if clean, or `line: integer, msg: string` (msg has no `name:line:` prefix).

- [ ] **Step 1: Add failing tests** with an `e(name, input, line, msg)` helper asserting `translate` returns `nil, line, msg`:
  - `if a:\n\tx()\n  y()` → 3, `"inconsistent use of tabs and spaces in indentation"`
  - `if a:\n    x()\n  y()` → 3, `"unindent does not match any outer indentation level"`
  - `if a:\ny()` → 2, `"expected an indented block"`; `if a:` at EOF → 1, same msg
  - `else:\n  x()` → 1, `"else without if"`; `elif a:` likewise `"elif without if"`
  - `x = {1,\n2` → 1, `"unclosed bracket"`; `s = "abc` → 1, `"unfinished string"`; `--[[\nx` → 1, `"unfinished long comment"`; `s = [[\nx` → 1, `"unfinished long string"`
  - `check("x = = 1", "=c")` → 1, msg contains `"unexpected symbol"`; `check("if a:\n  x(\n", "=c")` errors; `check("def f():\n  return 1", "=c")` → nil; `check` returns translate errors unchanged.

- [ ] **Step 2: `make test`.** Expected: new cases FAIL.

- [ ] **Step 3: Implement.** Errors detected during the Task 1 scan; line numbers are 1-based and point at the offending line (or the opening line for unclosed constructs). `check` = `translate` then `load(out, chunkname)`; on load error parse `":(%d+): (.*)$"`.

- [ ] **Step 4: `make test`.** Expected: all pass.

- [ ] **Step 5: Commit** `git commit -am "Add nibble flavour errors and check"`

### Task 3: Engine integration, `--check`, demo, README

**Files:**
- Modify: `Makefile`, `src/api.c`, `src/api.h`, `src/main.c`, `README.md`
- Create: `carts/demo.nib`

**Interfaces:**
- Consumes: `translate`, `check` from Tasks 1–2.
- Produces: `bool api_check(const char *path, char *err, size_t errlen)` in `api.h`: true if clean; else `err` = `path:LINE: msg`. Also `api_load` accepts `.nib`.

- [ ] **Step 1: Makefile embed.** Rule `build/nib_lua.h: src/nib.lua | build` using `sed` (escape `\` and `"`, wrap each line as `"...\n"`) into `static const char NIB_LUA[] = ...;`. Add `-Ibuild` to `CFLAGS`, and `build/api.o: build/nib_lua.h`. Run `make`. Expected: builds clean with no warnings.

- [ ] **Step 2: `api.c`.** Helper `static bool push_nib(lua_State *L, const char *field)` loads `NIB_LUA` (chunk `=nib`), calls it and pushes `module[field]`. Helper to read a whole file with stdio. In `api_load`: if the path ends with `.nib`, read it, call `translate`, and on `nil` set `err` to `"%s:%d: %s"` (path, line, msg), close and return NULL. Otherwise `luaL_loadbuffer(out, len, "@path")` replaces `luaL_loadfile`. `api_check`: a bare `luaL_newstate`, `.nib` → `check(src, "@" .. path)`, `.lua` → `luaL_loadfile`, format the error the same way.

- [ ] **Step 3: `main.c` `--check`.** If `argc > 2 && strcmp(argv[1], "--check") == 0`, before `SDL_Init`: `api_check(argv[2], ...)`, then on error `puts(err)` and return 1, otherwise return 0.

- [ ] **Step 4: Port `carts/demo.nib`** from `demo.lua` (same behaviour, uses `def`, `:`, `+=`, one-line `if`, `#` comments). Change `make run` and main's default path to `carts/demo.nib`.

- [ ] **Step 5: Verify.**
  - `./nibble --check carts/demo.nib; echo $?` → `0`
  - `./nibble --check carts/demo.lua; echo $?` → `0`
  - a copy with a bad dedent at line 5 → prints `<path>:5: unindent does not match any outer indentation level`, exit 1
  - a copy with `x = = 1` at line 7 → `<path>:7: unexpected symbol near '='`, exit 1
  - `make run`: demo plays as before (visual check by the user; in-session use `timeout 3 ./nibble carts/demo.nib` and confirm no error output). Breaking the file while it runs shows the error screen with the right line.
  - `make test` still passes.

- [ ] **Step 6: README.** Add a short "nibble flavour" section (syntax table, `.nib` vs `.lua`, the line-preservation note) and `--check` under Build & run; update the Layout list (`src/nib.lua`, `tests/`) and `make test`. Match the existing terse style.

- [ ] **Step 7: Commit** `git add -A && git commit -m "Run .nib carts, add --check, port demo"`
