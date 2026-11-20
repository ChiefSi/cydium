// Diagnostic image: helps identify a board's panel settings (mirroring,
// red/blue order, inversion). See "Diagnostic image" in README.md.
//
// Each touch draws the next 40x40 numbered block, tiled
// left to right, top to bottom over the 240x320 portrait screen, i.e. 6
// columns x 8 rows, ids 0..47. Blocks are drawn unclipped. After block 47 the
// next touch clears and restarts. BOOT toggles colour inversion.

#include <cstring>

#include "board.h"
#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch.h"

namespace
{

constexpr const char* kTag = "app";
constexpr int kBlock = 40;
constexpr int kCols = 6;  // x = 0, 40, ... 200
constexpr int kRows = 8;  // y = 0, 40, ... 280
constexpr int kNumBlocks = kCols * kRows;
constexpr int kScale = 2;  // 5x7 font drawn at 10x14

// 5x7 digits, one byte per row, bit 4 = leftmost column.
constexpr uint8_t kDigits[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
};

// Three colours: horizontal neighbours (id +/- 1) and vertical neighbours
// (id +/- 6) always differ.
constexpr uint16_t kPalette[] = {
    display::rgb565_be(255, 0, 0),  // red
    display::rgb565_be(0, 255, 0),  // green
    display::rgb565_be(0, 0, 255),  // blue
};
constexpr const char* kPaletteNames[] = {"red", "green", "blue"};
constexpr uint16_t kText = display::rgb565_be(255, 255, 255);

uint16_t s_pixels[kBlock * kBlock];

void draw_digit(int digit, int x0, int y0)
{
  for (int row = 0; row < 7; ++row)
    for (int col = 0; col < 5; ++col)
      if (kDigits[digit][row] & (0x10 >> col))
        for (int dy = 0; dy < kScale; ++dy)
          for (int dx = 0; dx < kScale; ++dx)
            s_pixels[(y0 + row * kScale + dy) * kBlock + x0 + col * kScale + dx] = kText;
}

void draw_block(int id)
{
  const int x = (id % kCols) * kBlock;
  const int y = (id / kCols) * kBlock;
  const int colour = (id % kCols + id / kCols) % 3;

  for (auto& p : s_pixels)
    p = kPalette[colour];

  constexpr int kGlyphW = 5 * kScale;
  constexpr int kGlyphH = 7 * kScale;
  constexpr int kGap = kScale;
  const int digits = id >= 10 ? 2 : 1;
  const int text_w = digits * kGlyphW + (digits - 1) * kGap;
  int tx = (kBlock - text_w) / 2;
  const int ty = (kBlock - kGlyphH) / 2;
  if (digits == 2)
  {
    draw_digit(id / 10, tx, ty);
    tx += kGlyphW + kGap;
  }
  draw_digit(id % 10, tx, ty);

  display::blit(x, y, kBlock, kBlock, s_pixels);
  ESP_LOGI(kTag, "block %2d at x=%3d y=%3d (%s)", id, x, y, kPaletteNames[colour]);
}

}  // namespace

extern "C" void app_main()
{
  ESP_ERROR_CHECK(board::init());
  ESP_ERROR_CHECK(display::init());
  board::set_backlight(255);
  ESP_ERROR_CHECK(touch::init());

  ESP_LOGI(kTag, "block test: touch to draw the next 40x40 block (0..%d), BOOT toggles inversion",
           kNumBlocks - 1);

  int next = 0;
  bool was_down = false;
  bool button_was_down = false;
  int up_ms = 0;
  for (;;)
  {
    touch::RawSample s;
    const bool down = touch::irq_active() && touch::read_raw(&s);
    if (down)
    {
      if (!was_down)
      {
        if (next == kNumBlocks)
        {
          display::fill(0);
          ESP_LOGI(kTag, "cleared, restarting at block 0");
          next = 0;
        }
        draw_block(next++);
      }
      was_down = true;
      up_ms = 0;
    }
    else if (was_down && (up_ms += 20) >= 150)
    {
      was_down = false;  // only re-arm after a clear 150 ms release
    }

    const bool button = board::boot_button_down();
    if (button && !button_was_down)
    {
      display::set_invert(!display::inverted());
      ESP_LOGI(kTag, "colour inversion %s", display::inverted() ? "ON" : "OFF");
    }
    button_was_down = button;

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
