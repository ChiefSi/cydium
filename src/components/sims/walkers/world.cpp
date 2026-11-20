#include "world.h"

#include <algorithm>
#include <cstdlib>

namespace walkers
{

void World::start(Terrain& terrain, const LevelParams& params, uint32_t seed)
{
  terrain_ = &terrain;
  params_ = params;
  params_.total = std::min(params_.total, kMaxWalkers);
  params_.needed = std::min(params_.needed, params_.total);
  rng_.seed_with(seed);
  skills_ = params_.skills;
  count_ = saved_ = lost_ = 0;
  effect_count_ = 0;
  ticks_ = 0;
}

void World::spawn()
{
  Walker& w = walkers_[count_++];
  w = {};
  w.x = static_cast<int16_t>(params_.spawn_x);
  w.y = static_cast<int16_t>(params_.spawn_y);
  w.dir = params_.spawn_dir;
  w.state = State::kFalling;
  w.hair = static_cast<uint8_t>(rng_.below(kHairColours));
  w.fall_from = w.y;
  w.fuse = -1;
}

void World::tick()
{
  if (count_ < params_.total && ticks_ >= static_cast<uint32_t>(params_.first_spawn) &&
      (ticks_ - params_.first_spawn) % params_.spawn_interval == 0)
    spawn();
  for (int i = 0; i < count_; ++i)
    if (walkers_[i].active())
      update(walkers_[i]);
  ++ticks_;
}

void World::effect(Effect::Kind kind, int x, int y)
{
  if (effect_count_ < kMaxEffects)
    effects_[effect_count_++] = {kind, static_cast<int16_t>(x), static_cast<int16_t>(y)};
}

int World::take_effects(Effect* out, int max)
{
  const int n = std::min(max, effect_count_);
  std::copy(effects_.begin(), effects_.begin() + n, out);
  effect_count_ = 0;
  return n;
}

void World::finish(Walker& w, State end)
{
  w.state = end;
  w.timer = 0;
  if (end == State::kSaved)
    ++saved_;
  else
    ++lost_;
}

void World::update(Walker& w)
{
  if (w.timer < 255)
    ++w.timer;
  if (w.fuse > 0 && !w.doomed() && --w.fuse == 0)
  {
    explode(w);
    return;
  }
  switch (w.state)
  {
    case State::kFalling:
      fall(w);
      break;
    case State::kWalking:
      walk(w);
      break;
    case State::kDigging:
      dig(w);
      break;
    case State::kBuilding:
      build(w);
      break;
    case State::kShrugging:
      if (w.timer >= kShrugTicks)
        w.state = State::kWalking, w.timer = 0;
      break;
    case State::kBlocking:
      if (!terrain_->solid(w.x, w.y + 1))
        w.state = State::kFalling, w.fall_from = w.y, w.timer = 0;
      break;
    case State::kSplatting:
      if (w.timer >= kSplatTicks)
        finish(w, State::kDead);
      break;
    case State::kExiting:
      if (w.timer >= kExitTicks)
      {
        finish(w, State::kSaved);
        effect(Effect::Kind::kSaved, w.x, w.y);
      }
      break;
    default:
      break;
  }
}

void World::fall(Walker& w)
{
  const int speed = w.umbrella && w.y - w.fall_from >= kUmbrellaOpen ? kFloatSpeed : kFallSpeed;
  for (int i = 0; i < speed; ++i)
  {
    if (w.y >= terrain_->height() + kWalkerH)  // out of the bottom
    {
      finish(w, State::kDead);
      effect(Effect::Kind::kLost, w.x, terrain_->height() - 1);
      return;
    }
    if (terrain_->solid(w.x, w.y + 1))
    {
      w.timer = 0;
      if (!w.umbrella && w.y - w.fall_from > kSplatFall)
      {
        w.state = State::kSplatting;
        effect(Effect::Kind::kSplat, w.x, w.y);
      }
      else
      {
        w.state = State::kWalking;
      }
      return;
    }
    ++w.y;
  }
}

bool World::blocked(const Walker& w, int nx) const
{
  for (int i = 0; i < count_; ++i)
  {
    const Walker& b = walkers_[i];
    if (&b == &w || b.state != State::kBlocking)
      continue;
    if (std::abs(b.y - w.y) < kWalkerH && std::abs(b.x - nx) <= kBlockReach &&
        (b.x - w.x) * w.dir > 0)
      return true;
  }
  return false;
}

void World::walk(Walker& w)
{
  if (params_.goal.contains(w.x, w.y))
  {
    w.state = State::kExiting;
    w.timer = 0;
    return;
  }
  const int nx = w.x + w.dir;
  if (blocked(w, nx))
  {
    w.dir = static_cast<int8_t>(-w.dir);
    return;
  }
  if (terrain_->solid(nx, w.y))
  {
    for (int s = 1; s <= kStepUp; ++s)
      if (!terrain_->solid(nx, w.y - s))
      {
        w.x = static_cast<int16_t>(nx);
        w.y = static_cast<int16_t>(w.y - s);
        return;
      }
    w.dir = static_cast<int8_t>(-w.dir);  // a wall
    return;
  }
  w.x = static_cast<int16_t>(nx);
  if (terrain_->solid(w.x, w.y + 1))
    return;
  for (int s = 1; s <= kStepDown; ++s)
    if (terrain_->solid(w.x, w.y + 1 + s))
    {
      w.y = static_cast<int16_t>(w.y + s);
      return;
    }
  w.state = State::kFalling;
  w.fall_from = w.y;
  w.timer = 0;
}

void World::dig(Walker& w)
{
  if (w.timer % kDigTicks != 0)
    return;
  const int row = w.y + 1;
  bool any = false;
  for (int x = w.x - kDigHalfWidth; x <= w.x + kDigHalfWidth; ++x)
  {
    const Material m = terrain_->at(x, row);
    if (m == Material::kMetal && x >= 0 && x < terrain_->width())
    {
      w.state = State::kWalking;  // can't dig metal
      w.timer = 0;
      return;
    }
    any = any || m == Material::kDirt;
  }
  if (!any)  // broke through
  {
    w.state = State::kFalling;
    w.fall_from = w.y;
    w.timer = 0;
    return;
  }
  terrain_->dig_row(w.x - kDigHalfWidth, w.x + kDigHalfWidth, row);
  ++w.y;
}

void World::build(Walker& w)
{
  if (w.timer % kBuildTicks != 0)
    return;
  if (w.bricks >= kBuildBricks)
  {
    w.state = State::kShrugging;
    w.timer = 0;
    return;
  }
  terrain_->brick(w.x + w.dir, w.x + w.dir * kBrickWidth, w.y);
  ++w.bricks;
  const int nx = w.x + 2 * w.dir;
  const int ny = w.y - 1;
  if (terrain_->solid(nx, ny) || terrain_->solid(w.x + w.dir, ny) ||
      terrain_->solid(nx, ny - kWalkerH + 1))
  {
    w.state = State::kWalking;  // up against something: stop and walk on
    w.timer = 0;
    return;
  }
  w.x = static_cast<int16_t>(nx);
  w.y = static_cast<int16_t>(ny);
}

void World::explode(Walker& w)
{
  terrain_->blast(w.x, w.y - kWalkerH / 2, kBlastRadius);
  effect(Effect::Kind::kExplosion, w.x, w.y - kWalkerH / 2);
  w.fuse = -1;
  finish(w, State::kDead);
}

bool World::can_assign(int i, Skill s) const
{
  if (i < 0 || i >= count_)
    return false;
  const Walker& w = walkers_[i];
  if (w.doomed() || skills_[static_cast<int>(s)] == 0)
    return false;
  switch (s)
  {
    case Skill::kUmbrella:
      return !w.umbrella;
    case Skill::kExploder:
      return w.fuse < 0;
    default:  // one skill at a time, given while walking
      return w.state == State::kWalking && w.fuse < 0;
  }
}

bool World::assign(int i, Skill s)
{
  if (!can_assign(i, s))
    return false;
  int& left = skills_[static_cast<int>(s)];
  if (left != kUnlimited)
    --left;
  Walker& w = walkers_[i];
  switch (s)
  {
    case Skill::kDigger:
      w.state = State::kDigging;
      break;
    case Skill::kBuilder:
      w.state = State::kBuilding;
      w.bricks = 0;
      break;
    case Skill::kBlocker:
      w.state = State::kBlocking;
      break;
    case Skill::kExploder:
      w.fuse = kFuseTicks;
      return true;
    case Skill::kUmbrella:
      w.umbrella = true;
      return true;
    default:
      return false;
  }
  w.timer = 0;
  return true;
}

}  // namespace walkers
