#pragma once

#include <array>
#include <cstdint>

#include "gfx/rect.h"
#include "sim/rng.h"
#include "terrain.h"

namespace walkers
{

// Logic runs at a fixed rate, independent of the frame rate.
constexpr int kTickMs = 50;
constexpr int kTicksPerSecond = 1000 / kTickMs;

// Walker geometry and physics, in pixels and ticks. A walker's (x, y) is the
// pixel its feet are in; it stands on (x, y + 1).
constexpr int kWalkerH = 10;
constexpr int kStepUp = 4;        // highest step walked up; anything taller turns them
constexpr int kStepDown = 3;      // deeper drops are falls
constexpr int kFallSpeed = 3;     // pixels per tick
constexpr int kFloatSpeed = 1;    // with the umbrella open
constexpr int kUmbrellaOpen = 8;  // fall distance before the umbrella opens
constexpr int kSplatFall = 56;    // falls longer than this kill
constexpr int kBlockReach = 3;    // a blocker turns walkers this close
constexpr int kDigHalfWidth = 4;
constexpr int kDigTicks = 2;  // per row
constexpr int kBuildBricks = 12;
constexpr int kBrickWidth = 6;
constexpr int kBuildTicks = 2;  // per brick: one brick and one step up per 2 ticks
constexpr int kBlastRadius = 10;
constexpr int kFuseTicks = 3 * kTicksPerSecond;
constexpr int kShrugTicks = 10;
constexpr int kSplatTicks = 12;
constexpr int kExitTicks = 10;
constexpr int kHairColours = 8;

enum class Skill : uint8_t
{
  kDigger,
  kBuilder,
  kExploder,
  kBlocker,
  kUmbrella,  // an add-on: combines with any one other skill
  kCount,
};
constexpr int kSkillCount = static_cast<int>(Skill::kCount);
constexpr int kUnlimited = -1;

enum class State : uint8_t
{
  kFalling,
  kWalking,
  kDigging,
  kBuilding,
  kShrugging,  // builder out of bricks
  kBlocking,
  kSplatting,
  kExiting,
  kDead,
  kSaved,
};

struct Walker
{
  int16_t x;
  int16_t y;
  int8_t dir;  // -1 left, +1 right
  State state;
  uint8_t hair;  // 0 .. kHairColours - 1
  bool umbrella;
  int16_t fall_from;  // y where the current fall started
  int16_t fuse;       // ticks until it explodes; -1 if not lit
  uint8_t timer;      // ticks in the current state
  uint8_t bricks;     // bricks laid so far

  bool active() const
  {
    return state != State::kDead && state != State::kSaved;
  }
  bool doomed() const  // dying or leaving; takes no skills
  {
    return !active() || state == State::kSplatting || state == State::kExiting;
  }
};

// One-off events for the renderer (explosions, splats).
struct Effect
{
  enum class Kind : uint8_t
  {
    kExplosion,
    kSplat,
    kSaved,
    kLost,  // fell out of the bottom
  };
  Kind kind;
  int16_t x;
  int16_t y;
};

struct LevelParams
{
  int total = 10;  // walkers that will come out
  int needed = 5;  // walkers that must reach the goal
  int spawn_interval = 3 * kTicksPerSecond;
  int first_spawn = kTicksPerSecond;  // delay before the first walker
  int spawn_x = 0;                    // where walkers appear (feet)
  int spawn_y = 0;
  int8_t spawn_dir = 1;
  gfx::Rect goal;                         // walkers whose feet enter it are saved
  std::array<int, kSkillCount> skills{};  // count per skill, or kUnlimited
};

// The walkers and their rules. Deterministic for a given seed, so a level can
// be played out headless to check it can be won.
class World
{
 public:
  static constexpr int kMaxWalkers = 40;
  static constexpr int kMaxEffects = 16;

  void start(Terrain& terrain, const LevelParams& params, uint32_t seed);
  void tick();

  // Gives a skill to walker i if the rules allow it and one is left.
  bool assign(int i, Skill s);
  bool can_assign(int i, Skill s) const;
  int skills_left(Skill s) const
  {
    return skills_[static_cast<int>(s)];
  }

  int count() const
  {
    return count_;
  }
  const Walker& walker(int i) const
  {
    return walkers_[i];
  }
  const LevelParams& params() const
  {
    return params_;
  }
  uint32_t ticks() const
  {
    return ticks_;
  }

  int spawned() const
  {
    return count_;
  }
  int saved() const
  {
    return saved_;
  }
  int lost() const
  {
    return lost_;
  }
  int active() const
  {
    return count_ - saved_ - lost_;
  }
  bool hatch_open() const
  {
    return ticks_ >= static_cast<uint32_t>(params_.first_spawn);
  }
  // Every walker is out and has been saved or lost.
  bool finished() const
  {
    return count_ == params_.total && active() == 0;
  }
  bool won() const
  {
    return saved_ >= params_.needed;
  }
  // The goal can no longer be reached even if everyone left is saved.
  bool hopeless() const
  {
    return saved_ + (params_.total - lost_ - saved_) < params_.needed;
  }

  // Effects since the last call.
  int take_effects(Effect* out, int max);

  Terrain& terrain()
  {
    return *terrain_;
  }

 private:
  void spawn();
  void update(Walker& w);
  void fall(Walker& w);
  void walk(Walker& w);
  void dig(Walker& w);
  void build(Walker& w);
  void explode(Walker& w);
  bool blocked(const Walker& w, int nx) const;
  void finish(Walker& w, State end);
  void effect(Effect::Kind kind, int x, int y);

  Terrain* terrain_ = nullptr;
  LevelParams params_;
  sim::Rng rng_;
  std::array<Walker, kMaxWalkers> walkers_{};
  std::array<int, kSkillCount> skills_{};
  std::array<Effect, kMaxEffects> effects_{};
  int effect_count_ = 0;
  int count_ = 0;
  int saved_ = 0;
  int lost_ = 0;
  uint32_t ticks_ = 0;
};

}  // namespace walkers
