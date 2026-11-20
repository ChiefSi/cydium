#pragma once

#include "gfx/layer.h"
#include "gfx/palette.h"
#include "gfx/rect.h"

namespace sim
{

// What a simulation draws on: the background and foreground layers (index 0
// in the foreground is transparent), the palette, and dirty tracking. The
// firmware backs it with the display renderer; tests use a plain fake.
class Surface
{
 public:
  virtual ~Surface() = default;

  virtual gfx::IndexedLayer& bg() = 0;
  virtual gfx::IndexedLayer& fg() = 0;
  virtual gfx::Palette& palette() = 0;

  // Marks an area (or everything) as changed, so it is sent to the panel.
  // Palette changes need invalidate_all().
  virtual void invalidate(const gfx::Rect& r) = 0;
  virtual void invalidate_all() = 0;

  // Screen area covered by something drawn on top (the menu icon), which a
  // simulation should keep its controls out of. Empty if none.
  virtual gfx::Rect reserved()
  {
    return {};
  }

  int width()
  {
    return bg().width();
  }
  int height()
  {
    return bg().height();
  }
};

}  // namespace sim
