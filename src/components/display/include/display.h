#pragma once

#include <cstdint>

#include "esp_err.h"
#include "esp_lcd_types.h"

namespace display
{

constexpr int kChunkRows = 16;

// Sets up SPI2_HOST, the panel IO and the ILI9341 panel, and two DMA-capable
// chunk buffers of 240 x kChunkRows RGB565 pixels.
esp_err_t init();

esp_lcd_panel_handle_t panel();

void set_invert(bool invert);
bool inverted();

// Converts 8-bit RGB to RGB565, byte-swapped for the ILI9341 (big-endian).
constexpr uint16_t rgb565_be(uint8_t r, uint8_t g, uint8_t b)
{
  uint16_t c = static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
  return static_cast<uint16_t>((c >> 8) | (c << 8));
}

// Fills the whole screen with one colour (already byte-swapped). Blocks until
// the last chunk has been sent.
void fill(uint16_t color_be);

}  // namespace display
