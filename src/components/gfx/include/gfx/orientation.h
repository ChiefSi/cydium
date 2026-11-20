#pragma once

#include <cstdint>

namespace gfx
{

// Screen orientation as seen by the user, relative to portrait with the USB
// port at the bottom.
enum class Orientation : uint8_t
{
  kPortrait,      // 240 x 320, USB at the bottom
  kLandscapeCw,   // 320 x 240, rotated clockwise: USB on the left
  kLandscapeCcw,  // 320 x 240, rotated anticlockwise: USB on the right
};

constexpr int kPortraitWidth = 240;
constexpr int kPortraitHeight = 320;

constexpr bool is_landscape(Orientation o)
{
  return o != Orientation::kPortrait;
}

constexpr int screen_width(Orientation o)
{
  return is_landscape(o) ? kPortraitHeight : kPortraitWidth;
}

constexpr int screen_height(Orientation o)
{
  return is_landscape(o) ? kPortraitWidth : kPortraitHeight;
}

}  // namespace gfx
