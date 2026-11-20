#include "gfx/layer.h"

#include <cstdlib>
#include <cstring>

namespace gfx
{

bool IndexedLayer::begin(int w, int h)
{
  for (auto& s : strips_)
  {
    if (!s)
      s = static_cast<uint8_t*>(std::malloc(kStripBytes));
    if (!s)
    {
      end();
      return false;
    }
  }
  return resize(w, h);
}

void IndexedLayer::end()
{
  for (auto& s : strips_)
  {
    std::free(s);
    s = nullptr;
  }
  w_ = h_ = 0;
}

bool IndexedLayer::resize(int w, int h)
{
  if (!strips_[0] || w <= 0 || h <= 0 || w * h != kPixels || h % kStrips != 0)
    return false;
  w_ = w;
  h_ = h;
  strip_rows_ = h / kStrips;
  return true;
}

void IndexedLayer::clear(uint8_t index)
{
  for (auto* s : strips_)
    if (s)
      std::memset(s, index, kStripBytes);
}

}  // namespace gfx
