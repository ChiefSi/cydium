#include <cstring>

#include "bounce.h"
#include "gfx.h"
#include "sim.h"
#include "starfield.h"
#include "unity.h"
#include "walkers.h"

namespace
{

class FakeSurface : public sim::Surface
{
 public:
  FakeSurface()
  {
    bg_.begin(240, 320);
    fg_.begin(240, 320);
  }
  gfx::IndexedLayer& bg() override
  {
    return bg_;
  }
  gfx::IndexedLayer& fg() override
  {
    return fg_;
  }
  gfx::Palette& palette() override
  {
    return palette_;
  }
  void invalidate(const gfx::Rect& r) override
  {
    if (!r.empty())
      ++invalidations;
  }
  void invalidate_all() override
  {
    ++invalidate_alls;
  }
  void rotate()
  {
    const int w = bg_.height(), h = bg_.width();
    bg_.resize(w, h);
    fg_.resize(w, h);
  }

  int invalidations = 0;
  int invalidate_alls = 0;

 private:
  gfx::IndexedLayer bg_;
  gfx::IndexedLayer fg_;
  gfx::Palette palette_;
};

// Records the calls the Manager makes.
class Probe : public sim::Simulation
{
 public:
  explicit Probe(bool ok = true) : ok_(ok) {}
  const char* name() const override
  {
    return "Probe";
  }
  const char* description() const override
  {
    return "test";
  }
  bool begin(sim::Context& ctx) override
  {
    ++begins;
    // The surface must arrive cleared.
    saw_clear = ctx.surface.bg().row(0)[0] == 0 && ctx.surface.fg().row(0)[0] == 0 &&
                ctx.surface.palette()[1] == 0;
    ctx.surface.bg().clear(7);
    ctx.surface.fg().clear(7);
    ctx.surface.palette().set(1, 255, 255, 255);
    return ok_;
  }
  void end() override
  {
    ++ends;
  }
  void step(sim::Context&, uint32_t dt_ms) override
  {
    ++steps;
    last_dt = dt_ms;
  }
  void tap(sim::Context&, int x, int y) override
  {
    tap_x = x;
    tap_y = y;
  }

  int begins = 0, ends = 0, steps = 0, tap_x = -1, tap_y = -1;
  uint32_t last_dt = 0;
  bool saw_clear = false;

 private:
  bool ok_;
};

void test_rng_is_repeatable_and_in_range()
{
  sim::Rng a(42), b(42), zero(0);
  for (int i = 0; i < 100; ++i)
    TEST_ASSERT_EQUAL_UINT32(a.next(), b.next());
  for (int i = 0; i < 1000; ++i)
  {
    const int v = a.range(-3, 5);
    TEST_ASSERT_TRUE(v >= -3 && v <= 5);
    TEST_ASSERT_TRUE(a.below(7) < 7);
    TEST_ASSERT_NOT_EQUAL(0u, zero.next());  // a zero seed still works
  }
  TEST_ASSERT_EQUAL(0, a.below(0));
}

void test_manager_switches_simulations()
{
  FakeSurface s;
  sim::Manager m(s, 1);
  Probe p0, p1;
  TEST_ASSERT_FALSE(m.select(0));  // nothing registered
  TEST_ASSERT_TRUE(m.add(p0));
  TEST_ASSERT_TRUE(m.add(p1));
  TEST_ASSERT_EQUAL(2, m.count());
  TEST_ASSERT_EQUAL(-1, m.active());

  m.step(10);  // nothing active: ignored
  TEST_ASSERT_TRUE(m.select(1));
  TEST_ASSERT_EQUAL(1, m.active());
  TEST_ASSERT_EQUAL(1, p1.begins);
  TEST_ASSERT_TRUE(p1.saw_clear);
  TEST_ASSERT_TRUE(s.invalidate_alls > 0);

  m.step(33);
  m.tap(5, 6);
  TEST_ASSERT_EQUAL(1, p1.steps);
  TEST_ASSERT_EQUAL_UINT32(33, p1.last_dt);
  TEST_ASSERT_EQUAL(5, p1.tap_x);
  TEST_ASSERT_EQUAL(0, p0.steps);

  // Switching ends the old one and hands the new one a cleared surface.
  TEST_ASSERT_TRUE(m.select(0));
  TEST_ASSERT_EQUAL(1, p1.ends);
  TEST_ASSERT_EQUAL(1, p0.begins);
  TEST_ASSERT_TRUE(p0.saw_clear);

  // restart() begins again without end(); out of range selects 0.
  TEST_ASSERT_TRUE(m.restart());
  TEST_ASSERT_EQUAL(2, p0.begins);
  TEST_ASSERT_TRUE(p0.saw_clear);
  TEST_ASSERT_EQUAL(0, p0.ends);
  TEST_ASSERT_TRUE(m.select(200));
  TEST_ASSERT_EQUAL(0, m.active());
}

void test_manager_handles_failed_begin()
{
  FakeSurface s;
  sim::Manager m(s, 1);
  Probe ok, bad(false);
  m.add(ok);
  m.add(bad);
  TEST_ASSERT_FALSE(m.select(1));
  TEST_ASSERT_EQUAL(-1, m.active());
  TEST_ASSERT_EQUAL(1, bad.ends);                // end() still called to free
  TEST_ASSERT_EQUAL_UINT8(0, s.bg().row(0)[0]);  // left blank
  TEST_ASSERT_FALSE(m.restart());
  m.step(10);
  TEST_ASSERT_EQUAL(0, bad.steps);
  TEST_ASSERT_TRUE(m.select(0));
}

// Runs a simulation through both orientations with taps, checking that it
// draws something and keeps invalidating as it animates.
void exercise(sim::Simulation& sim)
{
  FakeSurface s;
  sim::Manager m(s, 1234);
  m.add(sim);
  TEST_ASSERT_TRUE(m.select(0));
  for (int pass = 0; pass < 2; ++pass)
  {
    bool drew_bg = false;
    for (int y = 0; y < s.height() && !drew_bg; ++y)
      for (int x = 0; x < s.width() && !drew_bg; ++x)
        drew_bg = s.bg().row(y)[x] != 0;
    TEST_ASSERT_TRUE(drew_bg);

    s.invalidations = 0;
    for (int i = 0; i < 600; ++i)
    {
      m.step(i % 50 == 0 ? 500 : 33);  // include stalls
      if (i % 97 == 0)
        m.tap((i * 7) % s.width(), (i * 13) % s.height());
    }
    m.tap(s.width() - 1, s.height() - 1);
    m.tap(0, 0);
    m.step(33);
    TEST_ASSERT_TRUE(s.invalidations > 0);

    s.rotate();
    TEST_ASSERT_TRUE(m.restart());
  }
}

void test_bounce_runs()
{
  bounce::Bounce b;
  exercise(b);
}

void test_starfield_runs()
{
  starfield::Starfield sf;
  exercise(sf);
}

void test_walkers_runs()
{
  walkers::Walkers w;
  exercise(w);
}

}  // namespace

void run_sim_tests()
{
  RUN_TEST(test_rng_is_repeatable_and_in_range);
  RUN_TEST(test_manager_switches_simulations);
  RUN_TEST(test_manager_handles_failed_begin);
  RUN_TEST(test_bounce_runs);
  RUN_TEST(test_starfield_runs);
  RUN_TEST(test_walkers_runs);
}
