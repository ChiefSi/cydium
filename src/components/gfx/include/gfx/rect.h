#pragma once

#include <algorithm>

namespace gfx
{

struct Rect
{
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  constexpr int right() const
  {
    return x + w;
  }  // exclusive
  constexpr int bottom() const
  {
    return y + h;
  }  // exclusive
  constexpr bool empty() const
  {
    return w <= 0 || h <= 0;
  }

  constexpr bool contains(int px, int py) const
  {
    return px >= x && px < x + w && py >= y && py < y + h;
  }

  constexpr bool intersects(const Rect& o) const
  {
    return !intersect(o).empty();
  }

  constexpr Rect intersect(const Rect& o) const
  {
    const int l = std::max(x, o.x);
    const int t = std::max(y, o.y);
    const int r = std::min(right(), o.right());
    const int b = std::min(bottom(), o.bottom());
    return (r > l && b > t) ? Rect{l, t, r - l, b - t} : Rect{};
  }

  constexpr bool operator==(const Rect& o) const
  {
    return x == o.x && y == o.y && w == o.w && h == o.h;
  }
};

}  // namespace gfx
