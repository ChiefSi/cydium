#pragma once

#include <cstdint>

#include "gfx/layer.h"
#include "gfx/rect.h"

namespace walkers
{

// Palette layout. The background layer doubles as the collision map: the
// palette index of a pixel says what it is made of, so terrain costs no
// memory beyond the layer itself.
namespace pal
{
constexpr uint8_t kSkyFirst = 1;  // 1..16: sky gradient (air)
constexpr int kSkyCount = 16;
constexpr uint8_t kDecorFirst = 32;  // 32..63: passable scenery and UI (air)
constexpr uint8_t kDirtFirst = 64;   // 64..103: dirt (destructible)
constexpr int kDirtCount = 40;
constexpr uint8_t kBrickFirst = 104;  // 104..111: builder bricks (destructible)
constexpr int kBrickCount = 8;
constexpr uint8_t kSolidEnd = 128;    // first index after the destructible range
constexpr uint8_t kMetalFirst = 128;  // 128..143: metal (indestructible)
constexpr int kMetalCount = 16;
constexpr uint8_t kSpriteFirst = 160;  // 160..255: foreground colours (air in bg)
}  // namespace pal

enum class Material : uint8_t
{
  kAir,
  kDirt,   // dug, blasted and built
  kMetal,  // never changes
};

constexpr Material material_of(uint8_t index)
{
  return index >= pal::kDirtFirst && index < pal::kSolidEnd ? Material::kDirt
         : index >= pal::kMetalFirst && index < pal::kMetalFirst + pal::kMetalCount
             ? Material::kMetal
             : Material::kAir;
}

// The play area: the top `height` rows of the background layer. Outside it,
// the left and right edges are metal walls and above and below is open air
// (walkers that fall out of the bottom die).
class Terrain
{
 public:
  void attach(gfx::IndexedLayer& layer, int height);

  int width() const
  {
    return w_;
  }
  int height() const
  {
    return h_;
  }

  Material at(int x, int y) const
  {
    if (x < 0 || x >= w_)
      return Material::kMetal;
    if (y < 0 || y >= h_)
      return Material::kAir;
    return material_of(layer_->row(y)[x]);
  }
  bool solid(int x, int y) const
  {
    return at(x, y) != Material::kAir;
  }

  // Sky colour for row y, which dug and blasted pixels revert to.
  uint8_t sky(int y) const
  {
    return static_cast<uint8_t>(pal::kSkyFirst + y * pal::kSkyCount / (h_ > 0 ? h_ : 1));
  }

  uint8_t& pixel(int x, int y)
  {
    return layer_->row(y)[x];
  }

  // Fills the whole play area with sky.
  void clear();

  // Turns the dirt pixels of row y in [x0, x1] into sky. Metal is kept.
  void dig_row(int x0, int x1, int y);
  // Clears the dirt within radius r of (cx, cy).
  void blast(int cx, int cy, int r);
  // Puts brick pixels into the air pixels of row y in [x0, x1].
  void brick(int x0, int x1, int y);

  // Area changed since the last call (empty if none); for redrawing.
  gfx::Rect take_changed();

 private:
  void mark(const gfx::Rect& r);

  gfx::IndexedLayer* layer_ = nullptr;
  int w_ = 0;
  int h_ = 0;
  gfx::Rect changed_;
};

}  // namespace walkers
