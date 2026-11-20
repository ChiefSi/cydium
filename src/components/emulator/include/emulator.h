#pragma once

#include <cstdint>

#include "gfx/orientation.h"

// Host side of the emulator image (IDF linux target only). The display,
// touch and board components call into it instead of the hardware; a web
// page shows the screen and sends mouse presses back as touches.
namespace emulator
{

// Starts the web server (once) and logs its address. The address and port
// come from the command line (--addr, --port) or environment (see options.h).
void start();

// Sets the screen orientation and clears the screen.
void set_orientation(gfx::Orientation o);

// Copies a w x h block of big-endian RGB565 pixels to (x, y), clipped to the
// screen.
void write(int x, int y, int w, int h, const uint16_t* pixels_be);

void set_invert(bool invert);
void set_backlight(uint8_t level);

// The pen, in the coordinates of the screen as it was shown when the mouse
// was pressed. A press reads as down for at least kMinPressMs, so a quick
// click isn't missed by the 20 ms touch polling.
struct Touch
{
  bool down;
  int x;
  int y;
  gfx::Orientation orientation;
};
constexpr int kMinPressMs = 60;
Touch touch();

bool boot_button();

}  // namespace emulator
