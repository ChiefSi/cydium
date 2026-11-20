#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>

#include "gfx/orientation.h"

namespace emulator
{

// Shared between the firmware tasks (writers) and the web server threads.
struct State
{
  std::mutex mutex;
  std::condition_variable changed;  // signalled whenever seq changes

  // Screen, in the current orientation, big-endian RGB565 as sent.
  uint32_t seq = 1;
  gfx::Orientation orientation = gfx::Orientation::kPortrait;
  int width = gfx::kPortraitWidth;
  int height = gfx::kPortraitHeight;
  uint16_t pixels[gfx::kPortraitWidth * gfx::kPortraitHeight] = {};
  bool invert = false;
  uint8_t backlight = 0;

  // Input from the page.
  bool pen_down = false;
  int64_t pen_down_at_ms = -1000000;
  int pen_x = 0;
  int pen_y = 0;
  gfx::Orientation pen_orientation = gfx::Orientation::kPortrait;
  bool button_down = false;
  int64_t button_down_at_ms = -1000000;
};

State& state();
int64_t now_ms();

}  // namespace emulator
