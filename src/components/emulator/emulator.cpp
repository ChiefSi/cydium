#include "emulator.h"

#include <chrono>
#include <cstring>

#include "esp_timer.h"
#include "state.h"

// IDF's esp_timer has only headers on the linux target.
extern "C" int64_t esp_timer_get_time()
{
  using namespace std::chrono;
  return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

namespace emulator
{

State& state()
{
  static State s;
  return s;
}

int64_t now_ms()
{
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void set_orientation(gfx::Orientation o)
{
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  s.orientation = o;
  s.width = gfx::screen_width(o);
  s.height = gfx::screen_height(o);
  std::memset(s.pixels, 0, sizeof(s.pixels));
  ++s.seq;
  s.changed.notify_all();
}

void write(int x, int y, int w, int h, const uint16_t* pixels_be)
{
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  for (int row = 0; row < h; ++row)
  {
    const int sy = y + row;
    if (sy < 0 || sy >= s.height)
      continue;
    const int x0 = x < 0 ? 0 : x;
    const int x1 = x + w > s.width ? s.width : x + w;
    if (x0 < x1)
      std::memcpy(&s.pixels[sy * s.width + x0], &pixels_be[row * w + (x0 - x)],
                  (x1 - x0) * sizeof(uint16_t));
  }
  ++s.seq;
  s.changed.notify_all();
}

void set_invert(bool invert)
{
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  s.invert = invert;
  ++s.seq;
  s.changed.notify_all();
}

void set_backlight(uint8_t level)
{
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  s.backlight = level;
  ++s.seq;
  s.changed.notify_all();
}

Touch touch()
{
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  const bool down = s.pen_down || now_ms() - s.pen_down_at_ms < kMinPressMs;
  return {down, s.pen_x, s.pen_y, s.pen_orientation};
}

bool boot_button()
{
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  return s.button_down || now_ms() - s.button_down_at_ms < kMinPressMs;
}

}  // namespace emulator
