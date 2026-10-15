package.path = "src/?.lua;" .. package.path
local nib = require "nib"

local passed, failed = 0, 0

local function lines(s)
  local _, n = s:gsub("\n", "")
  return n
end

local function fail(name, why)
  failed = failed + 1
  print("FAIL " .. name .. "\n" .. why .. "\n")
end

-- translate(input) must equal expected, with the same line count
local function t(name, input, expected)
  local out, line, msg = nib.translate(input)
  if out == nil then
    return fail(name, ("error %s: %s"):format(line, msg))
  end
  if out ~= expected then
    return fail(name, "expected:\n" .. expected .. "\ngot:\n" .. out)
  end
  if lines(out) ~= lines(input) then
    return fail(name, "line count changed")
  end
  passed = passed + 1
end

-- translate(input) must fail with this line and message
local function e(name, input, line, msg)
  local out, l, m = nib.translate(input)
  if out ~= nil then
    return fail(name, "expected an error, got:\n" .. out)
  end
  if l ~= line or m ~= msg then
    return fail(name, ("expected %d: %s\ngot %s: %s"):format(line, msg, l, m))
  end
  passed = passed + 1
end

local function ok(name, cond, why)
  if cond then passed = passed + 1 else fail(name, why or "") end
end

-- blocks
t("if", "if a:\n  x()", "if a then\n  x() end")
t("if chain", "if a:\n  x()\nelif b:\n  y()\nelse:\n  z()",
  "if a then\n  x()\nelseif b then\n  y()\nelse\n  z() end")
t("while", "while a:\n  x()", "while a do\n  x() end")
t("for numeric", "for i = 1, 3:\n  x(i)", "for i = 1, 3 do\n  x(i) end")
t("for in", "for k, v in pairs(t):\n  x(k)", "for k, v in pairs(t) do\n  x(k) end")
t("def", "def f(a):\n  return a", "function f(a)\n  return a end")
t("local def", "local def f():\n  x()", "local function f()\n  x() end")
t("def field", "def o.f():\n  x()", "function o.f()\n  x() end")
t("def method", "def o:f():\n  x()", "function o:f()\n  x() end")
t("nested", "def f():\n  for i=1,2:\n    if a:\n      x()\ny()",
  "function f()\n  for i=1,2 do\n    if a then\n      x() end end end\ny()")
t("partial dedent", "def f():\n  if a:\n    x()\n  y()\nz()",
  "function f()\n  if a then\n    x() end\n  y() end\nz()")
t("trailing space after colon", "if a:  \n  x()", "if a then  \n  x() end")

-- one-line form
t("one-line if", "if btn(0): px -= 1", "if btn(0) then px = px - (1) end")
t("one-line in block", "def f():\n  if a: x()\n  y()",
  "function f()\n  if a then x() end\n  y() end")
t("one-line else", "if a:\n  x()\nelse: y()", "if a then\n  x()\nelse y() end")
t("one-line elif", "if a: x()\nelif b: y()\nelse: z()",
  "if a then x()\nelseif b then y()\nelse z() end")

-- operators
t("not equal", "if a != b:\n  x()", "if a ~= b then\n  x() end")
t("+=", "x += 1", "x = x + (1)")
t("-=", "x -= y", "x = x - (y)")
t("*=", "x *= a + b", "x = x * (a + b)")
t("/=", "x /= 2", "x = x / (2)")
t("..=", 's ..= "!"', 's = s .. ("!")')
t("field +=", "o.p[i] += 1", "o.p[i] = o.p[i] + (1)")
t("comparisons untouched", "x = a <= b and c >= d and e == f",
  "x = a <= b and c >= d and e == f")
t("+= in call untouched", "f(x == 1)", "f(x == 1)")

-- comments
t("hash comment", "# hi", "-- hi")
t("indented hash comment", "  #hi", "  --hi")
t("trailing hash comment", "x = 1  # hi", "x = 1  -- hi")
t("length", "n = #t", "n = #t")
t("length in expr", "n = #t + #u", "n = #t + #u")
t("end before comment", "if a:\n  x()  # c", "if a then\n  x() end  -- c")
t("lua comment kept", "x = 1 -- hi", "x = 1 -- hi")

-- method calls and strings
t("method call", "obj:move()", "obj:move()")
t("method in header", "if obj:ok():\n  x()", "if obj:ok() then\n  x() end")
t("string untouched", 'print("a:b # != +=")', 'print("a:b # != +=")')
t("escaped quote", 's = "a\\":"', 's = "a\\":"')
t("colon after string", 'if s == "x:":\n  y()', 'if s == "x:" then\n  y() end')
t("single quotes", "if s == ':':\n  y()", "if s == ':' then\n  y() end")

-- multi-line constructs
t("multi-line table",
  "for i = 1, 2:\n  b[i] = {\n    x = 1,\n  y = 2,\n}\nz()",
  "for i = 1, 2 do\n  b[i] = {\n    x = 1,\n  y = 2,\n} end\nz()")
t("long string", "s = [[\nif a:\n]]", "s = [[\nif a:\n]]")
t("long string level", "s = [==[\n]]\nif a:\n]==]", "s = [==[\n]]\nif a:\n]==]")
t("long comment", "--[[\nx:\n]]", "--[[\nx:\n]]")
t("plain lua inside", "def f():\n  for i=1,2 do\n    x()\n  end",
  "function f()\n  for i=1,2 do\n    x()\n  end end")
t("plain lua function arg", "def f():\n  g(function()\n    x()\n  end)\ny()",
  "function f()\n  g(function()\n    x()\n  end) end\ny()")

-- blank and comment lines
t("blank and comment lines", "if a:\n  x()\n\n# c\n  y()\nz()",
  "if a then\n  x()\n\n-- c\n  y() end\nz()")
t("comment line before dedent", "if a:\n  x()\n# c\ny()",
  "if a then\n  x() end\n-- c\ny()")

-- else at outer level closes inner blocks
t("else closes inner", "if a:\n  if b:\n    x()\nelse:\n  y()",
  "if a then\n  if b then\n    x() end\nelse\n  y() end")

-- line endings and empty input
t("crlf", "if a:\r\n  x()\r\n", "if a then\r\n  x() end\r\n")
t("empty", "", "")
t("no trailing newline", "def f():\n  x()", "function f()\n  x() end")
t("trailing newline", "def f():\n  x()\n", "function f()\n  x() end\n")

-- plain Lua passes through
do
  local f = assert(io.open("carts/demo.lua"))
  local src = f:read("a")
  f:close()
  t("demo.lua unchanged", src, src)
end

print(("ok %d, failed %d"):format(passed, failed))
os.exit(failed == 0 and 0 or 1)
