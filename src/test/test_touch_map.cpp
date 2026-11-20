#include "touch_map.h"
#include "unity.h"

namespace
{

using gfx::Orientation;
using touch::Calibration;
using touch::Point;

constexpr Calibration kPlain = {0, 4000, 0, 4000, false, false, false};

void assert_point(int x, int y, Point p)
{
  TEST_ASSERT_EQUAL(x, p.x);
  TEST_ASSERT_EQUAL(y, p.y);
}

void test_linear_mapping()
{
  assert_point(0, 0, touch::map_to_portrait(0, 0, kPlain));
  assert_point(120, 160, touch::map_to_portrait(2000, 2000, kPlain));
  assert_point(239, 319, touch::map_to_portrait(4000, 4000, kPlain));
}

void test_clamping()
{
  constexpr Calibration c = {200, 3800, 300, 3700, false, false, false};
  assert_point(0, 0, touch::map_to_portrait(10, 10, c));
  assert_point(239, 319, touch::map_to_portrait(4095, 4095, c));
}

void test_swap_and_flip()
{
  // Raw X runs along the long axis: raw x drives screen y.
  constexpr Calibration swap = {0, 4000, 0, 4000, true, false, false};
  assert_point(0, 319, touch::map_to_portrait(4000, 0, swap));
  constexpr Calibration flip = {0, 4000, 0, 4000, false, true, true};
  assert_point(239, 319, touch::map_to_portrait(0, 0, flip));
}

void test_rotation()
{
  const Point top_left{0, 0};
  assert_point(0, 0, touch::rotate_from_portrait(top_left, Orientation::kPortrait));
  // Clockwise (USB on the left): portrait top-left becomes top-right.
  assert_point(319, 0, touch::rotate_from_portrait(top_left, Orientation::kLandscapeCw));
  // Anticlockwise (USB on the right): portrait top-left becomes bottom-left.
  assert_point(0, 239, touch::rotate_from_portrait(top_left, Orientation::kLandscapeCcw));
  // Portrait bottom-left (next to USB) ends up on the USB side.
  const Point bottom_left{0, 319};
  assert_point(0, 0, touch::rotate_from_portrait(bottom_left, Orientation::kLandscapeCw));
  assert_point(319, 239, touch::rotate_from_portrait(bottom_left, Orientation::kLandscapeCcw));
}

// Rough corner presses logged on a real CYD, with its calibrated Kconfig
// defaults: each must land in the matching quadrant of the portrait screen.
void test_board_calibration_corners()
{
  constexpr Calibration board = {261, 3843, 196, 3847, false, true, false};
  struct Sample
  {
    uint16_t x, y;
    bool right, bottom;
  };
  const Sample samples[] = {
      {3435, 589, false, false},  // top left
      {694, 552, true, false},    // top right
      {3243, 3477, false, true},  // bottom left
      {622, 3570, true, true},    // bottom right
  };
  for (const Sample& s : samples)
  {
    const Point p = touch::map_to_portrait(s.x, s.y, board);
    TEST_ASSERT_TRUE(s.right ? p.x >= 120 : p.x < 120);
    TEST_ASSERT_TRUE(s.bottom ? p.y >= 160 : p.y < 160);
  }
}

// Raw reading that map_to_portrait() turns into (sx, sy): the inverse mapping.
touch::CalPoint press(int sx, int sy, const Calibration& c)
{
  const int vx = c.flip_x ? gfx::kPortraitWidth - sx : sx;
  const int vy = c.flip_y ? gfx::kPortraitHeight - sy : sy;
  const int ax_min = c.swap_xy ? c.y_min : c.x_min, ax_max = c.swap_xy ? c.y_max : c.x_max;
  const int by_min = c.swap_xy ? c.x_min : c.y_min, by_max = c.swap_xy ? c.x_max : c.y_max;
  const int a = ax_min + vx * (ax_max - ax_min) / gfx::kPortraitWidth;
  const int b = by_min + vy * (by_max - by_min) / gfx::kPortraitHeight;
  return {static_cast<int16_t>(sx), static_cast<int16_t>(sy),
          static_cast<uint16_t>(c.swap_xy ? b : a), static_cast<uint16_t>(c.swap_xy ? a : b)};
}

void check_fit(const Calibration& truth)
{
  const touch::CalPoint p[4] = {press(20, 20, truth), press(219, 20, truth), press(219, 299, truth),
                                press(20, 299, truth)};
  const Calibration fit = touch::fit_calibration(p);
  TEST_ASSERT_EQUAL(truth.swap_xy, fit.swap_xy);
  TEST_ASSERT_EQUAL(truth.flip_x, fit.flip_x);
  TEST_ASSERT_EQUAL(truth.flip_y, fit.flip_y);
  TEST_ASSERT_INT_WITHIN(20, truth.x_min, fit.x_min);
  TEST_ASSERT_INT_WITHIN(20, truth.x_max, fit.x_max);
  TEST_ASSERT_INT_WITHIN(20, truth.y_min, fit.y_min);
  TEST_ASSERT_INT_WITHIN(20, truth.y_max, fit.y_max);
  // And the fit maps the targets back onto themselves.
  for (const auto& q : p)
  {
    const Point m = touch::map_to_portrait(q.rx, q.ry, fit);
    TEST_ASSERT_INT_WITHIN(2, q.sx, m.x);
    TEST_ASSERT_INT_WITHIN(2, q.sy, m.y);
  }
}

void test_fit_calibration()
{
  check_fit({200, 3800, 300, 3700, false, false, false});
  check_fit({470, 3530, 420, 3680, false, true, false});
  check_fit({250, 3900, 150, 3850, true, false, true});
  check_fit({300, 3700, 200, 3600, true, true, true});
}

}  // namespace

void run_touch_map_tests()
{
  RUN_TEST(test_linear_mapping);
  RUN_TEST(test_clamping);
  RUN_TEST(test_swap_and_flip);
  RUN_TEST(test_rotation);
  RUN_TEST(test_board_calibration_corners);
  RUN_TEST(test_fit_calibration);
}
