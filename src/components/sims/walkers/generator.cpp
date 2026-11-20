// Level generator. A level is a stack of floors from the hatch at the top to
// the goal on the last floor. Each floor's way down (its "link") and any wall
// or gap on it is a challenge with a known solution, recorded as plan steps,
// so every level is built around a way to win it. The game checks each new
// level by playing it out with the autopilot before showing it.

#include <algorithm>

#include "level.h"
#include "sim/rng.h"

namespace walkers
{
namespace
{

constexpr int kFirstFloor = 40;   // surface row of the top floor
constexpr int kHatchBottom = 22;  // walkers appear here and drop to the floor
constexpr int kSafeMin = 42;      // floor-to-floor spacing walkers survive
constexpr int kSafeMax = 50;
constexpr int kDeepMin = 64;  // spacing that needs umbrellas
constexpr int kDeepMax = 76;
constexpr int kMaxFloors = 5;
constexpr int kLipHeight = 8;  // wall at the edge of an abyss; turns walkers
constexpr int kLipWidth = 4;
constexpr int kBottomMargin = 16;  // last floor surface at least this far up

enum class Link : uint8_t
{
  kOpen,      // an open end; a safe drop to the next floor
  kDig,       // closed floor; dig through it
  kUmbrella,  // an open end; a drop that kills without an umbrella
  kBlockDig,  // an open end over the abyss; block it, then dig
  kLast,      // the goal floor
};

struct Floor
{
  int y;       // surface row; walkers' feet are on y - 1
  int t;       // thickness
  int lo, hi;  // solid columns [lo, hi)
  bool lip_lo, lip_hi;
  Link link;
  int entry;  // where walkers land
};

uint32_t hash(int x, int y)
{
  uint32_t h = static_cast<uint32_t>(x) * 0x8DA6B343u ^ static_cast<uint32_t>(y) * 0xD8163841u;
  h ^= h >> 13;
  h *= 0x85EBCA6Bu;
  return h ^ (h >> 16);
}

class Painter
{
 public:
  explicit Painter(Terrain& t) : t_(t) {}

  // Fills [x0, x1) x [y0, y1); `top` is the surface row the dirt shading
  // counts from.
  void dirt(int x0, int x1, int y0, int y1, int top)
  {
    for (int y = std::max(y0, 0); y < std::min(y1, t_.height()); ++y)
      for (int x = std::max(x0, 0); x < std::min(x1, t_.width()); ++x)
      {
        const int shade = std::min(4, (y - top) / 3);
        t_.pixel(x, y) = static_cast<uint8_t>(pal::kDirtFirst + shade * 8 + (hash(x, y) & 7));
      }
  }

  void metal(int x0, int x1, int y0, int y1)
  {
    for (int y = std::max(y0, 0); y < std::min(y1, t_.height()); ++y)
      for (int x = std::max(x0, 0); x < std::min(x1, t_.width()); ++x)
      {
        const int px = (x - x0) % 8;
        const int py = (y - y0) % 8;
        int v = 4 + (hash(x, y) & 1);  // plate face
        if (px == 7 || py == 7 || x == x1 - 1 || y == y1 - 1)
          v = 0;  // shadow edge
        else if (px == 0 || py == 0)
          v = 1;  // lit edge
        else if ((px == 2 || px == 5) && (py == 2 || py == 5))
          v = 2;  // rivet
        t_.pixel(x, y) = static_cast<uint8_t>(pal::kMetalFirst + v);
      }
  }

  // A floor slab with a slightly ragged underside.
  void slab(int x0, int x1, int y, int t, sim::Rng& rng)
  {
    int extra = 0;
    for (int x = x0; x < x1; ++x)
    {
      extra = std::clamp(extra + rng.range(-1, 1), 0, 3);
      dirt(x, x + 1, y, y + t + extra, y);
    }
  }

 private:
  Terrain& t_;
};

struct Builder
{
  Terrain& terrain;
  Painter paint;
  sim::Rng rng;
  Level& level;
  int w;
  int h;

  bool add(const PlanStep& s)
  {
    if (level.step_count >= Level::kMaxSteps)
      return false;
    level.steps[level.step_count++] = s;
    return true;
  }

  // A wall on floor f between x = entry and x = target, with its builder
  // step. Returns the step index, or -1 if there is no room.
  int wall(const Floor& f, int target, int after)
  {
    const int dist = std::abs(target - f.entry);
    if (dist < 76)
      return -1;
    const int dir = target > f.entry ? 1 : -1;
    const int height = rng.range(7, 11);
    const int width = rng.range(6, 10);
    const int face = f.entry + dir * rng.range(44, dist - 28);  // first wall column
    const int far = face + dir * (width - 1);
    const int x0 = std::min(face, far);
    if (rng.chance(50))
      paint.metal(x0, x0 + width, f.y - height, f.y);
    else
      paint.dirt(x0, x0 + width, f.y - height, f.y, f.y - height);

    const int dmin = 2 * height - 5;
    const int dmax = 22;
    PlanStep s{};
    s.skill = Skill::kBuilder;
    s.dir = static_cast<int8_t>(dir);
    s.after = static_cast<int8_t>(after);
    s.y = static_cast<int16_t>(f.y - 1);
    s.x0 = static_cast<int16_t>(std::min(face - dir * dmin, face - dir * dmax));
    s.x1 = static_cast<int16_t>(std::max(face - dir * dmin, face - dir * dmax));
    const int sx = dir > 0 ? far + 3 : far - 3 - 30;
    s.success = {sx, f.y - height - 3, 31, height + 3};
    ++level.challenges;
    return add(s) ? level.step_count - 1 : -2;
  }

  int dig(const Floor& f, int c, int next_y, int after)
  {
    PlanStep s{};
    s.skill = Skill::kDigger;
    s.after = static_cast<int8_t>(after);
    s.y = static_cast<int16_t>(f.y - 1);
    s.x0 = static_cast<int16_t>(c - 6);
    s.x1 = static_cast<int16_t>(c + 6);
    s.success = {c - 8, f.y + f.t + 5, 17, std::max(1, next_y - (f.y + f.t + 5))};
    ++level.challenges;
    return add(s) ? level.step_count - 1 : -2;
  }
};

int pick(sim::Rng& rng, const int* weights, int n)
{
  int total = 0;
  for (int i = 0; i < n; ++i)
    total += weights[i];
  if (total == 0)
    return -1;
  int r = rng.below(total);
  for (int i = 0; i < n; ++i)
    if ((r -= weights[i]) < 0)
      return i;
  return n - 1;
}

}  // namespace

bool generate(uint32_t seed, Terrain& terrain, const gfx::Rect& avoid, Level* out)
{
  Level& level = *out;
  level = Level{};
  level.seed = seed;
  terrain.clear();
  Builder b{terrain, Painter(terrain), sim::Rng(seed * 2654435761u + 1),
            level,   terrain.width(),  terrain.height()};
  sim::Rng& rng = b.rng;
  const int w = b.w;
  const int h = b.h;

  // The hatch, near one side, facing the middle.
  const int side = rng.chance(50) ? -1 : 1;
  level.hatch_x = side < 0 ? rng.range(24, w / 3) : rng.range(w - w / 3, w - 24);
  level.hatch_y = kHatchBottom - kWalkerH;
  if (avoid.intersects({level.hatch_x - 14, 0, 28, kHatchBottom + 2}))
    return false;

  int abyss_lo = 0;  // floors below a block-and-dig stop short of the abyss
  int abyss_hi = w;
  int entry = level.hatch_x;
  int y = kFirstFloor;
  int blockers = 0;
  bool umbrellas = false;

  for (int n = 0;; ++n)
  {
    Floor f{};
    f.y = y;
    f.t = rng.range(10, 14);
    f.lo = abyss_lo;
    f.hi = abyss_hi;
    f.lip_lo = abyss_lo > 0;
    f.lip_hi = abyss_hi < w;
    f.entry = entry;
    if (entry < f.lo + 12 || entry > f.hi - 12)
      return false;

    // How this floor leads down.
    const int room = h - kBottomMargin - y;
    f.link = Link::kLast;
    if (n + 1 < kMaxFloors && room >= kSafeMin)
    {
      const int weights[] = {1, 3, room >= kDeepMin ? 2 : 0, 2};
      f.link = static_cast<Link>(pick(rng, weights, 4));
    }

    // Lips at the edges of an abyss.
    if (f.lip_lo)
      b.paint.metal(f.lo, f.lo + kLipWidth, f.y - kLipHeight, f.y);
    if (f.lip_hi)
      b.paint.metal(f.hi - kLipWidth, f.hi, f.y - kLipHeight, f.y);

    if (f.link == Link::kLast)
    {
      // Goal floor: solid to the bottom, maybe with a wall or a gap first.
      const int dir = f.hi - entry >= entry - f.lo ? 1 : -1;
      const int r = rng.below(3);
      int beyond = entry;  // the goal goes past this
      if (r == 0)          // a gap over the abyss: build across it
      {
        const int edge = entry + dir * rng.range(36, 60);  // last solid column
        const int gap = rng.range(8, 12);
        const int g0 = dir > 0 ? edge + 1 : edge - gap;
        if (g0 < f.lo + 20 || g0 + gap > f.hi - 50)
          return false;
        b.paint.slab(f.lo, g0, f.y, h - f.y, rng);
        b.paint.slab(g0 + gap, f.hi, f.y, h - f.y, rng);
        PlanStep s{};
        s.skill = Skill::kBuilder;
        s.dir = static_cast<int8_t>(dir);
        s.after = -1;
        s.critical = true;
        s.y = static_cast<int16_t>(f.y - 1);
        s.x0 = static_cast<int16_t>(std::min(edge - dir * 2, edge - dir * 9));
        s.x1 = static_cast<int16_t>(std::max(edge - dir * 2, edge - dir * 9));
        const int far = dir > 0 ? g0 + gap : g0 - 1;
        s.success = {dir > 0 ? far + 2 : far - 32, f.y - 16, 31, 16};
        if (!b.add(s))
          return false;
        ++level.challenges;
        beyond = far + dir * 24;
      }
      else
      {
        b.paint.slab(f.lo, f.hi, f.y, h - f.y, rng);
        if (r == 1)
        {
          const int goal_room = dir > 0 ? f.hi - 40 : f.lo + 40;
          if (b.wall(f, goal_room, -1) < -1)
            return false;
          if (level.step_count > 0 && level.steps[level.step_count - 1].y == f.y - 1)
            beyond = level.steps[level.step_count - 1].success.x + (dir > 0 ? 6 : 24);
          else
            beyond = entry + dir * 50;
        }
        else
        {
          beyond = entry + dir * 50;
        }
      }
      const int lo = dir > 0 ? beyond : f.lo + 14;
      const int hi = dir > 0 ? f.hi - 14 : beyond;
      if (hi - lo < 4)
        return false;
      level.goal_x = rng.range(lo, hi);
      level.goal_y = f.y - 1;
      if (avoid.intersects({level.goal_x - 10, f.y - 22, 20, 22}))
        return false;
      break;
    }

    const int spacing =
        f.link == Link::kUmbrella ? rng.range(kDeepMin, kDeepMax) : rng.range(kSafeMin, kSafeMax);
    const int next_y = y + spacing;

    if (f.link == Link::kDig)
    {
      // Closed floor; a dig zone away from where walkers land, maybe behind
      // a wall, maybe the only dirt in a metal floor.
      const int lo = f.lo + 18;
      const int hi = f.hi - 18;
      int c = rng.range(lo, hi);
      for (int tries = 0; std::abs(c - entry) < 30 && tries < 8; ++tries)
        c = rng.range(lo, hi);
      if (std::abs(c - entry) < 30)
        return false;
      if (rng.chance(30))
      {
        b.paint.metal(f.lo, f.hi, f.y, f.y + f.t);
        b.paint.dirt(c - 12, c + 13, f.y, f.y + f.t, f.y);
      }
      else
      {
        b.paint.slab(f.lo, f.hi, f.y, f.t, rng);
      }
      int after = -1;
      if (rng.chance(40))
      {
        after = b.wall(f, c, -1);
        if (after < -1)
          return false;
      }
      if (b.dig(f, c, next_y, after) < 0)
        return false;
      entry = c;
    }
    else
    {
      // An open end on the side away from where walkers land, not over an
      // abyss already used.
      int dir = f.hi - entry >= entry - f.lo ? 1 : -1;
      if ((dir > 0 && f.lip_hi) || (dir < 0 && f.lip_lo))
        dir = -dir;
      if ((dir > 0 && f.lip_hi) || (dir < 0 && f.lip_lo))
        return false;
      const int margin = f.link == Link::kBlockDig ? 70 : 50;
      const int lo = dir > 0 ? std::max(entry + margin, f.lo + 60) : f.lo + 24;
      const int hi = dir > 0 ? f.hi - 24 : std::min(entry - margin, f.hi - 60);
      if (hi < lo)
        return false;
      const int end = rng.range(lo, hi);  // first open column
      if (dir > 0)
        b.paint.slab(f.lo, end, f.y, f.t, rng);
      else
        b.paint.slab(end + 1, f.hi, f.y, f.t, rng);

      if (f.link == Link::kBlockDig)
      {
        // Blocker short of the edge, then dig on the safe side of it.
        const int bx = end - dir * rng.range(10, 16);
        PlanStep s{};
        s.skill = Skill::kBlocker;
        s.dir = static_cast<int8_t>(dir);
        s.after = -1;
        s.critical = true;
        s.y = static_cast<int16_t>(f.y - 1);
        s.x0 = static_cast<int16_t>(bx - 3);
        s.x1 = static_cast<int16_t>(bx + 3);
        if (!b.add(s))
          return false;
        const int block_step = level.step_count - 1;
        ++level.challenges;
        ++blockers;
        const int dlo = dir > 0 ? f.lo + 18 : bx + 18;
        const int dhi = dir > 0 ? bx - 18 : f.hi - 18;
        if (dhi < dlo)
          return false;
        const int c = rng.range(dlo, dhi);
        if (b.dig(f, c, next_y, block_step) < 0)
          return false;
        level.challenges--;  // block-and-dig counts once
        if (dir > 0)
          abyss_hi = end;
        else
          abyss_lo = end + 1;
        entry = c;
      }
      else
      {
        if (rng.chance(40) && b.wall(f, end, -1) < -1)
          return false;
        if (f.link == Link::kUmbrella)
        {
          PlanStep s{};
          s.skill = Skill::kUmbrella;
          s.each = true;
          s.critical = true;
          s.after = -1;
          s.y = static_cast<int16_t>(f.y - 1);
          s.x0 = 0;
          s.x1 = static_cast<int16_t>(w - 1);
          if (!b.add(s))
            return false;
          if (!umbrellas)
            ++level.challenges;
          umbrellas = true;
        }
        entry = end;
      }
    }
    y = next_y;
  }

  // Walker numbers: the plan loses one walker per blocker, plus some slack.
  LevelParams& p = level.params;
  p.total = rng.range(10, 20);
  p.needed = std::max(1, p.total - blockers - rng.range(1, 3));
  p.spawn_interval = rng.range(40, 60);
  p.first_spawn = 30;
  p.spawn_x = level.hatch_x;
  p.spawn_y = level.hatch_y + kWalkerH;
  p.spawn_dir = static_cast<int8_t>(-side);
  p.goal = {level.goal_x - 2, level.goal_y - 2, 5, 3};
  p.skills.fill(0);
  for (int i = 0; i < level.step_count; ++i)
  {
    const PlanStep& s = level.steps[i];
    if (!s.each)
      ++p.skills[static_cast<int>(s.skill)];
  }
  p.skills[static_cast<int>(Skill::kExploder)] += blockers;  // to free them at the end
  if (umbrellas)
    p.skills[static_cast<int>(Skill::kUmbrella)] = p.total;
  return level.challenges >= (h > 240 ? 3 : 2);
}

}  // namespace walkers
