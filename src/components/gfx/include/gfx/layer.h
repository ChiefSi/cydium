#pragma once

#include <cstdint>

namespace gfx
{

// One byte per pixel (palette index) covering the whole screen, held in
// kStrips separate allocations so no single block exceeds ~19 KB. Portrait
// (240x320) and landscape (320x240) need the same bytes, so resize() only
// reinterprets the strips and never reallocates.
class IndexedLayer
{
 public:
  static constexpr int kStrips = 4;
  static constexpr int kPixels = 240 * 320;
  static constexpr int kStripBytes = kPixels / kStrips;

  IndexedLayer() = default;
  ~IndexedLayer()
  {
    end();
  }
  IndexedLayer(const IndexedLayer&) = delete;
  IndexedLayer& operator=(const IndexedLayer&) = delete;

  // Allocates the strips (once) and sets the size. w * h must equal kPixels
  // and h must be a multiple of kStrips. Returns false on failure.
  bool begin(int w, int h);
  void end();
  bool resize(int w, int h);

  int width() const
  {
    return w_;
  }
  int height() const
  {
    return h_;
  }

  uint8_t* row(int y)
  {
    return strips_[y / strip_rows_] + (y % strip_rows_) * w_;
  }
  const uint8_t* row(int y) const
  {
    return strips_[y / strip_rows_] + (y % strip_rows_) * w_;
  }

  void clear(uint8_t index);

 private:
  uint8_t* strips_[kStrips] = {};
  int w_ = 0;
  int h_ = 0;
  int strip_rows_ = 1;
};

}  // namespace gfx
