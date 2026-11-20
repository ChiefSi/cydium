#include "terrain.h"

#include <algorithm>

namespace walkers
{

void Terrain::attach(gfx::IndexedLayer& layer, int height)
{
  layer_ = &layer;
  w_ = layer.width();
  h_ = std::min(height, layer.height());
  changed_ = {};
}

void Terrain::clear()
{
  for (int y = 0; y < h_; ++y)
  {
    uint8_t* row = layer_->row(y);
    std::fill(row, row + w_, sky(y));
  }
  mark({0, 0, w_, h_});
}

void Terrain::dig_row(int x0, int x1, int y)
{
  if (y < 0 || y >= h_)
    return;
  x0 = std::max(x0, 0);
  x1 = std::min(x1, w_ - 1);
  uint8_t* row = layer_->row(y);
  for (int x = x0; x <= x1; ++x)
    if (material_of(row[x]) == Material::kDirt)
      row[x] = sky(y);
  mark({x0, y, x1 - x0 + 1, 1});
}

void Terrain::blast(int cx, int cy, int r)
{
  for (int dy = -r; dy <= r; ++dy)
  {
    const int y = cy + dy;
    if (y < 0 || y >= h_)
      continue;
    uint8_t* row = layer_->row(y);
    for (int dx = -r; dx <= r; ++dx)
    {
      const int x = cx + dx;
      if (x >= 0 && x < w_ && dx * dx + dy * dy <= r * r && material_of(row[x]) == Material::kDirt)
        row[x] = sky(y);
    }
  }
  mark({cx - r, cy - r, 2 * r + 1, 2 * r + 1});
}

void Terrain::brick(int x0, int x1, int y)
{
  if (y < 0 || y >= h_)
    return;
  if (x0 > x1)
    std::swap(x0, x1);
  x0 = std::max(x0, 0);
  x1 = std::min(x1, w_ - 1);
  uint8_t* row = layer_->row(y);
  for (int x = x0; x <= x1; ++x)
    if (material_of(row[x]) == Material::kAir)
      row[x] = static_cast<uint8_t>(pal::kBrickFirst + ((x + y) & 1) * 2 + (x == x0 || x == x1));
  mark({x0, y, x1 - x0 + 1, 1});
}

void Terrain::mark(const gfx::Rect& r)
{
  const gfx::Rect a = r.intersect({0, 0, w_, h_});
  if (a.empty())
    return;
  if (changed_.empty())
  {
    changed_ = a;
    return;
  }
  const int x0 = std::min(changed_.x, a.x);
  const int y0 = std::min(changed_.y, a.y);
  changed_ = {x0, y0, std::max(changed_.right(), a.right()) - x0,
              std::max(changed_.bottom(), a.bottom()) - y0};
}

gfx::Rect Terrain::take_changed()
{
  const gfx::Rect r = changed_;
  changed_ = {};
  return r;
}

}  // namespace walkers
