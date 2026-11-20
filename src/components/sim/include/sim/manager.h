#pragma once

#include <array>
#include <cstdint>

#include "sim/rng.h"
#include "sim/simulation.h"
#include "sim/surface.h"

namespace sim
{

// Holds the registered simulations and runs the active one on the surface.
// Simulations are registered once at boot and must outlive the Manager.
class Manager
{
 public:
  static constexpr int kMaxSimulations = 16;

  explicit Manager(Surface& surface, uint32_t seed = 1) : surface_(surface), rng_(seed) {}

  void seed(uint32_t seed)
  {
    rng_.seed_with(seed);
  }

  // Registers a simulation; its index is the registration order. Returns
  // false when the list is full.
  bool add(Simulation& s);

  int count() const
  {
    return count_;
  }
  const Simulation& at(int index) const
  {
    return *sims_[index];
  }

  // Index of the running simulation, or -1 if none.
  int active() const
  {
    return active_;
  }

  // Ends the current simulation and begins the one at index (an index out of
  // range selects 0). Returns false if it failed to begin; nothing runs then
  // and the screen is blank.
  bool select(int index);

  // Begins the active simulation again on a cleared surface, e.g. after the
  // orientation changed.
  bool restart();

  void step(uint32_t dt_ms);
  void tap(int x, int y);

 private:
  bool begin_active();
  void clear_surface();

  Surface& surface_;
  Rng rng_;
  std::array<Simulation*, kMaxSimulations> sims_{};
  int count_ = 0;
  int active_ = -1;
};

}  // namespace sim
