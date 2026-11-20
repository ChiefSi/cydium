#pragma once

// Hardware-free mapping from raw XPT2046 readings to screen pixels, so it can
// be unit-tested on the host.

#include <cstdint>

#include "gfx/orientation.h"

namespace touch
{

struct Point
{
  int16_t x;
  int16_t y;
};

// Raw-to-portrait calibration. After an optional axis swap, raw values from
// *_min to *_max map linearly across the portrait screen (240 x 320, USB at
// the bottom), then each screen axis can be flipped.
struct Calibration
{
  uint16_t x_min;  // raw X reading range
  uint16_t x_max;
  uint16_t y_min;  // raw Y reading range
  uint16_t y_max;
  bool swap_xy;  // raw X measures the screen's vertical axis
  bool flip_x;   // screen x runs right-to-left after scaling
  bool flip_y;   // screen y runs bottom-to-top after scaling
};

namespace detail
{
constexpr int scale(int raw, int lo, int hi, int size)
{
  if (hi == lo)
    return 0;
  int v = (raw - lo) * size / (hi - lo);
  return v < 0 ? 0 : (v >= size ? size - 1 : v);
}
}  // namespace detail

// Raw reading -> portrait pixel (0..239, 0..319).
constexpr Point map_to_portrait(uint16_t raw_x, uint16_t raw_y, const Calibration& c)
{
  const int a = c.swap_xy ? raw_y : raw_x;  // drives screen x
  const int b = c.swap_xy ? raw_x : raw_y;  // drives screen y
  int x = c.swap_xy ? detail::scale(a, c.y_min, c.y_max, gfx::kPortraitWidth)
                    : detail::scale(a, c.x_min, c.x_max, gfx::kPortraitWidth);
  int y = c.swap_xy ? detail::scale(b, c.x_min, c.x_max, gfx::kPortraitHeight)
                    : detail::scale(b, c.y_min, c.y_max, gfx::kPortraitHeight);
  if (c.flip_x)
    x = gfx::kPortraitWidth - 1 - x;
  if (c.flip_y)
    y = gfx::kPortraitHeight - 1 - y;
  return {static_cast<int16_t>(x), static_cast<int16_t>(y)};
}

// Portrait pixel -> pixel in orientation o.
constexpr Point rotate_from_portrait(Point p, gfx::Orientation o)
{
  switch (o)
  {
    case gfx::Orientation::kLandscapeCw:  // USB on the left
      return {static_cast<int16_t>(gfx::kPortraitHeight - 1 - p.y), p.x};
    case gfx::Orientation::kLandscapeCcw:  // USB on the right
      return {p.y, static_cast<int16_t>(gfx::kPortraitWidth - 1 - p.x)};
    case gfx::Orientation::kPortrait:
    default:
      return p;
  }
}

constexpr Point map_to_screen(uint16_t raw_x, uint16_t raw_y, const Calibration& c,
                              gfx::Orientation o)
{
  return rotate_from_portrait(map_to_portrait(raw_x, raw_y, c), o);
}

// A press on a known portrait target: screen position and averaged raw reading.
struct CalPoint
{
  int16_t sx;
  int16_t sy;
  uint16_t rx;
  uint16_t ry;
};

namespace detail
{
struct AxisFit
{
  uint16_t min;
  uint16_t max;
  bool flip;
};

// Extrapolates the raw values at screen 0 and `size` from two (screen, raw)
// pairs, and expresses them as a min/max range plus flip.
inline AxisFit fit_axis(float s0, float r0, float s1, float r1, int size)
{
  const float slope = (r1 - r0) / (s1 - s0);
  const float at0 = r0 - slope * s0;
  const float at_size = r0 + slope * (size - s0);
  auto clamp = [](float v)
  { return static_cast<uint16_t>(v < 0 ? 0 : (v > 4095 ? 4095 : v + 0.5f)); };
  if (at0 <= at_size)
    return {clamp(at0), clamp(at_size), false};
  return {clamp(at_size), clamp(at0), true};
}
}  // namespace detail

// Fits a calibration from presses on four portrait targets, in the order
// top-left, top-right, bottom-right, bottom-left. Detects the axis swap from
// which raw channel changes more between left and right targets.
inline Calibration fit_calibration(const CalPoint (&p)[4])
{
  auto diff = [](int a, int b) { return a > b ? a - b : b - a; };
  const int horiz_rx = diff(p[1].rx, p[0].rx) + diff(p[2].rx, p[3].rx);
  const int horiz_ry = diff(p[1].ry, p[0].ry) + diff(p[2].ry, p[3].ry);
  const bool swap = horiz_ry > horiz_rx;

  // a: raw channel that drives screen x; b: screen y.
  auto a = [&](const CalPoint& q) { return static_cast<float>(swap ? q.ry : q.rx); };
  auto b = [&](const CalPoint& q) { return static_cast<float>(swap ? q.rx : q.ry); };

  const detail::AxisFit fx =
      detail::fit_axis((p[0].sx + p[3].sx) / 2.0f, (a(p[0]) + a(p[3])) / 2,
                       (p[1].sx + p[2].sx) / 2.0f, (a(p[1]) + a(p[2])) / 2, gfx::kPortraitWidth);
  const detail::AxisFit fy =
      detail::fit_axis((p[0].sy + p[1].sy) / 2.0f, (b(p[0]) + b(p[1])) / 2,
                       (p[2].sy + p[3].sy) / 2.0f, (b(p[2]) + b(p[3])) / 2, gfx::kPortraitHeight);

  Calibration c{};
  c.swap_xy = swap;
  // map_to_portrait scales screen x with the Y range when swapped.
  if (swap)
  {
    c.y_min = fx.min, c.y_max = fx.max;
    c.x_min = fy.min, c.x_max = fy.max;
  }
  else
  {
    c.x_min = fx.min, c.x_max = fx.max;
    c.y_min = fy.min, c.y_max = fy.max;
  }
  c.flip_x = fx.flip;
  c.flip_y = fy.flip;
  return c;
}

}  // namespace touch
