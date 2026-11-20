#pragma once

#include <array>
#include <cstdint>

#include "autopilot.h"
#include "gfx/rect.h"
#include "level.h"
#include "sim/simulation.h"
#include "terrain.h"
#include "world.h"

namespace walkers
{

// How many skills a level gives, compared with what its plan needs.
enum class Difficulty : uint8_t
{
  kEasy,    // unlimited
  kMedium,  // a few spare of each
  kHard,    // one spare of each
};

// Walkers: an original scene with Lemmings-style mechanics. Small walkers
// drop from a hatch and must be guided to the goal with skills. An autopilot
// plays by default; touching the screen takes over until 20 s without a
// touch.
class Walkers : public sim::Simulation
{
 public:
  const char* name() const override
  {
    return "Walkers";
  }
  const char* description() const override
  {
    return "Guide the walkers home";
  }

  bool begin(sim::Context& ctx) override;
  void step(sim::Context& ctx, uint32_t dt_ms) override;
  void tap(sim::Context& ctx, int x, int y) override;

  // Applies from the next level.
  void set_difficulty(Difficulty d)
  {
    difficulty_ = d;
  }

 private:
  enum class Phase : uint8_t
  {
    kIntro,  // level banner
    kPlaying,
    kResult,  // won: next level shortly; lost: retry or new
  };

  struct Particle
  {
    int16_t x, y;    // 1/16 px
    int16_t vx, vy;  // 1/16 px per tick
    uint8_t life;
    uint8_t colour;
  };

  static constexpr int kMaxParticles = 64;
  static constexpr int kMaxDrawn = World::kMaxWalkers + kMaxParticles + 8;

  void new_level(sim::Context& ctx, bool retry);
  void layout(sim::Context& ctx);
  void draw_bar(sim::Context& ctx);
  void draw_status(sim::Context& ctx);
  void redraw_fg(sim::Context& ctx);
  void logic_tick(sim::Context& ctx);
  void effects();
  void end_level(sim::Context& ctx);
  int button_at(int x, int y) const;
  int walker_at(int x, int y, Skill s) const;
  bool user_active() const;

  Terrain terrain_;
  World world_;
  Level level_;
  Autopilot pilot_;
  Difficulty difficulty_ = Difficulty::kMedium;
  Phase phase_ = Phase::kIntro;
  uint32_t seed_ = 1;
  int attempt_ = 0;
  int level_number_ = 0;
  int play_h_ = 0;
  gfx::Rect bar_;
  gfx::Rect reserved_;
  std::array<gfx::Rect, kSkillCount> buttons_{};
  gfx::Rect retry_button_;
  gfx::Rect new_button_;
  int selected_ = -1;  // the user's chosen skill
  uint32_t acc_ms_ = 0;
  uint32_t phase_ms_ = 0;
  uint32_t since_touch_ms_ = 1u << 30;
  uint32_t since_progress_ms_ = 0;
  int last_saved_ = 0;
  int last_lost_ = 0;
  uint32_t status_key_ = 0;  // what the bar shows, to redraw only on change
  std::array<Particle, kMaxParticles> particles_{};
  std::array<gfx::Rect, kMaxDrawn> drawn_{};
  int drawn_count_ = 0;
  bool hatch_open_ = false;
};

}  // namespace walkers
