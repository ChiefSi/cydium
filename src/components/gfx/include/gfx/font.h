#pragma once

#include <cstdint>
#include <cstring>

namespace gfx
{

// Fixed-width bitmap font. Each glyph is `h` bytes, one per row from the top;
// bit 0 is the leftmost pixel (so w <= 8).
struct Font
{
  const uint8_t* glyphs;
  uint8_t first;
  uint8_t last;
  uint8_t w;
  uint8_t h;

  // Glyph rows for `c`; characters outside the font render as '?'.
  const uint8_t* glyph(char c) const
  {
    auto uc = static_cast<uint8_t>(c);
    if (uc < first || uc > last)
      uc = '?';
    return glyphs + (uc - first) * h;
  }

  int text_width(const char* s, int scale = 1) const
  {
    return static_cast<int>(std::strlen(s)) * w * scale;
  }
};

extern const Font kFont8x8;

}  // namespace gfx
