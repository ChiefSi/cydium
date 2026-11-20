#pragma once

#include <cstdint>

#include "esp_err.h"
#include "gfx/canvas.h"
#include "gfx/layer.h"
#include "gfx/palette.h"
#include "gfx/rect.h"

namespace display
{

// Draws on top of the composited layers, one output chunk at a time. Called
// from Renderer::present() for every chunk that is sent; the canvas covers
// only that chunk's rows, so drawing outside them is clipped for free.
class Overlay
{
 public:
  virtual ~Overlay() = default;
  virtual void compose(gfx::ChunkCanvas& canvas) = 0;
};

// Owns the two indexed layers (background and foreground, index 0 in the
// foreground is transparent) and the palette, and pushes them to the panel
// in kChunkRows-row chunks. Only chunks marked dirty are composited and sent;
// two DMA buffers let the next chunk composite while the previous one is
// transferred.
class Renderer
{
 public:
  // Allocates both layers at the display's current size. Call once, after
  // display::init().
  esp_err_t begin();

  gfx::IndexedLayer& bg()
  {
    return bg_;
  }
  gfx::IndexedLayer& fg()
  {
    return fg_;
  }
  gfx::Palette& palette()
  {
    return palette_;
  }

  void set_overlay(Overlay* overlay)
  {
    overlay_ = overlay;
  }

  // Marks the chunks covering r for resending.
  void invalidate(const gfx::Rect& r);
  void invalidate_all();

  // Re-reads the display size after display::set_orientation(): resizes both
  // layers (no reallocation) and marks everything dirty. The layer contents
  // are stale afterwards and must be redrawn.
  void resize();

  // Composites and sends all dirty chunks. Returns the number sent.
  int present();

 private:
  static constexpr int kMaxChunks = 320 / 16;

  gfx::IndexedLayer bg_;
  gfx::IndexedLayer fg_;
  gfx::Palette palette_;
  Overlay* overlay_ = nullptr;
  uint32_t dirty_ = 0;  // one bit per chunk
  int chunks_ = 0;
};

}  // namespace display
