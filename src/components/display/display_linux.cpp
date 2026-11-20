// Emulator display: the same interface as the ILI9341 driver, drawing into
// the emulator's screen instead. Transfers complete at once.
#include "display.h"
#include "emulator.h"
#include "esp_log.h"

namespace display
{
namespace
{

constexpr const char* kTag = "display";
constexpr int kChunkPixels = kMaxWidth * kChunkRows;
constexpr int kNumBuffers = 2;

uint16_t s_buf[kNumBuffers][kChunkPixels];
int s_next = 0;
bool s_inverted = false;
gfx::Orientation s_orientation = gfx::Orientation::kPortrait;

#ifdef CONFIG_CYD_LCD_INVERT_DEFAULT
constexpr bool kInvertDefault = true;
#else
constexpr bool kInvertDefault = false;
#endif

}  // namespace

esp_err_t init()
{
  emulator::start();
  set_orientation(gfx::Orientation::kPortrait);
  set_invert(kInvertDefault);
  ESP_LOGI(kTag, "emulated %dx%d panel", width(), height());
  return ESP_OK;
}

void set_invert(bool invert)
{
  s_inverted = invert;
  emulator::set_invert(invert);
}

bool inverted()
{
  return s_inverted;
}

void set_orientation(gfx::Orientation o)
{
  s_orientation = o;
  emulator::set_orientation(o);
}

gfx::Orientation orientation()
{
  return s_orientation;
}

int width()
{
  return gfx::screen_width(s_orientation);
}

int height()
{
  return gfx::screen_height(s_orientation);
}

uint16_t* acquire_buffer()
{
  uint16_t* buf = s_buf[s_next];
  s_next = (s_next + 1) % kNumBuffers;
  return buf;
}

void send(int y, int rows, uint16_t* buf)
{
  emulator::write(0, y, width(), rows, buf);
}

void wait_idle() {}

void fill_rect(int x0, int y0, int x1, int y1, uint16_t color_be)
{
  x0 = x0 < 0 ? 0 : x0;
  y0 = y0 < 0 ? 0 : y0;
  x1 = x1 > width() ? width() : x1;
  y1 = y1 > height() ? height() : y1;
  if (x0 >= x1 || y0 >= y1)
    return;
  uint16_t* buf = acquire_buffer();
  const int w = x1 - x0;
  for (int i = 0; i < w; ++i)
    buf[i] = color_be;
  for (int y = y0; y < y1; ++y)
    emulator::write(x0, y, w, 1, buf);
}

void blit(int x, int y, int w, int h, const uint16_t* pixels_be)
{
  if (w > 0 && h > 0 && w * h <= kChunkPixels)
    emulator::write(x, y, w, h, pixels_be);  // the emulator clips
}

void fill(uint16_t color_be)
{
  fill_rect(0, 0, width(), height(), color_be);
}

}  // namespace display
