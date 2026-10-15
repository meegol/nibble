-- nibble demo cart
-- arrows/wasd: move   z: recolor balls

function _init()
  px, py = 64, 90
  balls = {}
  for i = 1, 10 do
    balls[i] = {
      x = rnd(100) + 14, y = rnd(60) + 24,
      dx = rnd(2) - 1, dy = rnd(2) - 1,
      r = flr(rnd(4)) + 2, c = flr(rnd(11)) + 1,
    }
  end
end

function _update()
  if btn(0) then px = px - 1 end
  if btn(1) then px = px + 1 end
  if btn(2) then py = py - 1 end
  if btn(3) then py = py + 1 end
  px, py = mid(6, px, 121), mid(22, py, 108)

  for _, b in ipairs(balls) do
    b.x, b.y = b.x + b.dx, b.y + b.dy
    if b.x < 1 + b.r or b.x > 126 - b.r then b.dx = -b.dx end
    if b.y < 17 + b.r or b.y > 112 - b.r then b.dy = -b.dy end
  end

  if btnp(4) then
    for _, b in ipairs(balls) do b.c = flr(rnd(11)) + 1 end
  end
end

function _draw()
  cls(0)

  -- scrolling stripes
  for i = 0, 11 do
    local y = 16 + (i * 8 + flr(time() * 12)) % 96
    line(1, y, 126, y, 15)
  end

  for _, b in ipairs(balls) do circfill(b.x, b.y, b.r, b.c) end

  circfill(px, py, 4, 4)
  circ(px, py, 6 + flr(time() * 4) % 2, 3)

  -- header: title + palette strip
  print("NIBBLE", 52, 3, 12)
  for i = 0, 15 do rectfill(i * 8, 10, i * 8 + 7, 13, i) end

  rect(0, 15, 127, 113, 14)
  print("arrows: move   z: recolor", 14, 118, 13)
end
