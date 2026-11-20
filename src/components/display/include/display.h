#pragma once

#include <cstdint>

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "gfx/color.h"
#include "gfx/orientation.h"

namespace display
{

constexpr int kChunkRows = 16;
constexpr int kMaxWidth = 320;  // landscape width; chunk buffers are sized for it

// Sets up SPI2_HOST, the panel IO and the ILI9341 panel in portrait, and two
// DMA-capable chunk buffers of kMaxWidth x kChunkRows RGB565 pixels.
esp_err_t init();

esp_lcd_panel_handle_t panel();

void set_invert(bool invert);
bool inverted();

// Rotates the panel's address space. Waits for in-flight transfers first;
// the screen content is not redrawn (callers repaint after rotating).
void set_orientation(gfx::Orientation o);
gfx::Orientation orientation();
int width();
int height();

// The landscape direction picked in Kconfig (CYD_DISPLAY_LANDSCAPE_*), and
// the default orientation (CYD_DISPLAY_DEFAULT_*).
gfx::Orientation landscape_orientation();
gfx::Orientation default_orientation();

// Kept for existing callers; same as gfx::rgb565_be.
constexpr uint16_t rgb565_be(uint8_t r, uint8_t g, uint8_t b)
{
  return gfx::rgb565_be(r, g, b);
}

// ---- Low-level chunk output (used by Renderer) ----------------------------

// Blocks until a DMA chunk buffer (kMaxWidth * kChunkRows pixels) is free and
// returns it. Buffers are handed out round-robin; each must be passed to
// send() before the next acquire_buffer().
uint16_t* acquire_buffer();

// Queues `rows` full-width rows starting at y from `buf` (from
// acquire_buffer()). Returns immediately; the buffer is released when the
// DMA transfer completes.
void send(int y, int rows, uint16_t* buf);

// Blocks until every queued transfer has completed.
void wait_idle();

// ---- Simple direct drawing (diagnostics, boot) -----------------------------

// Fills [x0, x1) x [y0, y1) with one colour (already byte-swapped), clipped
// to the screen. Blocks until the last chunk has been sent.
void fill_rect(int x0, int y0, int x1, int y1, uint16_t color_be);

// Copies a w x h block of pixels (already byte-swapped) to (x, y) WITHOUT
// clipping, so callers can probe addresses outside the screen. w * h must fit
// in one chunk buffer (kMaxWidth * kChunkRows pixels). Blocks until sent.
void blit(int x, int y, int w, int h, const uint16_t* pixels_be);

// Fills the whole screen with one colour (already byte-swapped). Blocks until
// the last chunk has been sent.
void fill(uint16_t color_be);

}  // namespace display
