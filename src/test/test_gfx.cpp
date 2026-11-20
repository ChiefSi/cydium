#include <cstring>
#include <vector>

#include "gfx.h"
#include "unity.h"

namespace
{

using gfx::Rect;

void test_rect_intersect()
{
  const Rect a{0, 0, 10, 10};
  TEST_ASSERT_TRUE((a.intersect({5, 5, 10, 10}) == Rect{5, 5, 5, 5}));
  TEST_ASSERT_TRUE(a.intersect({10, 0, 5, 5}).empty());
  TEST_ASSERT_TRUE(a.contains(9, 9));
  TEST_ASSERT_FALSE(a.contains(10, 9));
}

void test_colour_conversion()
{
  TEST_ASSERT_EQUAL_HEX16(0x00F8, gfx::rgb565_be(255, 0, 0));
  TEST_ASSERT_EQUAL_HEX16(0xE007, gfx::rgb565_be(0, 255, 0));
  TEST_ASSERT_EQUAL_HEX16(0x1F00, gfx::rgb565_be(0, 0, 255));
  TEST_ASSERT_EQUAL_HEX16(gfx::rgb565_be(120, 124, 120),
                          gfx::dim_be(gfx::rgb565_be(240, 248, 240)));
}

void test_blend()
{
  const uint16_t red = gfx::rgb565_be(255, 0, 0);
  const uint16_t blue = gfx::rgb565_be(0, 0, 255);
  TEST_ASSERT_EQUAL_HEX16(red, gfx::blend_be(blue, red, 255));
  TEST_ASSERT_EQUAL_HEX16(blue, gfx::blend_be(blue, red, 0));
  const uint16_t mid = gfx::swap16(gfx::blend_be(blue, red, 128));
  TEST_ASSERT_INT_WITHIN(1, 16, (mid >> 11) & 0x1F);  // half red
  TEST_ASSERT_INT_WITHIN(1, 15, mid & 0x1F);          // half blue
}

void test_layer_resize_keeps_storage()
{
  gfx::IndexedLayer l;
  TEST_ASSERT_TRUE(l.begin(240, 320));
  TEST_ASSERT_FALSE(l.resize(200, 320));  // wrong pixel count
  TEST_ASSERT_TRUE(l.resize(320, 240));
  TEST_ASSERT_EQUAL(320, l.width());
  TEST_ASSERT_EQUAL(240, l.height());

  // Every pixel is addressable and distinct in both orientations.
  for (int pass = 0; pass < 2; ++pass)
  {
    const int w = l.width(), h = l.height();
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x)
        l.row(y)[x] = static_cast<uint8_t>((x * 7 + y * 13) & 0xFF);
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x)
        TEST_ASSERT_EQUAL_UINT8((x * 7 + y * 13) & 0xFF, l.row(y)[x]);
    TEST_ASSERT_TRUE(l.resize(240, 320));
  }
}

int count(gfx::IndexedLayer& l, uint8_t v)
{
  int n = 0;
  for (int y = 0; y < l.height(); ++y)
    for (int x = 0; x < l.width(); ++x)
      n += l.row(y)[x] == v;
  return n;
}

void test_canvas_clipping()
{
  gfx::IndexedLayer l;
  TEST_ASSERT_TRUE(l.begin(240, 320));
  gfx::Canvas c(l);
  c.clear(0);
  c.fill_rect({-10, -10, 20, 20}, 1);  // 10x10 visible
  TEST_ASSERT_EQUAL(100, count(l, 1));
  c.hline(230, 5, 50, 2);  // 10 visible
  TEST_ASSERT_EQUAL(10, count(l, 2));
  c.set_clip({100, 100, 10, 10});
  c.fill_rect({0, 0, 240, 320}, 3);
  TEST_ASSERT_EQUAL(100, count(l, 3));
  c.reset_clip();
  c.rect({50, 50, 10, 10}, 4);
  TEST_ASSERT_EQUAL(36, count(l, 4));
}

void test_canvas_line_and_circle()
{
  gfx::IndexedLayer l;
  TEST_ASSERT_TRUE(l.begin(240, 320));
  gfx::Canvas c(l);
  c.clear(0);
  c.line(0, 0, 9, 9, 1);
  TEST_ASSERT_EQUAL(10, count(l, 1));
  TEST_ASSERT_EQUAL_UINT8(1, l.row(9)[9]);
  c.fill_circle(100, 100, 5, 2);
  TEST_ASSERT_EQUAL_UINT8(2, l.row(100)[100]);
  TEST_ASSERT_EQUAL_UINT8(2, l.row(95)[100]);
  TEST_ASSERT_EQUAL_UINT8(0, l.row(95)[95]);
}

void test_text()
{
  gfx::IndexedLayer l;
  TEST_ASSERT_TRUE(l.begin(240, 320));
  gfx::Canvas c(l);
  c.clear(0);
  TEST_ASSERT_EQUAL(16, gfx::kFont8x8.text_width("AB"));
  TEST_ASSERT_EQUAL(48, c.text(0, 0, "ABC", 1, 2));
  // 'A' top row is 0x0C: pixels 2 and 3 set, drawn at scale 2.
  TEST_ASSERT_EQUAL_UINT8(0, l.row(0)[3]);
  TEST_ASSERT_EQUAL_UINT8(1, l.row(0)[4]);
  TEST_ASSERT_EQUAL_UINT8(1, l.row(1)[7]);
  TEST_ASSERT_EQUAL_UINT8(0, l.row(0)[8]);
  // Right alignment ends at x.
  c.clear(0);
  c.text(100, 0, "_", 5, 1, gfx::Align::kRight);  // '_' is the bottom row
  TEST_ASSERT_EQUAL_UINT8(5, l.row(7)[99]);
  TEST_ASSERT_EQUAL_UINT8(5, l.row(7)[92]);
  TEST_ASSERT_EQUAL_UINT8(0, l.row(7)[100]);
}

void test_blit8()
{
  gfx::IndexedLayer l;
  TEST_ASSERT_TRUE(l.begin(240, 320));
  gfx::Canvas c(l);
  c.clear(9);
  const uint8_t sprite[] = {1, 0, 2, 3};
  c.blit8(sprite, 2, 2, 10, 10, 0, false, 10);
  TEST_ASSERT_EQUAL_UINT8(11, l.row(10)[10]);
  TEST_ASSERT_EQUAL_UINT8(9, l.row(10)[11]);  // transparent
  c.blit8(sprite, 2, 2, 20, 20, 0, true);
  TEST_ASSERT_EQUAL_UINT8(1, l.row(20)[21]);  // flipped
  TEST_ASSERT_EQUAL_UINT8(3, l.row(21)[20]);
}

void test_chunk_canvas_stays_in_chunk()
{
  constexpr int kW = 320, kRows = 16;
  std::vector<uint16_t> buf(kW * (kRows + 2), 0xAAAA);  // guard rows either side
  gfx::ChunkCanvas c(buf.data() + kW, kW, 32, kRows);   // screen rows 32..47
  c.fill_rect({0, 0, kW, 240}, 0x1234);
  for (int i = 0; i < kW; ++i)
  {
    TEST_ASSERT_EQUAL_HEX16(0xAAAA, buf[i]);
    TEST_ASSERT_EQUAL_HEX16(0xAAAA, buf[kW * (kRows + 1) + i]);
  }
  TEST_ASSERT_EQUAL_HEX16(0x1234, buf[kW + 5]);
  c.text(0, 40, "Hello", 0xFFFF, 2);  // straddles the chunk's bottom edge
  for (int i = 0; i < kW; ++i)
    TEST_ASSERT_EQUAL_HEX16(0xAAAA, buf[kW * (kRows + 1) + i]);

  const uint16_t px[] = {gfx::rgb565_be(255, 255, 255)};
  const uint8_t alpha[] = {255};
  c.blend(px, alpha, 1, 1, 3, 33);
  TEST_ASSERT_EQUAL_HEX16(px[0], buf[kW + kW + 3]);
  c.dim({3, 33, 1, 1});
  TEST_ASSERT_EQUAL_HEX16(gfx::dim_be(px[0]), buf[kW + kW + 3]);
}

}  // namespace

void run_gfx_tests()
{
  RUN_TEST(test_rect_intersect);
  RUN_TEST(test_colour_conversion);
  RUN_TEST(test_blend);
  RUN_TEST(test_layer_resize_keeps_storage);
  RUN_TEST(test_canvas_clipping);
  RUN_TEST(test_canvas_line_and_circle);
  RUN_TEST(test_text);
  RUN_TEST(test_blit8);
  RUN_TEST(test_chunk_canvas_stays_in_chunk);
}
