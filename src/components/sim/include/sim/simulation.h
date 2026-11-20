#pragma once

#include <cstdint>

#include "sim/rng.h"
#include "sim/surface.h"

namespace sim
{

// What a simulation gets from the Manager on every call.
struct Context
{
  Surface& surface;
  Rng& rng;
};

// One scene. Only the active simulation holds resources: the Manager calls
// begin() when it is selected and end() before switching away, so anything
// larger than a few hundred bytes should be allocated in begin() and freed in
// end(), never per step.
class Simulation
{
 public:
  virtual ~Simulation() = default;

  virtual const char* name() const = 0;
  virtual const char* description() const = 0;  // one short line, for the menu

  // Sets up the palette and state and draws the whole scene at the surface's
  // current size. On entry both layers are cleared to 0, the palette is all
  // black and everything is marked dirty. Also called again after the size
  // changes (orientation), without end() in between. Returns false if it
  // cannot run (e.g. out of memory); end() is still called.
  virtual bool begin(Context& ctx) = 0;

  // Frees what begin() allocated.
  virtual void end() {}

  // Advances by dt_ms and draws the changes, invalidating what it touched.
  virtual void step(Context& ctx, uint32_t dt_ms) = 0;

  // A tap at screen coordinates that the menu didn't use.
  virtual void tap(Context& ctx, int x, int y)
  {
    (void)ctx;
    (void)x;
    (void)y;
  }
};

}  // namespace sim
