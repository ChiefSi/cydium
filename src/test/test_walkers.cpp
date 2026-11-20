#include <cstdio>

#include "autopilot.h"
#include "gfx.h"
#include "level.h"
#include "terrain.h"
#include "unity.h"
#include "world.h"

namespace
{

using walkers::LevelParams;
using walkers::Material;
using walkers::Skill;
using walkers::State;
using walkers::Terrain;
using walkers::World;

constexpr int kW = 240;
constexpr int kH = 274;  // play area in portrait
constexpr int kFloor = 200;

// A layer with a play area of kW x kH, sky everywhere.
struct Fixture
{
  gfx::IndexedLayer layer;
  Terrain terrain;
  World world;

  Fixture()
  {
    layer.begin(240, 320);
    terrain.attach(layer, kH);
    terrain.clear();
  }

  void fill(int x, int y, int w, int h, Material m)
  {
    for (int yy = y; yy < y + h; ++yy)
      for (int xx = x; xx < x + w; ++xx)
        if (xx >= 0 && xx < kW && yy >= 0 && yy < kH)
          terrain.pixel(xx, yy) = m == Material::kDirt    ? walkers::pal::kDirtFirst
                                  : m == Material::kMetal ? walkers::pal::kMetalFirst
                                                          : terrain.sky(yy);
  }

  // One walker standing at (x, y) facing dir, unlimited skills.
  void start(int x, int y, int dir, int total = 1, int interval = 20)
  {
    LevelParams p;
    p.total = total;
    p.needed = total;
    p.first_spawn = 0;
    p.spawn_interval = interval;
    p.spawn_x = x;
    p.spawn_y = y;
    p.spawn_dir = static_cast<int8_t>(dir);
    p.goal = {-100, -100, 1, 1};
    p.skills.fill(walkers::kUnlimited);
    world.start(terrain, p, 1);
  }

  void run(int ticks)
  {
    for (int i = 0; i < ticks; ++i)
      world.tick();
  }

  const walkers::Walker& w(int i = 0)
  {
    return world.walker(i);
  }
};

void test_walks_and_turns_at_walls()
{
  Fixture f;
  f.fill(0, kFloor, kW, 20, Material::kDirt);
  f.fill(120, kFloor - 8, 6, 8, Material::kMetal);  // too tall to step up
  f.start(100, kFloor - 1, 1);
  f.run(2);
  TEST_ASSERT_EQUAL(State::kWalking, f.w().state);
  f.run(30);
  TEST_ASSERT_TRUE(f.w().x < 120);
  TEST_ASSERT_EQUAL(-1, f.w().dir);
  // The screen's left edge is a wall too.
  f.run(200);
  TEST_ASSERT_TRUE(f.w().x >= 0);
  TEST_ASSERT_TRUE(f.w().active());
}

void test_steps_up_small_ledges()
{
  Fixture f;
  f.fill(0, kFloor, kW, 20, Material::kDirt);
  f.fill(120, kFloor - 4, 60, 4, Material::kDirt);  // a 4 px step
  f.start(100, kFloor - 1, 1);
  f.run(40);
  TEST_ASSERT_EQUAL(kFloor - 5, f.w().y);
  TEST_ASSERT_EQUAL(1, f.w().dir);
}

void test_falls_splat_and_umbrella()
{
  Fixture safe;
  safe.fill(0, kFloor, kW, 20, Material::kDirt);
  safe.start(100, kFloor - 50, 1);
  safe.run(40);
  TEST_ASSERT_EQUAL(State::kWalking, safe.w().state);

  Fixture high;
  high.fill(0, kFloor, kW, 20, Material::kDirt);
  high.start(100, kFloor - 80, 1);
  high.run(60);
  TEST_ASSERT_EQUAL(State::kDead, high.w().state);
  TEST_ASSERT_EQUAL(1, high.world.lost());

  Fixture floaty;
  floaty.fill(0, kFloor, kW, 20, Material::kDirt);
  floaty.start(100, kFloor - 80, 1);
  floaty.world.tick();
  TEST_ASSERT_TRUE(floaty.world.assign(0, Skill::kUmbrella));
  floaty.run(120);
  TEST_ASSERT_EQUAL(State::kWalking, floaty.w().state);
}

void test_falling_out_of_the_bottom_kills()
{
  Fixture f;
  f.start(100, 100, 1);
  f.run(100);
  TEST_ASSERT_EQUAL(State::kDead, f.w().state);
  TEST_ASSERT_TRUE(f.world.finished());
  TEST_ASSERT_FALSE(f.world.won());
}

void test_digger()
{
  Fixture f;
  f.fill(0, kFloor, kW, 12, Material::kDirt);
  f.fill(0, kFloor + 40, kW, 12, Material::kDirt);
  f.start(100, kFloor - 1, 1);
  f.world.tick();
  TEST_ASSERT_TRUE(f.world.assign(0, Skill::kDigger));
  f.run(60);
  TEST_ASSERT_EQUAL(State::kWalking, f.w().state);
  TEST_ASSERT_EQUAL(kFloor + 39, f.w().y);  // through to the floor below
  TEST_ASSERT_EQUAL(Material::kAir, f.terrain.at(100, kFloor + 5));

  // Metal stops a digger.
  Fixture m;
  m.fill(0, kFloor, kW, 12, Material::kMetal);
  m.start(100, kFloor - 1, 1);
  m.world.tick();
  TEST_ASSERT_TRUE(m.world.assign(0, Skill::kDigger));
  m.run(4);
  TEST_ASSERT_EQUAL(State::kWalking, m.w().state);
  TEST_ASSERT_EQUAL(kFloor - 1, m.w().y);
}

void test_builder_climbs_a_wall()
{
  Fixture f;
  f.fill(0, kFloor, kW, 12, Material::kDirt);
  f.fill(140, kFloor - 10, 8, 10, Material::kMetal);  // 10 px wall
  f.start(124, kFloor - 1, 1);                        // 16 px from it
  f.world.tick();
  TEST_ASSERT_TRUE(f.world.assign(0, Skill::kBuilder));
  f.run(80);
  TEST_ASSERT_TRUE(f.w().x > 148);  // over the wall
  TEST_ASSERT_TRUE(f.w().active());
}

void test_builder_bridges_a_gap()
{
  Fixture f;
  f.fill(0, kFloor, 100, 12, Material::kDirt);
  f.fill(116, kFloor, kW - 116, 12, Material::kDirt);  // 16 px gap, open below
  f.start(98, kFloor - 1, 1);
  f.world.tick();
  TEST_ASSERT_TRUE(f.world.assign(0, Skill::kBuilder));
  f.run(100);
  TEST_ASSERT_TRUE(f.w().active());
  TEST_ASSERT_TRUE(f.w().x > 120);
  TEST_ASSERT_EQUAL(kFloor - 1, f.w().y);
}

void test_blocker_and_exploder()
{
  Fixture f;
  f.fill(0, kFloor, kW, 12, Material::kDirt);
  f.start(100, kFloor - 1, 1, 2, 30);
  f.run(12);  // walks to x = 110
  TEST_ASSERT_TRUE(f.world.assign(0, Skill::kBlocker));
  const int bx = f.w().x;
  f.run(60);
  // The second walker turns at the blocker and never passes it.
  TEST_ASSERT_TRUE(f.w(1).x < bx);
  // Blockers take no other skill, except an exploder.
  TEST_ASSERT_FALSE(f.world.can_assign(0, Skill::kDigger));
  TEST_ASSERT_TRUE(f.world.assign(0, Skill::kExploder));
  f.run(walkers::kFuseTicks + 1);
  TEST_ASSERT_EQUAL(State::kDead, f.w().state);
  TEST_ASSERT_EQUAL(Material::kAir, f.terrain.at(bx, kFloor + 2));  // blasted
  f.run(200);
  TEST_ASSERT_TRUE(f.w(1).x > bx + 5);  // released
}

void test_exploder_spares_metal()
{
  Fixture f;
  f.fill(0, kFloor, kW, 12, Material::kMetal);
  f.start(100, kFloor - 1, 1);
  f.world.tick();
  TEST_ASSERT_TRUE(f.world.assign(0, Skill::kExploder));
  f.run(walkers::kFuseTicks + 1);
  TEST_ASSERT_EQUAL(Material::kMetal, f.terrain.at(f.w().x, kFloor));
}

void test_skill_rules_and_counts()
{
  Fixture f;
  f.fill(0, kFloor, kW, 30, Material::kDirt);
  f.start(100, kFloor - 1, 1);
  LevelParams p = f.world.params();
  p.skills = {1, 0, 1, 0, 2};  // digger, builder, exploder, blocker, umbrella
  f.world.start(f.terrain, p, 1);
  f.world.tick();
  TEST_ASSERT_FALSE(f.world.can_assign(0, Skill::kBuilder));  // none left
  TEST_ASSERT_TRUE(f.world.assign(0, Skill::kDigger));
  TEST_ASSERT_EQUAL(0, f.world.skills_left(Skill::kDigger));
  TEST_ASSERT_FALSE(f.world.can_assign(0, Skill::kBlocker));   // busy digging
  TEST_ASSERT_TRUE(f.world.assign(0, Skill::kUmbrella));       // add-on
  TEST_ASSERT_FALSE(f.world.can_assign(0, Skill::kUmbrella));  // only once
  TEST_ASSERT_EQUAL(1, f.world.skills_left(Skill::kUmbrella));
}

void test_goal_saves()
{
  Fixture f;
  f.fill(0, kFloor, kW, 12, Material::kDirt);
  f.start(100, kFloor - 1, 1);
  LevelParams p = f.world.params();
  p.goal = {130, kFloor - 4, 4, 4};
  f.world.start(f.terrain, p, 1);
  f.run(60);
  TEST_ASSERT_EQUAL(State::kSaved, f.w().state);
  TEST_ASSERT_TRUE(f.world.finished());
  TEST_ASSERT_TRUE(f.world.won());
}

// Generates levels for many seeds in both orientations and plays each out with
// the checking autopilot. Most seeds must give a level, and nearly every
// level must be winnable (the game only shows ones that are).
void generated_levels(int w, int h)
{
  gfx::IndexedLayer layer;
  TEST_ASSERT_TRUE(layer.begin(w, 320 * 240 / w));
  Terrain terrain;
  terrain.attach(layer, h);
  int made = 0, won = 0;
  constexpr int kSeeds = 300;
  for (uint32_t seed = 1; seed <= kSeeds; ++seed)
  {
    walkers::Level level;
    if (!walkers::generate(seed, terrain, {0, 0, 0, 0}, &level))
      continue;
    ++made;
    TEST_ASSERT_TRUE(level.params.needed <= level.params.total);
    TEST_ASSERT_TRUE(level.params.needed >= 1);
    if (walkers::verify(level, terrain))
      ++won;
    else
      std::printf("  %dx%d seed %u: not won\n", w, h, static_cast<unsigned>(seed));
  }
  std::printf("%dx%d: %d of %d seeds made levels, %d of those winnable\n", w, h, made, kSeeds, won);
  TEST_ASSERT_TRUE(made >= kSeeds / 3);
  TEST_ASSERT_TRUE(won * 10 >= made * 9);
}

void test_generated_levels_portrait()
{
  generated_levels(240, 274);
}

void test_generated_levels_landscape()
{
  generated_levels(320, 194);
}

}  // namespace

void run_walkers_tests()
{
  RUN_TEST(test_walks_and_turns_at_walls);
  RUN_TEST(test_steps_up_small_ledges);
  RUN_TEST(test_falls_splat_and_umbrella);
  RUN_TEST(test_falling_out_of_the_bottom_kills);
  RUN_TEST(test_digger);
  RUN_TEST(test_builder_climbs_a_wall);
  RUN_TEST(test_builder_bridges_a_gap);
  RUN_TEST(test_blocker_and_exploder);
  RUN_TEST(test_exploder_spares_metal);
  RUN_TEST(test_skill_rules_and_counts);
  RUN_TEST(test_goal_saves);
  RUN_TEST(test_generated_levels_portrait);
  RUN_TEST(test_generated_levels_landscape);
}
