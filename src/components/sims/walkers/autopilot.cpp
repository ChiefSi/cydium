#include "autopilot.h"

#include <cstdlib>

namespace walkers
{
namespace
{

constexpr int kApproach = 48;  // start aiming at walkers this close to a zone
constexpr int kMaxHover = 6 * kTicksPerSecond;
constexpr int kMaxTicks = 8 * 60 * kTicksPerSecond;  // verify(): give up after

bool on_row(const Walker& w, int y)
{
  return std::abs(w.y - y) <= 1;
}

}  // namespace

void Autopilot::start(const Level& level, uint32_t seed, int attempt, bool checking)
{
  level_ = &level;
  rng_.seed_with(seed * 7919u + attempt + 1);
  attempt_ = attempt;
  checking_ = checking;
  steps_ = {};
  target_ = -1;
  step_ = -1;
}

bool Autopilot::in_zone(const PlanStep& s, const Walker& w) const
{
  if (s.each)
    return w.y <= s.y + 1;
  return on_row(w, s.y) && w.x >= s.x0 && w.x <= s.x1 && (s.dir == 0 || w.dir == s.dir);
}

int Autopilot::approach(const PlanStep& s, const Walker& w) const
{
  if (s.each)
    return w.y <= s.y + 1 ? 0 : -1;
  if (!on_row(w, s.y) || (w.state != State::kWalking && w.state != State::kFalling))
    return -1;
  if (in_zone(s, w))
    return 0;
  if (s.dir != 0 && w.dir != s.dir)
    return -1;
  const int d = w.x < s.x0 ? s.x0 - w.x : w.x - s.x1;
  const bool towards = w.x < s.x0 ? w.dir > 0 : w.dir < 0;
  return towards && d <= kApproach ? d : -1;
}

void Autopilot::update_steps(const World& world)
{
  for (int i = 0; i < level_->step_count; ++i)
  {
    const PlanStep& s = level_->steps[i];
    StepState& st = steps_[i];
    if (st.cooldown > 0)
      --st.cooldown;
    if (s.each)
      continue;
    // Done while its effect holds: a blocker in place, or a walker through.
    bool ok = false;
    for (int j = 0; j < world.count() && !ok; ++j)
    {
      const Walker& w = world.walker(j);
      if (s.skill == Skill::kBlocker)
        ok = w.state == State::kBlocking && on_row(w, s.y) && w.x >= s.x0 - 4 && w.x <= s.x1 + 4;
      else
        ok = w.state != State::kDead && s.success.contains(w.x, w.y);
    }
    st.done = st.done || ok;
    if (s.skill == Skill::kBlocker && !ok)
      st.done = false;
    if (st.done || st.assigned < 0)
      continue;
    const Walker& w = world.walker(st.assigned);
    const bool busy = w.state == State::kDigging || w.state == State::kBuilding ||
                      w.state == State::kShrugging || w.state == State::kBlocking;
    if (!w.active() || !busy)
    {
      st.assigned = -1;  // didn't work; try again with someone else
      st.cooldown = kTicksPerSecond;
    }
  }
}

bool Autopilot::step_ready(int i) const
{
  const PlanStep& s = level_->steps[i];
  const StepState& st = steps_[i];
  return !st.done && st.assigned < 0 && st.cooldown == 0 && (s.after < 0 || steps_[s.after].done);
}

void Autopilot::aim(int walker, Skill skill, int step, bool critical)
{
  target_ = walker;
  skill_ = skill;
  step_ = step;
  wait_ = 0;
  if (checking_)
  {
    think_ = critical ? 5 : 12;  // the slowest it would normally be
    fly_total_ = critical ? 6 : 8;
  }
  else
  {
    think_ = critical ? rng_.range(2, 5) : rng_.range(4, 12);
    fly_total_ = critical ? 6 : 8;
    if (step >= 0 && !steps_[step].seen && attempt_ == 0 && !critical)
      think_ += rng_.range(2, 5) * kTicksPerSecond;  // working out what to do
    // The odd fumble, rarer on each retry: hesitating too long.
    const int odds = 20 / (attempt_ + 1);
    if (!critical && rng_.below(100) < odds)
      think_ += rng_.range(2, 4) * kTicksPerSecond;
  }
  if (step >= 0)
    steps_[step].seen = true;
  fly_ = fly_total_;
}

bool Autopilot::choose(const World& world)
{
  // Plan steps in route order; the nearest walker coming up to each zone.
  for (int i = 0; i < level_->step_count; ++i)
  {
    if (!step_ready(i))
      continue;
    const PlanStep& s = level_->steps[i];
    int best = -1;
    int best_d = 1 << 30;
    for (int j = 0; j < world.count(); ++j)
    {
      const Walker& w = world.walker(j);
      if (!world.can_assign(j, s.skill))
        continue;
      bool taken = false;
      for (int k = 0; k < level_->step_count; ++k)
        taken = taken || steps_[k].assigned == j;
      const int d = taken ? -1 : approach(s, w);
      if (d >= 0 && d < best_d)
        best = j, best_d = d;
    }
    if (best >= 0)
    {
      aim(best, s.skill, i, s.critical);
      return true;
    }
  }

  // At the end: free the blockers once nobody else is left to save.
  if (world.spawned() == world.params().total)
  {
    int blocker = -1;
    bool others = false;
    for (int j = 0; j < world.count(); ++j)
    {
      const Walker& w = world.walker(j);
      if (!w.active())
        continue;
      if (w.state == State::kBlocking && w.fuse < 0)
        blocker = j;
      else if (w.state != State::kBlocking)
        others = true;
    }
    if (blocker >= 0 && !others && world.can_assign(blocker, Skill::kExploder))
    {
      aim(blocker, Skill::kExploder, -1, false);
      return true;
    }
  }
  return false;
}

bool Autopilot::tick(const World& world, int* walker, Skill* skill)
{
  update_steps(world);

  if (target_ < 0)
  {
    choose(world);
    return false;
  }
  if (think_ > 0)
  {
    --think_;
    return false;
  }
  if (fly_ > 0)
  {
    --fly_;
    return false;
  }
  // Over the walker: click once it is in place.
  const Walker& w = world.walker(target_);
  const bool can = world.can_assign(target_, skill_);
  const bool ready = step_ < 0 || in_zone(level_->steps[step_], w);
  if (can && ready)
  {
    *walker = target_;
    *skill = skill_;
    if (step_ >= 0 && !level_->steps[step_].each)
      steps_[step_].assigned = target_;
    target_ = -1;
    return true;
  }
  const bool coming = step_ >= 0 && approach(level_->steps[step_], w) >= 0;
  if (!can || !coming || ++wait_ > kMaxHover)
    target_ = -1;  // missed it; look again
  return false;
}

bool verify(const Level& level, Terrain& terrain)
{
  World world;
  world.start(terrain, level.params, level.seed);
  Autopilot pilot;
  pilot.start(level, level.seed, 0, true);
  for (int t = 0; t < kMaxTicks && !world.finished(); ++t)
  {
    int walker;
    Skill skill;
    if (pilot.tick(world, &walker, &skill))
      world.assign(walker, skill);
    world.tick();
  }
  return world.finished() && world.won();
}

}  // namespace walkers
