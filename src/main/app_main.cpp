// Phase 1 bring-up: backlight, display colour test with runtime inversion,
// BOOT button, light sensor and raw touch logging.

#include <cinttypes>

#include "board.h"
#include "display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch.h"

namespace
{

constexpr const char* kTag = "app";

struct TestColor
{
  const char* name;
  uint16_t value;
};

constexpr TestColor kColors[] = {
    {"black", display::rgb565_be(0, 0, 0)},       {"red", display::rgb565_be(255, 0, 0)},
    {"green", display::rgb565_be(0, 255, 0)},     {"blue", display::rgb565_be(0, 0, 255)},
    {"white", display::rgb565_be(255, 255, 255)}, {"grey", display::rgb565_be(64, 64, 64)},
};
constexpr int kNumColors = sizeof(kColors) / sizeof(kColors[0]);

void touch_task(void*)
{
  for (;;)
  {
    touch::RawSample s;
    if (touch::irq_active() && touch::read_raw(&s))
    {
      ESP_LOGI("touch", "raw x=%4u y=%4u z=%4u", s.x, s.y, s.z);
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    else
    {
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
}

void log_status()
{
  ESP_LOGI(kTag, "light=%d (lower = brighter)  heap free=%u largest=%u", board::read_light_raw(),
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
           static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}

}  // namespace

extern "C" void app_main()
{
  ESP_ERROR_CHECK(board::init());
  ESP_ERROR_CHECK(display::init());
  board::set_backlight(255);
  ESP_ERROR_CHECK(touch::init());

  xTaskCreatePinnedToCore(touch_task, "touch", 3072, nullptr, 5, nullptr, 1);

  ESP_LOGI(kTag, "colour test running; press BOOT to toggle colour inversion");

  int color = 0;
  bool button_was_down = false;
  int64_t next_color_us = 0;
  int64_t next_status_us = 0;

  for (;;)
  {
    int64_t now = esp_timer_get_time();

    if (now >= next_color_us)
    {
      int64_t t0 = esp_timer_get_time();
      display::fill(kColors[color].value);
      ESP_LOGI(kTag, "fill %-5s in %" PRId64 " us", kColors[color].name, esp_timer_get_time() - t0);
      color = (color + 1) % kNumColors;
      next_color_us = now + 1500 * 1000;
    }

    bool down = board::boot_button_down();
    if (down && !button_was_down)
    {
      display::set_invert(!display::inverted());
      ESP_LOGI(kTag, "colour inversion %s", display::inverted() ? "ON" : "OFF");
    }
    button_was_down = down;

    if (now >= next_status_us)
    {
      log_status();
      next_status_us = now + 5000 * 1000;
    }

    vTaskDelay(pdMS_TO_TICKS(30));  // also debounces the button
  }
}
