// Diagnostic image: helps identify a board's panel settings (mirroring,
// red/blue order, inversion). See "Diagnostic image" in README.md.
//
// Each touch draws the next 40x40 numbered block, tiled
// left to right, top to bottom over the 240x320 portrait screen, i.e. 6
// columns x 8 rows, ids 0..47. Blocks are drawn unclipped. After block 47 the
// next touch clears and restarts. BOOT toggles colour inversion.
//
// Touch calibration: at boot, four targets are shown in turn. The presses are
// fitted to a calibration, which is logged as CYD_TOUCH_* Kconfig lines and
// used for the rest of the session: each block-test press draws a crosshair
// where it maps, and raw/mapped coordinates are logged while held.

#include <cstdio>
#include <cstring>

#include "board.h"
#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gfx/canvas.h"
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

// Crosshair with a black outline at (x, y).
void draw_crosshair(int x, int y, uint16_t colour = display::rgb565_be(255, 255, 255))
{
  constexpr int kArm = 10;
  const uint16_t black = display::rgb565_be(0, 0, 0);
  display::fill_rect(x - kArm - 1, y - 2, x + kArm + 2, y + 3, black);
  display::fill_rect(x - 2, y - kArm - 1, x + 3, y + kArm + 2, black);
  display::fill_rect(x - kArm, y - 1, x + kArm + 1, y + 2, colour);
  display::fill_rect(x - 1, y - kArm, x + 2, y + kArm + 1, colour);
}

// One line of text, centred on a black band 16 px high at y.
void draw_message(int y, const char* text)
{
  static uint16_t band[240 * 16];
  gfx::ChunkCanvas c(band, 240, y, 16);
  c.fill_rect(c.bounds(), 0);
  const int scale = gfx::kFont8x8.text_width(text, 2) <= 240 ? 2 : 1;
  c.text(120, y + (16 - 8 * scale) / 2, text, display::rgb565_be(255, 255, 255), scale,
         gfx::Align::kCenter);
  display::blit(0, y, 240, 16, band);
}

// Waits for one press and returns its raw reading, averaged over the time the
// screen is held. Needs a clear 150 ms release to finish.
touch::RawSample wait_for_press()
{
  uint32_t sx = 0, sy = 0, n = 0;
  int up_ms = 0;
  for (;;)
  {
    touch::RawSample s;
    if (touch::irq_active() && touch::read_raw(&s))
    {
      sx += s.x;
      sy += s.y;
      ++n;
      up_ms = 0;
    }
    else if (n >= 3 && (up_ms += 20) >= 150)
    {
      return {static_cast<uint16_t>(sx / n), static_cast<uint16_t>(sy / n), 0};
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// Shows four targets in turn and fits a touch calibration to the presses.
touch::Calibration calibrate()
{
  constexpr int kInset = 20;
  const touch::CalPoint targets[4] = {
      {kInset, kInset, 0, 0},
      {gfx::kPortraitWidth - 1 - kInset, kInset, 0, 0},
      {gfx::kPortraitWidth - 1 - kInset, gfx::kPortraitHeight - 1 - kInset, 0, 0},
      {kInset, gfx::kPortraitHeight - 1 - kInset, 0, 0},
  };
  touch::CalPoint points[4];
  const uint16_t yellow = display::rgb565_be(255, 220, 0);
  char msg[32];
  for (int i = 0; i < 4; ++i)
  {
    display::fill(0);
    std::snprintf(msg, sizeof(msg), "Calibrate: tap %d/4", i + 1);
    draw_message(152, msg);
    draw_crosshair(targets[i].sx, targets[i].sy, yellow);
    ESP_LOGI(kTag, "calibration: tap the target at (%d, %d)", targets[i].sx, targets[i].sy);
    const touch::RawSample s = wait_for_press();
    points[i] = {targets[i].sx, targets[i].sy, s.x, s.y};
    ESP_LOGI(kTag, "calibration: target (%d, %d) raw x=%u y=%u", targets[i].sx, targets[i].sy, s.x,
             s.y);
  }

  const touch::Calibration c = touch::fit_calibration(points);
  ESP_LOGI(kTag, "calibration done; to make it the default, add to src/sdkconfig.defaults:");
  printf("CONFIG_CYD_TOUCH_X_MIN=%u\nCONFIG_CYD_TOUCH_X_MAX=%u\n", c.x_min, c.x_max);
  printf("CONFIG_CYD_TOUCH_Y_MIN=%u\nCONFIG_CYD_TOUCH_Y_MAX=%u\n", c.y_min, c.y_max);
  printf(c.swap_xy ? "CONFIG_CYD_TOUCH_SWAP_XY=y\n" : "# CONFIG_CYD_TOUCH_SWAP_XY is not set\n");
  printf(c.flip_x ? "CONFIG_CYD_TOUCH_FLIP_X=y\n" : "# CONFIG_CYD_TOUCH_FLIP_X is not set\n");
  printf(c.flip_y ? "CONFIG_CYD_TOUCH_FLIP_Y=y\n" : "# CONFIG_CYD_TOUCH_FLIP_Y is not set\n");
  return c;
}

}  // namespace

extern "C" void app_main()
{
  ESP_ERROR_CHECK(board::init());
  ESP_ERROR_CHECK(display::init());
  board::set_backlight(255);
  ESP_ERROR_CHECK(touch::init());

  const touch::Calibration& k = touch::calibration();
  ESP_LOGI(kTag, "Kconfig touch calibration: x %u..%u, y %u..%u, swap=%d flip_x=%d flip_y=%d",
           k.x_min, k.x_max, k.y_min, k.y_max, k.swap_xy, k.flip_x, k.flip_y);
  const touch::Calibration cal = calibrate();

  display::fill(0);
  draw_message(152, "Tap anywhere");
  ESP_LOGI(kTag,
           "block test (using the new calibration): touch to draw the next 40x40 block (0..%d), "
           "BOOT toggles inversion",
           kNumBlocks - 1);

  int next = 0;
  bool was_down = false;
  bool button_was_down = false;
  int up_ms = 0;
  int log_ms = 0;
  for (;;)
  {
    touch::RawSample s;
    const bool down = touch::irq_active() && touch::read_raw(&s);
    if (down)
    {
      const touch::Point p = touch::map_to_screen(s.x, s.y, cal, display::orientation());
      if (!was_down)
      {
        if (next == 0 || next == kNumBlocks)
        {
          display::fill(0);
          if (next == kNumBlocks)
            ESP_LOGI(kTag, "cleared, restarting at block 0");
          next = 0;
        }
        draw_block(next++);
        draw_crosshair(p.x, p.y);
        log_ms = 0;
      }
      if ((log_ms -= 20) <= 0)
      {
        ESP_LOGI("touch", "raw x=%4u y=%4u z=%4u -> screen x=%3d y=%3d", s.x, s.y, s.z, p.x, p.y);
        log_ms = 200;
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
