#pragma once

#include <array>
#include <cstdint>

#include "gfx/color.h"

namespace gfx
{

// 256 colours, stored big-endian RGB565 so compositing needs no swap.
class Palette
{
 public:
  void set(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
  {
    c_[index] = rgb565_be(r, g, b);
  }
  void set_be(uint8_t index, uint16_t c_be)
  {
    c_[index] = c_be;
  }
  uint16_t operator[](uint8_t index) const
  {
    return c_[index];
  }
  const uint16_t* data() const
  {
    return c_.data();
  }

 private:
  std::array<uint16_t, 256> c_{};
};

}  // namespace gfx
