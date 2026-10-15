-- nibble flavour: a line-preserving rewrite of a Python-like Lua dialect to Lua.
-- translate(src) -> lua_src | nil, line, msg
-- check(src, chunkname) -> nil | line, msg

local M = {}

local OPEN = { ["("] = true, ["{"] = true, ["["] = true }
local CLOSE = { [")"] = true, ["}"] = true, ["]"] = true }

-- Splits src into {text, term} records; term is the line ending ("\n", "\r\n" or "").
local function split(src)
  local lines, pos = {}, 1
  while pos <= #src do
    local nl = src:find("\n", pos, true)
    local text = src:sub(pos, (nl or #src + 1) - 1)
    local term = nl and "\n" or ""
    if text:sub(-1) == "\r" then
      text, term = text:sub(1, -2), "\r" .. term
    end
    lines[#lines + 1] = { text = text, term = term }
    pos = (nl or #src) + 1
  end
  return lines
end

-- Classifies each char of a line as code "c", string "s" or comment "m", and records
-- bracket depth per char. st carries string/long-bracket/bracket state across lines.
-- Returns info, or nil, msg on an unfinished short string.
local function scan(text, st, lineno)
  local kinds, depths = {}, {}
  local info = {
    kinds = kinds, depths = depths,
    clean = not st.long and not st.str and #st.brackets == 0,
  }
  local i, n = 1, #text

  local function mark(from, to, kind)
    for j = from, to do
      kinds[j], depths[j] = kind, #st.brackets
    end
  end

  while i <= n do
    local c = text:sub(i, i)
    if st.long then
      local _, e = text:find("]" .. ("="):rep(st.long.level) .. "]", i, true)
      mark(i, e or n, st.long.comment and "m" or "s")
      if e then st.long = nil end
      i = (e or n) + 1
    elseif st.str then
      while i <= n do
        local ch = text:sub(i, i)
        mark(i, i, "s")
        if ch == "\\" then
          mark(i + 1, math.min(i + 1, n), "s")
          i = i + 2
        elseif ch == st.str then
          st.str = nil
          i = i + 1
          break
        else
          i = i + 1
        end
      end
      -- a string may only continue onto the next line after a trailing backslash
      if st.str and not (i == n + 2 and text:sub(n, n) == "\\") then
        return nil, "unfinished string"
      end
    elseif c == '"' or c == "'" then
      st.str = c
      mark(i, i, "s")
      i = i + 1
    elseif text:match("^%[=*%[", i) then
      local eq = text:match("^%[(=*)%[", i)
      st.long = { level = #eq, comment = false, line = lineno }
      mark(i, i + #eq + 1, "s")
      i = i + #eq + 2
    elseif text:sub(i, i + 1) == "--" then
      local eq = text:match("^%-%-%[(=*)%[", i)
      if eq then
        st.long = { level = #eq, comment = true, line = lineno }
        mark(i, i + #eq + 3, "m")
        i = i + #eq + 4
      else
        mark(i, n, "m")
        i = n + 1
      end
    elseif c == "#" and (text:sub(1, i - 1):match("^%s*$") or text:sub(i + 1, i + 1):match("^%s?$")) then
      info.hash = i
      mark(i, n, "m")
      i = n + 1
    else
      mark(i, i, "c")
      if OPEN[c] then
        st.brackets[#st.brackets + 1] = lineno
      elseif CLOSE[c] and #st.brackets > 0 then
        st.brackets[#st.brackets] = nil
        depths[i] = #st.brackets
      end
      i = i + 1
    end
  end

  -- cend: last char that is a string or non-space code
  for j = n, 1, -1 do
    if kinds[j] == "s" or (kinds[j] == "c" and not text:sub(j, j):match("%s")) then
      info.cend = j
      break
    end
  end
  -- " end" can be appended to this line only if its last content char isn't inside a string
  info.can_end = info.cend ~= nil and not st.str and not (st.long and not st.long.comment)
  return info
end

local HEADERS = {
  { pat = "^if%f[^%w_]", kind = "if", colon = " then" },
  { pat = "^elif%f[^%w_]", kind = "elif", colon = " then", kw = "elseif" },
  { pat = "^else%f[^%w_]", kind = "else", colon = "" },
  { pat = "^while%f[^%w_]", kind = "loop", colon = " do" },
  { pat = "^for%f[^%w_]", kind = "loop", colon = " do" },
  { pat = "^def%f[^%w_]", kind = "def", colon = "", kw = "function" },
  { pat = "^local%s+def%f[^%w_]", kind = "def", colon = "", kw = "function" },
}

-- Finds a block header on a clean line: returns header, colon position (nil when the
-- colon isn't on this line).
local function find_header(text, info, indent)
  local rest = text:sub(indent + 1)
  for _, h in ipairs(HEADERS) do
    if rest:match(h.pat) and info.kinds[indent + 1] == "c" then
      for j = indent + 1, info.cend or 0 do
        if text:sub(j, j) == ":" and info.kinds[j] == "c" and info.depths[j] == 0
            and (j == info.cend or text:sub(j + 1, j + 1):match("%s")) then
          return h, j
        end
      end
      return h, nil
    end
  end
end

-- Finds `lhs op= rhs` in text[from..to] at depth 0 and returns the edits for it.
local function compound(text, info, from, to)
  for j = from, to - 1 do
    if info.kinds[j] == "c" and info.depths[j] == 0 then
      local op
      if text:sub(j, j + 2) == "..=" then
        op = ".."
      elseif text:sub(j, j):match("[%+%-%*/]") and text:sub(j + 1, j + 1) == "="
          and text:sub(j + 2, j + 2) ~= "=" and text:sub(j - 1, j - 1) ~= "/" then
        op = text:sub(j, j)
      end
      if op then
        local lhs = text:sub(from, j - 1):match("^%s*(.-)%s*$")
        local eq = j + #op
        local rstart = text:find("%S", eq + 1)
        if lhs == "" or not rstart or rstart > to then return nil end
        return {
          { pos = j, len = #op + 1, repl = "= " .. lhs .. " " .. op },
          { pos = rstart, len = 0, repl = "(" },
          { pos = to + 1, len = 0, repl = ")" },
        }
      end
    end
  end
end

function M.translate(src)
  local lines = split(src)
  local st = { brackets = {} }
  local infos, edits = {}, {}
  local seq = 0
  local stack = {}
  local last_content
  local indent_char

  local function edit(i, pos, len, repl)
    seq = seq + 1
    edits[i][#edits[i] + 1] = { pos = pos, len = len, repl = repl, seq = seq }
  end

  local pending  -- a header whose colon is on a later line

  -- Rewrites compound assignments in each ;-separated statement of text[from..cend].
  local function compounds(i, text, info, from)
    local start = from
    for j = from, info.cend + 1 do
      local stop = j > info.cend
          or (text:sub(j, j) == ";" and info.kinds[j] == "c" and info.depths[j] == 0)
      if stop then
        local e = j - 1
        while e >= start and text:sub(e, e):match("%s") do e = e - 1 end
        for _, ed in ipairs(compound(text, info, start, e) or {}) do
          edit(i, ed.pos, ed.len, ed.repl)
        end
        start = j + 1
      end
    end
  end

  local function add_ends(k)
    if k > 0 and last_content then
      edit(last_content, infos[last_content].cend + 1, 0, (" end"):rep(k))
    end
  end

  for i, line in ipairs(lines) do
    local text = line.text
    edits[i] = {}
    local info, err = scan(text, st, i)
    if not info then return nil, i, err end
    infos[i] = info

    if info.hash then
      edit(i, info.hash, 1, text:sub(info.hash + 1, info.hash + 1) == "[" and "-- " or "--")
    end
    for j = 1, #text - 1 do
      if text:sub(j, j + 1) == "!=" and info.kinds[j] == "c" then edit(i, j, 2, "~=") end
    end

    if info.clean and info.cend and info.kinds[#text:match("^[ \t]*") + 1] ~= "m" then
      local ind = text:match("^[ \t]*")
      local n = #ind
      if n > 0 then
        indent_char = indent_char or ind:sub(1, 1)
        if ind:find(indent_char == " " and "\t" or " ", 1, true) then
          return nil, i, "inconsistent use of tabs and spaces in indentation"
        end
      end

      local h, colon = find_header(text, info, n)
      -- a header can continue onto later lines while a bracket is open
      local multiline = h and not colon and #st.brackets > 0
      if h and not colon and not multiline then h = nil end
      local continues = h and (h.kind == "elif" or h.kind == "else")

      local top = stack[#stack]
      if top and not top.body and not top.oneline then
        if n <= top.indent then return nil, i, "expected an indented block" end
        top.body = n
      end

      -- close blocks this line dedents out of
      local ends, last_real, continued = 0, nil, false
      while #stack > 0 do
        top = stack[#stack]
        if not top.oneline and top.body <= n then break end
        stack[#stack] = nil
        if continues and top.kind == "if" and top.indent == n then
          continued = true
          break
        end
        ends = ends + 1
        if not top.oneline then last_real = top end
      end
      if last_real and n > last_real.indent and not continued then
        return nil, i, "unindent does not match any outer indentation level"
      end
      if continues and not continued then
        return nil, i, h.kind .. " without if"
      end
      add_ends(ends)

      local stmt_from = n + 1
      if h then
        local kwpos = text:find("def", n + 1, true)
        if h.kw == "elseif" then
          edit(i, n + 1, 4, "elseif")
        elseif h.kw then
          edit(i, kwpos, 3, "function")
        end
        local block = {
          indent = n, line = i,
          kind = (h.kind == "loop" or h.kind == "def") and "other" or "if",
        }
        if multiline then
          pending = { block = block, h = h }
          stmt_from = nil
        else
          edit(i, colon, 1, h.colon)
          block.oneline = colon < info.cend
          stack[#stack + 1] = block
          stmt_from = block.oneline and colon + 1 or nil
        end
      end
      if stmt_from then compounds(i, text, info, stmt_from) end
    elseif pending and #st.brackets == 0 then
      -- the line closing a multi-line header must end with its colon
      local c = info.cend
      if c and text:sub(c, c) == ":" and info.kinds[c] == "c" and info.depths[c] == 0 then
        edit(i, c, 1, pending.h.colon)
        stack[#stack + 1] = pending.block
      end
      pending = nil
    end

    if info.can_end then last_content = i end
  end

  if st.str then return nil, #lines, "unfinished string" end
  if st.long then
    return nil, st.long.line, st.long.comment and "unfinished long comment" or "unfinished long string"
  end
  if #st.brackets > 0 then return nil, st.brackets[1], "unclosed bracket" end
  local top = stack[#stack]
  if top and not top.body and not top.oneline then
    return nil, top.line, "expected an indented block"
  end
  add_ends(#stack)

  local out = {}
  for i, line in ipairs(lines) do
    local text = line.text
    table.sort(edits[i], function(a, b)
      if a.pos ~= b.pos then return a.pos > b.pos end
      return a.seq > b.seq
    end)
    for _, e in ipairs(edits[i]) do
      text = text:sub(1, e.pos - 1) .. e.repl .. text:sub(e.pos + e.len)
    end
    out[i] = text .. line.term
  end
  return table.concat(out)
end

function M.check(src, chunkname)
  local out, line, msg = M.translate(src)
  if not out then return line, msg end
  local ok, err = load(out, chunkname)
  if ok then return nil end
  local l, m = err:match(":(%d+): (.*)$")
  return tonumber(l), m or err
end

return M
