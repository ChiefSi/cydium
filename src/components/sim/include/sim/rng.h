#pragma once

#include <cstdint>

namespace sim
{

// Small seedable pseudo-random generator (xorshift32), so simulations are
// repeatable in host tests. The firmware seeds it from esp_random().
class Rng
{
 public:
  explicit Rng(uint32_t seed = 1)
  {
    seed_with(seed);
  }

  void seed_with(uint32_t seed)
  {
    state_ = seed ? seed : 0x9E3779B9u;  // xorshift must not start at 0
  }

  uint32_t next()
  {
    uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return state_ = x;
  }

  // Uniform in [0, n); 0 when n <= 0.
  int below(int n)
  {
    return n > 0 ? static_cast<int>((static_cast<uint64_t>(next()) * n) >> 32) : 0;
  }

  // Uniform in [lo, hi] (inclusive).
  int range(int lo, int hi)
  {
    return lo + below(hi - lo + 1);
  }

  bool chance(int percent)
  {
    return below(100) < percent;
  }

 private:
  uint32_t state_ = 1;
};

}  // namespace sim
