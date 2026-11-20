#pragma once

#include <cstdint>

namespace gfx
{

// RGB565 in the CPU's native byte order.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t swap16(uint16_t v)
{
  return static_cast<uint16_t>((v >> 8) | (v << 8));
}

// RGB565 byte-swapped (big-endian), as the ILI9341 expects. All colours that
// reach the panel use this form.
constexpr uint16_t rgb565_be(uint8_t r, uint8_t g, uint8_t b)
{
  return swap16(rgb565(r, g, b));
}

// Halves each channel of a big-endian RGB565 colour.
constexpr uint16_t dim_be(uint16_t c_be)
{
  return swap16(static_cast<uint16_t>((swap16(c_be) >> 1) & 0x7BEF));
}

// Blends big-endian RGB565 src over dst with alpha 0 (dst) .. 255 (src).
inline uint16_t blend_be(uint16_t dst_be, uint16_t src_be, uint8_t alpha)
{
  if (alpha == 255)
    return src_be;
  if (alpha == 0)
    return dst_be;
  const uint16_t d = swap16(dst_be);
  const uint16_t s = swap16(src_be);
  const int a = alpha;
  const int ia = 255 - a;
  const int r = (((s >> 11) & 0x1F) * a + ((d >> 11) & 0x1F) * ia + 127) / 255;
  const int g = (((s >> 5) & 0x3F) * a + ((d >> 5) & 0x3F) * ia + 127) / 255;
  const int b = ((s & 0x1F) * a + (d & 0x1F) * ia + 127) / 255;
  return swap16(static_cast<uint16_t>((r << 11) | (g << 5) | b));
}

}  // namespace gfx
