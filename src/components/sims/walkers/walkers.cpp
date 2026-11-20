#include "walkers.h"

#include <algorithm>
#include <cstdio>

#include "art.h"
#include "gfx/canvas.h"

namespace walkers
{
namespace
{

using gfx::Align;
using gfx::Rect;
namespace ix = art::ix;

constexpr int kBarH = 46;
constexpr int kButtonW = 36;
constexpr int kButtonH = 32;
constexpr int kButtonGap = 2;
constexpr int kMaxTicksPerStep = 4;
constexpr uint32_t kIntroMs = 2500;
constexpr uint32_t kWonMs = 3500;
constexpr uint32_t kAutoChoiceMs = 4000;
constexpr uint32_t kTakeoverMs = 20000;  // the user has control until this long idle
constexpr uint32_t kStuckMs = 60000;     // all out and nothing saved or lost
constexpr uint32_t kMaxLevelTicks = 8 * 60 * kTicksPerSecond;
constexpr int kPickRadius = 12;
constexpr int kGravity = 2;  // particles, 1/16 px per tick per tick

Rect panel_rect(int w, int play_h, int h)
{
  const int pw = std::min(w - 16, 208);
  return {(w - pw) / 2, (play_h - h) / 2, pw, h};
}

}  // namespace

bool Walkers::begin(sim::Context& ctx)
{
  art::setup_palette(ctx.surface.palette());
  reserved_ = ctx.surface.reserved();
  layout(ctx);
  new_level(ctx, false);  // also on rotation: a new level for the new shape
  return true;
}

void Walkers::layout(sim::Context& ctx)
{
  const int w = ctx.surface.width();
  const int h = ctx.surface.height();
  play_h_ = h - kBarH;
  bar_ = {0, play_h_, w, kBarH};

  // Keep the bar's contents clear of the menu icon if it sits in the bar.
  int lo = 4;
  int hi = w - 4;
  const Rect r = reserved_.intersect(bar_);
  if (!r.empty())
  {
    if (r.x + r.w / 2 < w / 2)
      lo = r.right() + 2;
    else
      hi = r.x - 2;
  }
  int bw = kButtonW;
  if (kSkillCount * bw + (kSkillCount - 1) * kButtonGap > hi - lo)
    bw = (hi - lo - (kSkillCount - 1) * kButtonGap) / kSkillCount;
  const int total = kSkillCount * bw + (kSkillCount - 1) * kButtonGap;
  int x = lo + (hi - lo - total) / 2;
  for (int i = 0; i < kSkillCount; ++i, x += bw + kButtonGap)
    buttons_[i] = {x, play_h_ + 12, bw, kButtonH};
  status_key_ = ~0u;
}

void Walkers::new_level(sim::Context& ctx, bool retry)
{
  sim::Surface& s = ctx.surface;
  terrain_.attach(s.bg(), play_h_);
  const Rect avoid = reserved_.intersect({0, 0, s.width(), play_h_});
  if (retry)
  {
    ++attempt_;
    generate(seed_, terrain_, avoid, &level_);
  }
  else
  {
    attempt_ = 0;
    ++level_number_;
    // Only levels the autopilot has shown it can win (checking changes the
    // terrain, so the chosen level is drawn again afterwards).
    const uint32_t base = ctx.rng.next();
    bool found = false;
    for (uint32_t k = 0; k < 64 && !found; ++k)
    {
      seed_ = base + k * 7919u;
      found = generate(seed_, terrain_, avoid, &level_) && verify(level_, terrain_);
    }
    for (uint32_t k = 0; !found && k < 1000; ++k)
    {
      seed_ = base + 0x10000u + k;
      found = generate(seed_, terrain_, avoid, &level_);
    }
    generate(seed_, terrain_, avoid, &level_);
  }

  gfx::Canvas bg(s.bg());
  art::draw_hatch(bg, level_.hatch_x, level_.hatch_y + art::kSpriteH, false);
  art::draw_door(bg, level_.goal_x, level_.goal_y + 1);
  hatch_open_ = false;

  // Skill counts from the plan and the difficulty.
  LevelParams p = level_.params;
  sim::Rng extra(seed_ ^ 0xA5A5A5A5u);
  for (int i = 0; i < kSkillCount; ++i)
  {
    if (difficulty_ == Difficulty::kEasy)
      p.skills[i] = kUnlimited;
    else
      p.skills[i] += difficulty_ == Difficulty::kHard ? 1 : extra.range(1, 3);
  }
  world_.start(terrain_, p, seed_ + attempt_);
  pilot_.start(level_, seed_, attempt_, false);

  s.fg().clear(0);
  particles_ = {};
  drawn_count_ = 0;
  selected_ = -1;
  acc_ms_ = 0;
  phase_ = Phase::kIntro;
  phase_ms_ = 0;
  since_progress_ms_ = 0;
  last_saved_ = last_lost_ = 0;
  status_key_ = ~0u;
  draw_bar(ctx);
  s.invalidate_all();
}

bool Walkers::user_active() const
{
  return since_touch_ms_ < kTakeoverMs;
}

void Walkers::draw_bar(sim::Context& ctx)
{
  sim::Surface& s = ctx.surface;
  gfx::Canvas c(s.bg());
  c.fill_rect(bar_, ix::kBarBg);
  c.hline(0, bar_.y, bar_.w, ix::kBarBorder);
  const int active = user_active() ? selected_ : pilot_.selected();
  for (int i = 0; i < kSkillCount; ++i)
  {
    const Rect& b = buttons_[i];
    const Skill skill = static_cast<Skill>(i);
    c.fill_rect(b, ix::kButtonBg);
    if (i == active)
      c.rect(b, ix::kButtonActive, 2);
    else
      c.rect(b, ix::kButtonBorder);
    art::draw_icon(c, skill, b.x + (b.w - art::kIconSize) / 2, b.y + 3);
    const int left = world_.skills_left(skill);
    if (left != kUnlimited)
    {
      char n[8];
      std::snprintf(n, sizeof(n), "%d", left);
      c.text(b.x + b.w / 2, b.y + 22, n, left > 0 ? ix::kBarText : ix::kBarMuted, 1,
             Align::kCenter);
    }
  }
  draw_status(ctx);
  s.invalidate(bar_);
}

void Walkers::draw_status(sim::Context& ctx)
{
  gfx::Canvas c(ctx.surface.bg());
  const Rect row{buttons_[0].x, bar_.y + 2, buttons_[kSkillCount - 1].right() - buttons_[0].x, 9};
  c.fill_rect(row, ix::kBarBg);
  char text[40];
  std::snprintf(text, sizeof(text), "OUT %d/%d  IN %d/%d", world_.spawned(), world_.params().total,
                world_.saved(), world_.params().needed);
  c.text(row.x, row.y, text, ix::kBarText);
  c.text(row.right(), row.y, user_active() ? "YOU" : "AUTO", ix::kBarMuted, 1, Align::kRight);
}

int Walkers::button_at(int x, int y) const
{
  for (int i = 0; i < kSkillCount; ++i)
    if (buttons_[i].contains(x, y))
      return i;
  return -1;
}

int Walkers::walker_at(int x, int y, Skill s) const
{
  int best = -1;
  int best_d = kPickRadius * kPickRadius + 1;
  for (int i = 0; i < world_.count(); ++i)
  {
    const Walker& w = world_.walker(i);
    const int dx = w.x - x;
    const int dy = w.y - kWalkerH / 2 - y;
    const int d = dx * dx + dy * dy;
    if (d < best_d && world_.can_assign(i, s))
      best = i, best_d = d;
  }
  return best;
}

void Walkers::tap(sim::Context& ctx, int x, int y)
{
  since_touch_ms_ = 0;
  if (phase_ == Phase::kResult && !world_.won())
  {
    if (retry_button_.contains(x, y))
      new_level(ctx, true);
    else if (new_button_.contains(x, y))
      new_level(ctx, false);
    return;
  }
  if (bar_.contains(x, y))
  {
    const int b = button_at(x, y);
    if (b >= 0)
      selected_ = selected_ == b ? -1 : b;
    draw_bar(ctx);
    return;
  }
  if (phase_ == Phase::kPlaying && selected_ >= 0)
  {
    const Skill s = static_cast<Skill>(selected_);
    const int i = walker_at(x, y, s);
    if (i >= 0)
      world_.assign(i, s);
  }
  draw_bar(ctx);
}

void Walkers::effects()
{
  Effect e[World::kMaxEffects];
  const int n = world_.take_effects(e, World::kMaxEffects);
  for (int k = 0; k < n; ++k)
  {
    int count = 0;
    int vy0 = 0;
    switch (e[k].kind)
    {
      case Effect::Kind::kExplosion:
        count = 24, vy0 = -40;
        break;
      case Effect::Kind::kSplat:
        count = 6, vy0 = -16;
        break;
      case Effect::Kind::kSaved:
        count = 8, vy0 = -24;
        break;
      default:
        break;
    }
    sim::Rng rng(static_cast<uint32_t>(e[k].x * 131 + e[k].y * 7 + world_.ticks()));
    for (int i = 0, slot = 0; i < count && slot < kMaxParticles; ++slot)
    {
      Particle& p = particles_[slot];
      if (p.life)
        continue;
      p.x = static_cast<int16_t>(e[k].x * 16);
      p.y = static_cast<int16_t>(e[k].y * 16);
      p.vx = static_cast<int16_t>(rng.range(-24, 24));
      p.vy = static_cast<int16_t>(rng.range(vy0, vy0 / 4));
      p.life = static_cast<uint8_t>(rng.range(16, 32));
      p.colour = e[k].kind == Effect::Kind::kSaved ? ix::kAccent
                 : e[k].kind == Effect::Kind::kSplat
                     ? static_cast<uint8_t>(ix::kSparkFirst + 6)
                     : static_cast<uint8_t>(ix::kSparkFirst + rng.below(3));
      ++i;
    }
  }
}

void Walkers::end_level(sim::Context& ctx)
{
  (void)ctx;
  phase_ = Phase::kResult;
  phase_ms_ = 0;
  selected_ = -1;
}

void Walkers::logic_tick(sim::Context& ctx)
{
  if (phase_ == Phase::kPlaying)
  {
    if (user_active())
    {
      pilot_.observe(world_);
    }
    else
    {
      int i;
      Skill s;
      if (pilot_.tick(world_, &i, &s))
        world_.assign(i, s);
    }
    world_.tick();
    effects();
    if (world_.saved() != last_saved_ || world_.lost() != last_lost_)
    {
      last_saved_ = world_.saved();
      last_lost_ = world_.lost();
      since_progress_ms_ = 0;
    }
    if (world_.hatch_open() && !hatch_open_)
    {
      gfx::Canvas bg(ctx.surface.bg());
      const Rect h = art::hatch_rect(level_.hatch_x, level_.hatch_y + art::kSpriteH);
      for (int y = h.y; y < h.bottom() + 7; ++y)
        bg.hline(h.x, y, h.w, terrain_.sky(y));
      art::draw_hatch(bg, level_.hatch_x, level_.hatch_y + art::kSpriteH, true);
      ctx.surface.invalidate({h.x, h.y, h.w, h.h + 7});
      hatch_open_ = true;
    }
    const bool all_out = world_.spawned() == world_.params().total;
    if (world_.finished() || world_.hopeless() || world_.ticks() >= kMaxLevelTicks ||
        (all_out && since_progress_ms_ >= kStuckMs))
      end_level(ctx);
  }

  for (auto& p : particles_)
  {
    if (!p.life)
      continue;
    --p.life;
    p.x = static_cast<int16_t>(p.x + p.vx);
    p.y = static_cast<int16_t>(p.y + p.vy);
    p.vy = static_cast<int16_t>(p.vy + kGravity);
    if (p.colour >= ix::kSparkFirst && p.colour < ix::kSparkFirst + 7 && (p.life & 3) == 0)
      ++p.colour;  // cools
  }
}

void Walkers::redraw_fg(sim::Context& ctx)
{
  sim::Surface& s = ctx.surface;
  gfx::Canvas c(s.fg());
  for (int i = 0; i < drawn_count_; ++i)
  {
    c.fill_rect(drawn_[i], 0);
    s.invalidate(drawn_[i]);
  }
  drawn_count_ = 0;
  c.set_clip({0, 0, s.width(), play_h_});
  auto add = [&](const Rect& r)
  {
    if (drawn_count_ < kMaxDrawn)
      drawn_[drawn_count_++] = r;
    s.invalidate(r);
  };

  // Torches either side of the door.
  const Rect door = art::door_rect(level_.goal_x, level_.goal_y + 1);
  const uint8_t flame = static_cast<uint8_t>(ix::kFlame + ((world_.ticks() / 3) & 1));
  for (int fx : {door.x - 2, door.right()})
  {
    c.fill_rect({fx, door.y + 6, 2, 3}, flame);
    add({fx, door.y + 6, 2, 3});
  }

  for (int i = 0; i < world_.count(); ++i)
  {
    const Walker& w = world_.walker(i);
    if (w.active())
      add(art::draw_walker(c, w, world_.ticks()));
  }
  for (const auto& p : particles_)
    if (p.life)
    {
      const Rect r{p.x / 16, p.y / 16, 2, 2};
      c.fill_rect(r, p.colour);
      add(r);
    }

  // The autopilot's cursor, flying from the skill button to the walker.
  if (phase_ == Phase::kPlaying && !user_active() && pilot_.target() >= 0 && pilot_.selected() >= 0)
  {
    const Rect& b = buttons_[pilot_.selected()];
    const Walker& w = world_.walker(pilot_.target());
    const int f = pilot_.flight();
    const int x = b.x + b.w / 2 + (w.x - (b.x + b.w / 2)) * f / 256;
    const int y = b.y + b.h / 2 + (w.y - kWalkerH / 2 - (b.y + b.h / 2)) * f / 256;
    add(art::draw_cursor(c, x, std::min(y, play_h_ - 8)));
  }

  // Banners.
  const int w = s.width();
  char line[40];
  if (phase_ == Phase::kIntro)
  {
    const Rect r = panel_rect(w, play_h_, 44);
    c.fill_rect(r, ix::kPanel);
    c.rect(r, ix::kAccent);
    std::snprintf(line, sizeof(line), "Level %d", level_number_);
    c.text(r.x + r.w / 2, r.y + 8, line, ix::kAccent, 2, Align::kCenter);
    std::snprintf(line, sizeof(line), "Save %d of %d", level_.params.needed, level_.params.total);
    c.text(r.x + r.w / 2, r.y + 30, line, ix::kText, 1, Align::kCenter);
    add(r);
  }
  else if (phase_ == Phase::kResult)
  {
    const bool won = world_.won();
    const Rect r = panel_rect(w, play_h_, won ? 44 : 76);
    c.fill_rect(r, ix::kPanel);
    c.rect(r, won ? ix::kGreen : ix::kRed);
    c.text(r.x + r.w / 2, r.y + 8, won ? "Well done!" : "Not enough", won ? ix::kGreen : ix::kRed,
           2, Align::kCenter);
    std::snprintf(line, sizeof(line), "Saved %d, needed %d", world_.saved(),
                  world_.params().needed);
    c.text(r.x + r.w / 2, r.y + 30, line, ix::kText, 1, Align::kCenter);
    if (!won)
    {
      const int bw = (r.w - 24) / 2;
      retry_button_ = {r.x + 8, r.y + 46, bw, 22};
      new_button_ = {r.right() - 8 - bw, r.y + 46, bw, 22};
      for (const Rect* b : {&retry_button_, &new_button_})
      {
        c.rect(*b, ix::kAccent);
        c.text(b->x + b->w / 2, b->y + 7, b == &retry_button_ ? "Retry" : "New", ix::kText, 1,
               Align::kCenter);
      }
    }
    add(r);
  }
}

void Walkers::step(sim::Context& ctx, uint32_t dt_ms)
{
  since_touch_ms_ =
      since_touch_ms_ + dt_ms < since_touch_ms_ ? since_touch_ms_ : since_touch_ms_ + dt_ms;
  phase_ms_ += dt_ms;

  const Rect reserved = ctx.surface.reserved();
  if (!(reserved == reserved_))
  {
    reserved_ = reserved;
    layout(ctx);
    draw_bar(ctx);
  }

  switch (phase_)
  {
    case Phase::kIntro:
      if (phase_ms_ >= kIntroMs)
        phase_ = Phase::kPlaying, phase_ms_ = 0;
      break;
    case Phase::kPlaying:
      since_progress_ms_ += dt_ms;
      break;
    case Phase::kResult:
      if (world_.won() && phase_ms_ >= kWonMs)
      {
        new_level(ctx, false);
        return;
      }
      if (!world_.won() && !user_active() && phase_ms_ >= kAutoChoiceMs)
      {
        new_level(ctx, ctx.rng.chance(50));  // the autopilot picks retry or new
        return;
      }
      break;
  }

  acc_ms_ += dt_ms;
  int ticks = 0;
  while (acc_ms_ >= static_cast<uint32_t>(kTickMs) && ticks < kMaxTicksPerStep)
  {
    logic_tick(ctx);
    acc_ms_ -= kTickMs;
    ++ticks;
  }
  if (acc_ms_ >= static_cast<uint32_t>(kTickMs))
    acc_ms_ = 0;  // after a stall, don't try to catch up
  if (ticks == 0)
    return;

  redraw_fg(ctx);
  const Rect changed = terrain_.take_changed();
  if (!changed.empty())
    ctx.surface.invalidate(changed);

  // Redraw the bar when what it shows changes.
  uint32_t key = static_cast<uint32_t>(world_.spawned()) | world_.saved() << 6 |
                 (user_active() ? selected_ + 1 : pilot_.selected() + 1) << 12 |
                 (user_active() ? 1u : 0u) << 16;
  for (int i = 0; i < kSkillCount; ++i)
    key = key * 31 + static_cast<uint32_t>(world_.skills_left(static_cast<Skill>(i)));
  if (key != status_key_)
  {
    status_key_ = key;
    draw_bar(ctx);
  }
}

}  // namespace walkers
