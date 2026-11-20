#include "unity.h"

void run_gfx_tests();
void run_touch_map_tests();

void setUp() {}
void tearDown() {}

int main()
{
  UNITY_BEGIN();
  run_gfx_tests();
  run_touch_map_tests();
  return UNITY_END();
}
