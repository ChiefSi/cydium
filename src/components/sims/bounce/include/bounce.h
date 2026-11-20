#pragma once

#include <array>
#include <cstdint>

#include "gfx/rect.h"
#include "sim/simulation.h"

namespace bounce
{

// Renderer test scene: a gradient and grid on the background layer and
// bouncing balls on the foreground. A tap moves the first ball there.
class Bounce : public sim::Simulation
{
 public:
  const char* name() const override
  {
    return "Bounce";
  }
  const char* description() const override
  {
    return "Renderer test: bouncing balls";
  }

  bool begin(sim::Context& ctx) override;
  void step(sim::Context& ctx, uint32_t dt_ms) override;
  void tap(sim::Context& ctx, int x, int y) override;

 private:
  static constexpr int kNumBalls = 5;

  struct Ball
  {
    int32_t x, y;    // centre, in 1/256 pixels
    int32_t vx, vy;  // pixels per second
    uint8_t colour;
  };

  static gfx::Rect bounds(const Ball& b);
  void draw(sim::Context& ctx);

  std::array<Ball, kNumBalls> balls_{};
};

}  // namespace bounce
