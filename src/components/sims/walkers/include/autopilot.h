#pragma once

#include <array>
#include <cstdint>

#include "level.h"
#include "sim/rng.h"
#include "world.h"

namespace walkers
{

// Plays a level from its plan like a person would: it spots a walker coming
// up to where a skill is needed, picks the skill, moves the cursor over and
// clicks. When `checking`, it plays without mistakes or hesitation, to test
// that a new level can be won. Otherwise it hesitates on a first attempt and
// makes the odd mistake, fewer on each retry, within the level's spare
// walkers.
class Autopilot
{
 public:
  void start(const Level& level, uint32_t seed, int attempt, bool checking);

  // Decides for this tick (call before World::tick()). Returns true when it
  // clicks: give `*skill` to walker `*walker`.
  bool tick(const World& world, int* walker, Skill* skill);

  // Keeps track of the plan without acting, while the user plays.
  void observe(const World& world)
  {
    update_steps(world);
    target_ = -1;
  }

  // For drawing: the skill it has picked (-1 if none), and the walker the
  // cursor is on its way to, with progress 0..256 from the skill button.
  int selected() const
  {
    return target_ >= 0 && think_ == 0 ? static_cast<int>(skill_) : -1;
  }
  int target() const
  {
    return target_;
  }
  int flight() const
  {
    return fly_total_ > 0 ? 256 * (fly_total_ - fly_) / fly_total_ : 256;
  }

 private:
  struct StepState
  {
    bool done = false;
    bool seen = false;  // first sighting gets a long look on a first attempt
    int assigned = -1;  // walker doing it
    int cooldown = 0;
  };

  void update_steps(const World& world);
  bool step_ready(int i) const;
  bool in_zone(const PlanStep& s, const Walker& w) const;
  int approach(const PlanStep& s, const Walker& w) const;  // distance, or -1
  bool choose(const World& world);
  void aim(int walker, Skill skill, int step, bool critical);

  const Level* level_ = nullptr;
  sim::Rng rng_;
  int attempt_ = 0;
  bool checking_ = false;
  std::array<StepState, Level::kMaxSteps> steps_{};
  int target_ = -1;
  Skill skill_ = Skill::kDigger;
  int step_ = -1;  // -1: freeing a blocker at the end
  int think_ = 0;  // ticks before picking the skill
  int fly_ = 0;    // ticks of cursor travel left
  int fly_total_ = 0;
  int wait_ = 0;  // ticks spent hovering over a walker not yet in place
};

// Plays the level out headless with a checking autopilot on the terrain it
// was generated into (which this changes). Returns true if it can be won.
bool verify(const Level& level, Terrain& terrain);

}  // namespace walkers
