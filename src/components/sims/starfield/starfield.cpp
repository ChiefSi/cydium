#include "starfield.h"

#include "gfx/canvas.h"

namespace starfield
{
namespace
{

// Palette layout.
constexpr uint8_t kSpaceFirst = 1;  // 1..16: background gradient
constexpr int kSpaceCount = 16;
constexpr uint8_t kStarFirst = 32;  // 32..34: star colour per depth

constexpr int32_t kOne = 256;               // fixed-point 1.0
constexpr int32_t kSpeed[3] = {8, 20, 48};  // pixels per second, per depth
constexpr int kSize[3] = {1, 1, 2};         // star size, per depth
constexpr int kWarp = 8;                    // speed multiplier while warping

gfx::Rect star_rect(int32_t x, int y, int depth)
{
  return {x / kOne, y, kSize[depth], kSize[depth]};
}

}  // namespace

bool Starfield::begin(sim::Context& ctx)
{
  sim::Surface& s = ctx.surface;
  gfx::Palette& p = s.palette();
  for (int i = 0; i < kSpaceCount; ++i)
    p.set(kSpaceFirst + i, 2 + i / 2, 2 + i / 3, 10 + i * 2);
  p.set(kStarFirst + 0, 110, 110, 140);
  p.set(kStarFirst + 1, 180, 180, 210);
  p.set(kStarFirst + 2, 255, 255, 240);

  gfx::Canvas bg(s.bg());
  const int h = s.height();
  for (int y = 0; y < h; ++y)
    bg.hline(0, y, s.width(), static_cast<uint8_t>(kSpaceFirst + y * kSpaceCount / h));

  warp_ = false;
  for (auto& star : stars_)
  {
    place(star, ctx, true);
    draw(star, ctx, false);
  }
  return true;
}

void Starfield::place(Star& star, sim::Context& ctx, bool anywhere)
{
  const int w = ctx.surface.width();
  star.depth = static_cast<uint8_t>(ctx.rng.below(10) < 6 ? 0 : ctx.rng.below(10) < 7 ? 1 : 2);
  star.x = (anywhere ? ctx.rng.below(w) : w) * kOne;
  star.y = static_cast<int16_t>(ctx.rng.below(ctx.surface.height() - kSize[star.depth] + 1));
}

void Starfield::draw(const Star& star, sim::Context& ctx, bool erase)
{
  const gfx::Rect r = star_rect(star.x, star.y, star.depth);
  gfx::Canvas fg(ctx.surface.fg());
  fg.fill_rect(r, erase ? 0 : static_cast<uint8_t>(kStarFirst + star.depth));
  ctx.surface.invalidate(r);
}

void Starfield::step(sim::Context& ctx, uint32_t dt_ms)
{
  const int32_t dt = static_cast<int32_t>(dt_ms > 100 ? 100 : dt_ms);
  const int32_t mult = warp_ ? kWarp : 1;
  for (auto& star : stars_)
  {
    const int32_t nx = star.x - kSpeed[star.depth] * mult * kOne * dt / 1000;
    if (nx / kOne == star.x / kOne)
    {
      star.x = nx;  // still on the same pixel: nothing to redraw
      continue;
    }
    draw(star, ctx, true);
    star.x = nx;
    if (star.x < 0)
      place(star, ctx, false);
    draw(star, ctx, false);
  }
}

void Starfield::tap(sim::Context& ctx, int x, int y)
{
  (void)ctx;
  (void)x;
  (void)y;
  warp_ = !warp_;
}

}  // namespace starfield
