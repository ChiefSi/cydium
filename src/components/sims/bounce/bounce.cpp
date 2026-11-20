#include "bounce.h"

#include "gfx/canvas.h"

namespace bounce
{
namespace
{

// Palette layout.
constexpr uint8_t kGradientFirst = 1;  // 1..32: background gradient
constexpr int kGradientCount = 32;
constexpr uint8_t kGrid = 40;
constexpr uint8_t kBallFirst = 48;  // 48..: ball colours
constexpr int kRadius = 12;
constexpr int32_t kOne = 256;  // fixed-point 1.0

void setup_palette(gfx::Palette& p, int balls)
{
  for (int i = 0; i < kGradientCount; ++i)
    p.set(kGradientFirst + i, 8, 16 + i * 3, 40 + i * 4);
  p.set(kGrid, 40, 70, 110);
  constexpr uint8_t kRgb[][3] = {
      {255, 80, 80}, {80, 220, 120}, {90, 150, 255}, {255, 210, 70}, {230, 110, 255}};
  for (int i = 0; i < balls; ++i)
    p.set(kBallFirst + i, kRgb[i % 5][0], kRgb[i % 5][1], kRgb[i % 5][2]);
}

}  // namespace

gfx::Rect Bounce::bounds(const Ball& b)
{
  return {b.x / kOne - kRadius, b.y / kOne - kRadius, 2 * kRadius + 1, 2 * kRadius + 1};
}

bool Bounce::begin(sim::Context& ctx)
{
  sim::Surface& s = ctx.surface;
  const int w = s.width();
  const int h = s.height();
  setup_palette(s.palette(), kNumBalls);

  gfx::Canvas bg(s.bg());
  for (int y = 0; y < h; ++y)
    bg.hline(0, y, w, static_cast<uint8_t>(kGradientFirst + y * kGradientCount / h));
  for (int x = 0; x < w; x += 40)
    bg.vline(x, 0, h, kGrid);
  for (int y = 0; y < h; y += 40)
    bg.hline(0, y, w, kGrid);

  for (int i = 0; i < kNumBalls; ++i)
  {
    Ball& b = balls_[i];
    b.x = ctx.rng.range(kRadius, w - kRadius - 1) * kOne;
    b.y = ctx.rng.range(kRadius, h - kRadius - 1) * kOne;
    b.vx = (60 + 30 * (i % 3)) * (ctx.rng.chance(50) ? 1 : -1);
    b.vy = (30 + 30 * (i % 4)) * (ctx.rng.chance(50) ? 1 : -1);
    b.colour = static_cast<uint8_t>(kBallFirst + i);
  }
  draw(ctx);
  return true;
}

void Bounce::step(sim::Context& ctx, uint32_t dt_ms)
{
  sim::Surface& s = ctx.surface;
  gfx::Canvas fg(s.fg());
  const int32_t lo = kRadius * kOne;
  const int32_t hi_x = (s.width() - kRadius - 1) * kOne;
  const int32_t hi_y = (s.height() - kRadius - 1) * kOne;
  const int32_t dt = static_cast<int32_t>(dt_ms > 100 ? 100 : dt_ms);  // no jumps after stalls
  for (auto& b : balls_)
  {
    fg.fill_rect(bounds(b), 0);
    s.invalidate(bounds(b));
    b.x += b.vx * kOne * dt / 1000;
    b.y += b.vy * kOne * dt / 1000;
    if (b.x < lo || b.x > hi_x)
    {
      b.vx = -b.vx;
      b.x = b.x < lo ? lo : hi_x;
    }
    if (b.y < lo || b.y > hi_y)
    {
      b.vy = -b.vy;
      b.y = b.y < lo ? lo : hi_y;
    }
  }
  draw(ctx);
}

void Bounce::tap(sim::Context& ctx, int x, int y)
{
  gfx::Canvas fg(ctx.surface.fg());
  Ball& b = balls_[0];
  fg.fill_rect(bounds(b), 0);
  ctx.surface.invalidate(bounds(b));
  b.x = x * kOne;
  b.y = y * kOne;
  draw(ctx);  // step() pulls it back inside the walls
}

void Bounce::draw(sim::Context& ctx)
{
  gfx::Canvas fg(ctx.surface.fg());
  for (const auto& b : balls_)
  {
    fg.fill_circle(b.x / kOne, b.y / kOne, kRadius, b.colour);
    ctx.surface.invalidate(bounds(b));
  }
}

}  // namespace bounce
