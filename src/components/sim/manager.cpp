#include "sim/manager.h"

namespace sim
{

bool Manager::add(Simulation& s)
{
  if (count_ >= kMaxSimulations)
    return false;
  sims_[count_++] = &s;
  return true;
}

void Manager::clear_surface()
{
  surface_.bg().clear(0);
  surface_.fg().clear(0);
  surface_.palette() = gfx::Palette{};
  surface_.invalidate_all();
}

bool Manager::begin_active()
{
  clear_surface();
  Context ctx{surface_, rng_};
  if (sims_[active_]->begin(ctx))
    return true;
  sims_[active_]->end();
  active_ = -1;
  clear_surface();
  return false;
}

bool Manager::select(int index)
{
  if (count_ == 0)
    return false;
  if (index < 0 || index >= count_)
    index = 0;
  if (active_ >= 0)
    sims_[active_]->end();
  active_ = index;
  return begin_active();
}

bool Manager::restart()
{
  return active_ >= 0 && begin_active();
}

void Manager::step(uint32_t dt_ms)
{
  if (active_ < 0)
    return;
  Context ctx{surface_, rng_};
  sims_[active_]->step(ctx, dt_ms);
}

void Manager::tap(int x, int y)
{
  if (active_ < 0)
    return;
  Context ctx{surface_, rng_};
  sims_[active_]->tap(ctx, x, y);
}

}  // namespace sim
