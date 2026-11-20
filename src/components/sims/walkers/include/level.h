#pragma once

#include <array>
#include <cstdint>

#include "gfx/rect.h"
#include "terrain.h"
#include "world.h"

namespace walkers
{

// One move of a level's solution: give `skill` to a walker whose feet are on
// row `y` within [x0, x1] (and facing `dir`, unless 0). The step has worked
// once any walker's feet reach `success`.
struct PlanStep
{
  Skill skill;
  int8_t dir;
  int8_t after;   // step that must have worked first, or -1
  bool each;      // umbrella: every walker at or above row y, anywhere
  bool critical;  // walkers die if it comes too late
  int16_t y;
  int16_t x0;
  int16_t x1;
  gfx::Rect success;  // empty for blockers: done while one is blocking
};

struct Level
{
  static constexpr int kMaxSteps = 12;

  uint32_t seed = 0;
  LevelParams params;                       // skills holds the planned counts
  std::array<PlanStep, kMaxSteps> steps{};  // in route order
  int step_count = 0;
  int hatch_x = 0;  // centre of the hatch's bottom edge
  int hatch_y = 0;
  int goal_x = 0;  // centre of the door's bottom edge (on the floor)
  int goal_y = 0;
  int challenges = 0;  // obstacles that need a skill
};

// Draws a random level for `seed` into the terrain and describes it in
// `out`. Keeps the hatch and goal out of `avoid` (the menu icon). Returns
// false if this seed didn't give a usable layout; try another.
bool generate(uint32_t seed, Terrain& terrain, const gfx::Rect& avoid, Level* out);

}  // namespace walkers
