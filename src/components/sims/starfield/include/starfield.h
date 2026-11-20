#pragma once

#include <array>
#include <cstdint>

#include "sim/simulation.h"

namespace starfield
{

// Parallax starfield drifting right to left in three depth layers. A tap
// toggles warp speed.
class Starfield : public sim::Simulation
{
 public:
  const char* name() const override
  {
    return "Starfield";
  }
  const char* description() const override
  {
    return "Parallax stars; tap for warp";
  }

  bool begin(sim::Context& ctx) override;
  void step(sim::Context& ctx, uint32_t dt_ms) override;
  void tap(sim::Context& ctx, int x, int y) override;

 private:
  static constexpr int kNumStars = 120;

  struct Star
  {
    int32_t x;  // in 1/256 pixels
    int16_t y;
    uint8_t depth;  // 0 (far) .. 2 (near)
  };

  void place(Star& star, sim::Context& ctx, bool anywhere);
  void draw(const Star& star, sim::Context& ctx, bool erase);

  std::array<Star, kNumStars> stars_{};
  bool warp_ = false;
};

}  // namespace starfield
