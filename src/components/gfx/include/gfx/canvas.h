#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>

#include "gfx/color.h"
#include "gfx/font.h"
#include "gfx/layer.h"
#include "gfx/rect.h"

namespace gfx
{

// Text alignment relative to the x passed to BasicCanvas::text().
enum class Align : uint8_t
{
  kLeft,
  kCenter,
  kRight,
};

// Drawing primitives shared by the indexed-layer Canvas and the RGB565
// ChunkCanvas. `Rows` maps a y coordinate to a pointer to that row's first
// pixel (x = 0); every draw is clipped to clip().
template <typename Pixel, typename Rows>
class BasicCanvas
{
 public:
  BasicCanvas(Rows rows, Rect bounds) : rows_(rows), bounds_(bounds), clip_(bounds) {}

  const Rect& bounds() const
  {
    return bounds_;
  }
  const Rect& clip() const
  {
    return clip_;
  }
  void set_clip(const Rect& r)
  {
    clip_ = bounds_.intersect(r);
  }
  void reset_clip()
  {
    clip_ = bounds_;
  }

  void pixel(int x, int y, Pixel c)
  {
    if (clip_.contains(x, y))
      rows_(y)[x] = c;
  }

  void fill_rect(const Rect& r, Pixel c)
  {
    const Rect a = r.intersect(clip_);
    for (int y = a.y; y < a.bottom(); ++y)
    {
      Pixel* p = rows_(y);
      std::fill(p + a.x, p + a.right(), c);
    }
  }

  void hline(int x, int y, int w, Pixel c)
  {
    fill_rect({x, y, w, 1}, c);
  }
  void vline(int x, int y, int h, Pixel c)
  {
    fill_rect({x, y, 1, h}, c);
  }

  // Outline, `t` pixels thick, inside r.
  void rect(const Rect& r, Pixel c, int t = 1)
  {
    fill_rect({r.x, r.y, r.w, t}, c);
    fill_rect({r.x, r.bottom() - t, r.w, t}, c);
    fill_rect({r.x, r.y + t, t, r.h - 2 * t}, c);
    fill_rect({r.right() - t, r.y + t, t, r.h - 2 * t}, c);
  }

  // Bresenham line, both end points included.
  void line(int x0, int y0, int x1, int y1, Pixel c)
  {
    const int dx = std::abs(x1 - x0);
    const int dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;)
    {
      pixel(x0, y0, c);
      if (x0 == x1 && y0 == y1)
        break;
      const int e2 = 2 * err;
      if (e2 >= dy)
      {
        err += dy;
        x0 += sx;
      }
      if (e2 <= dx)
      {
        err += dx;
        y0 += sy;
      }
    }
  }

  void circle(int cx, int cy, int r, Pixel c)
  {
    int x = r;
    int y = 0;
    int err = 1 - r;
    while (x >= y)
    {
      pixel(cx + x, cy + y, c);
      pixel(cx - x, cy + y, c);
      pixel(cx + x, cy - y, c);
      pixel(cx - x, cy - y, c);
      pixel(cx + y, cy + x, c);
      pixel(cx - y, cy + x, c);
      pixel(cx + y, cy - x, c);
      pixel(cx - y, cy - x, c);
      ++y;
      if (err < 0)
      {
        err += 2 * y + 1;
      }
      else
      {
        --x;
        err += 2 * (y - x) + 1;
      }
    }
  }

  void fill_circle(int cx, int cy, int r, Pixel c)
  {
    if (!clip_.intersects({cx - r, cy - r, 2 * r + 1, 2 * r + 1}))
      return;
    for (int dy = -r; dy <= r; ++dy)
    {
      int dx = 0;
      while ((dx + 1) * (dx + 1) + dy * dy <= r * r)
        ++dx;
      hline(cx - dx, cy + dy, 2 * dx + 1, c);
    }
  }

  // Draws text with set glyph pixels only (transparent background). Returns
  // the text's width in pixels. Glyphs outside clip() are skipped cheaply.
  int text(int x, int y, const char* s, Pixel c, int scale = 1, Align align = Align::kLeft,
           const Font& f = kFont8x8)
  {
    const int width = f.text_width(s, scale);
    if (align == Align::kCenter)
      x -= width / 2;
    else if (align == Align::kRight)
      x -= width;

    const int gw = f.w * scale;
    const int gh = f.h * scale;
    if (y >= clip_.bottom() || y + gh <= clip_.y)
      return width;
    for (; *s; ++s, x += gw)
    {
      if (x >= clip_.right() || x + gw <= clip_.x)
        continue;
      const uint8_t* g = f.glyph(*s);
      for (int gy = 0; gy < f.h; ++gy)
      {
        const int py = y + gy * scale;
        if (py >= clip_.bottom() || py + scale <= clip_.y || !g[gy])
          continue;
        for (int gx = 0; gx < f.w; ++gx)
          if (g[gy] & (1u << gx))
            fill_rect({x + gx * scale, py, scale, scale}, c);
      }
    }
    return width;
  }

 protected:
  Pixel* row(int y)
  {
    return rows_(y);
  }

  Rows rows_;
  Rect bounds_;
  Rect clip_;
};

struct LayerRows
{
  IndexedLayer* layer;
  uint8_t* operator()(int y) const
  {
    return layer->row(y);
  }
};

// Draws palette indices into an IndexedLayer.
class Canvas : public BasicCanvas<uint8_t, LayerRows>
{
 public:
  explicit Canvas(IndexedLayer& layer)
      : BasicCanvas(LayerRows{&layer}, Rect{0, 0, layer.width(), layer.height()})
  {
  }

  void clear(uint8_t index)
  {
    fill_rect(bounds_, index);
  }

  // Copies a w x h 8-bit sprite to (x, y). Pixels equal to `transparent` are
  // skipped; other indices are offset by `palette_offset` (wrapping).
  void blit8(const uint8_t* src, int w, int h, int x, int y, uint8_t transparent = 0,
             bool flip_x = false, uint8_t palette_offset = 0)
  {
    const Rect a = Rect{x, y, w, h}.intersect(clip_);
    for (int dy = a.y; dy < a.bottom(); ++dy)
    {
      const uint8_t* s = src + (dy - y) * w;
      uint8_t* d = row(dy);
      for (int dx = a.x; dx < a.right(); ++dx)
      {
        const int sx = flip_x ? (w - 1 - (dx - x)) : (dx - x);
        const uint8_t v = s[sx];
        if (v != transparent)
          d[dx] = static_cast<uint8_t>(v + palette_offset);
      }
    }
  }
};

struct ChunkRows
{
  uint16_t* buf;
  int y0;
  int stride;
  uint16_t* operator()(int y) const
  {
    return buf + (y - y0) * stride;
  }
};

// Draws big-endian RGB565 straight into one composited output chunk: the
// rows [y0, y0 + rows) of a `width`-wide screen. Used for overlays (menu,
// icons) on top of the composited layers.
class ChunkCanvas : public BasicCanvas<uint16_t, ChunkRows>
{
 public:
  ChunkCanvas(uint16_t* buf, int width, int y0, int rows)
      : BasicCanvas(ChunkRows{buf, y0, width}, Rect{0, y0, width, rows})
  {
  }

  // Halves the brightness of everything in r.
  void dim(const Rect& r)
  {
    const Rect a = r.intersect(clip_);
    for (int y = a.y; y < a.bottom(); ++y)
    {
      uint16_t* p = row(y);
      for (int x = a.x; x < a.right(); ++x)
        p[x] = dim_be(p[x]);
    }
  }

  // Alpha-blends a w x h big-endian RGB565 bitmap with 8-bit alpha at (x, y).
  void blend(const uint16_t* pixels_be, const uint8_t* alpha, int w, int h, int x, int y)
  {
    const Rect a = Rect{x, y, w, h}.intersect(clip_);
    for (int dy = a.y; dy < a.bottom(); ++dy)
    {
      const int so = (dy - y) * w;
      uint16_t* d = row(dy);
      for (int dx = a.x; dx < a.right(); ++dx)
      {
        const int si = so + dx - x;
        d[dx] = blend_be(d[dx], pixels_be[si], alpha[si]);
      }
    }
  }
};

}  // namespace gfx
