// cydium: renderer test scene with the settings menu.
//
// The scene draws a gradient and grid on the background layer and bouncing
// balls on the foreground layer; only the chunks the balls touch are resent.
// The menu icon opens About and Settings; tapping elsewhere moves the first
// ball to the tap. Frame rate and heap are logged every 5 seconds.

#include <cinttypes>

#include "board.h"
#include "buildinfo.h"
#include "display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gfx.h"
#include "menu.h"
#include "renderer.h"
#include "settings.h"
#include "touch.h"

namespace
{

constexpr const char* kTag = "app";
constexpr int kFrameMs = 33;  // ~30 fps target

// Palette layout.
constexpr uint8_t kGradientFirst = 1;  // 1..32: background gradient
constexpr int kGradientCount = 32;
constexpr uint8_t kGrid = 40;
constexpr uint8_t kBallFirst = 48;  // 48..: ball colours
constexpr int kNumBalls = 5;
constexpr int kBallRadius = 12;

display::Renderer s_renderer;
ui::Menu s_menu;
QueueHandle_t s_taps = nullptr;

struct Ball
{
  int x, y;    // centre, in pixels
  int vx, vy;  // pixels per frame
  uint8_t colour;
};

class TestScene
{
 public:
  void setup_palette(gfx::Palette& p)
  {
    p.set(0, 0, 0, 0);
    for (int i = 0; i < kGradientCount; ++i)
      p.set(kGradientFirst + i, 8, 16 + i * 3, 40 + i * 4);
    p.set(kGrid, 40, 70, 110);
    const uint8_t rgb[kNumBalls][3] = {
        {255, 80, 80}, {80, 220, 120}, {90, 150, 255}, {255, 210, 70}, {230, 110, 255}};
    for (int i = 0; i < kNumBalls; ++i)
      p.set(kBallFirst + i, rgb[i][0], rgb[i][1], rgb[i][2]);
  }

  // (Re)draws the background and restarts the balls for the current size.
  void reset(display::Renderer& r)
  {
    const int w = r.bg().width();
    const int h = r.bg().height();
    gfx::Canvas bg(r.bg());
    for (int y = 0; y < h; ++y)
      bg.hline(0, y, w, static_cast<uint8_t>(kGradientFirst + y * kGradientCount / h));
    for (int x = 0; x < w; x += 40)
      bg.vline(x, 0, h, kGrid);
    for (int y = 0; y < h; y += 40)
      bg.hline(0, y, w, kGrid);

    r.fg().clear(0);
    for (int i = 0; i < kNumBalls; ++i)
    {
      Ball& b = balls_[i];
      b.x = kBallRadius + static_cast<int>(esp_random() % (w - 2 * kBallRadius));
      b.y = kBallRadius + static_cast<int>(esp_random() % (h - 2 * kBallRadius));
      b.vx = (esp_random() & 1) ? 2 + i % 3 : -(2 + i % 3);
      b.vy = (esp_random() & 1) ? 1 + i % 4 : -(1 + i % 4);
      b.colour = static_cast<uint8_t>(kBallFirst + i);
    }
    draw(r);
    r.invalidate_all();
  }

  void step(display::Renderer& r)
  {
    gfx::Canvas fg(r.fg());
    const int w = r.fg().width();
    const int h = r.fg().height();
    for (auto& b : balls_)
    {
      fg.fill_rect(bounds(b), 0);
      r.invalidate(bounds(b));
      b.x += b.vx;
      b.y += b.vy;
      if (b.x < kBallRadius || b.x >= w - kBallRadius)
      {
        b.vx = -b.vx;
        b.x = b.x < kBallRadius ? kBallRadius : w - kBallRadius - 1;
      }
      if (b.y < kBallRadius || b.y >= h - kBallRadius)
      {
        b.vy = -b.vy;
        b.y = b.y < kBallRadius ? kBallRadius : h - kBallRadius - 1;
      }
    }
    draw(r);
  }

  void move_first_ball(int x, int y)
  {
    balls_[0].x = x;
    balls_[0].y = y;
  }

 private:
  static gfx::Rect bounds(const Ball& b)
  {
    return {b.x - kBallRadius, b.y - kBallRadius, 2 * kBallRadius + 1, 2 * kBallRadius + 1};
  }

  void draw(display::Renderer& r)
  {
    gfx::Canvas fg(r.fg());
    for (const auto& b : balls_)
    {
      fg.fill_circle(b.x, b.y, kBallRadius, b.colour);
      r.invalidate(bounds(b));
    }
  }

  Ball balls_[kNumBalls] = {};
};

TestScene s_scene;

gfx::Orientation orientation_from_setting()
{
  return settings::get(settings::Id::kOrientation) == settings::kLandscape
             ? display::landscape_orientation()
             : gfx::Orientation::kPortrait;
}

void apply_orientation()
{
  display::set_orientation(orientation_from_setting());
  s_renderer.resize();
  s_scene.reset(s_renderer);
  s_menu.relayout();
}

void on_setting_changed(settings::Id id, uint8_t value, void*)
{
  switch (id)
  {
    case settings::Id::kInvert:
      display::set_invert(value != 0);
      break;
    case settings::Id::kOrientation:
      apply_orientation();
      break;
    case settings::Id::kIconPosition:
      s_menu.relayout();
      break;
    default:
      break;
  }
}

// Samples touch and queues one raw sample per press (a new press needs a
// clear release of at least 100 ms first).
void input_task(void*)
{
  bool down = false;
  int up_ms = 0;
  for (;;)
  {
    touch::RawSample s;
    if (touch::irq_active() && touch::read_raw(&s))
    {
      if (!down)
        xQueueSend(s_taps, &s, 0);
      down = true;
      up_ms = 0;
    }
    else if (down && (up_ms += 20) >= 100)
    {
      down = false;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

uint32_t now_ms()
{
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

}  // namespace

extern "C" void app_main()
{
  ESP_LOGI(kTag, "%s %s", buildinfo::name(), buildinfo::version());

  settings::init();  // failures are logged; defaults are used
  ESP_ERROR_CHECK(board::init());
  ESP_ERROR_CHECK(display::init());
  display::set_invert(settings::get_bool(settings::Id::kInvert));
  display::set_orientation(orientation_from_setting());

  ESP_ERROR_CHECK(s_renderer.begin());
  s_scene.setup_palette(s_renderer.palette());
  s_scene.reset(s_renderer);
  s_menu.begin(s_renderer);
  settings::set_listener(on_setting_changed, nullptr);
  s_renderer.present();
  board::set_backlight(255);

  ESP_ERROR_CHECK(touch::init());
  s_taps = xQueueCreate(8, sizeof(touch::RawSample));
  xTaskCreatePinnedToCore(input_task, "input", 3072, nullptr, 5, nullptr, 1);

  int frames = 0;
  int chunks = 0;
  int64_t next_log_us = esp_timer_get_time() + 5000 * 1000;
  TickType_t wake = xTaskGetTickCount();
  for (;;)
  {
    touch::RawSample s;
    while (xQueueReceive(s_taps, &s, 0) == pdTRUE)
    {
      const touch::Point p = touch::to_screen(s, display::orientation());
      ESP_LOGI(kTag, "tap raw=(%u,%u) screen=(%d,%d)", s.x, s.y, p.x, p.y);
      if (!s_menu.tap(p.x, p.y, now_ms()))
        s_scene.move_first_ball(p.x, p.y);
    }
    s_menu.tick(now_ms());

    s_scene.step(s_renderer);
    chunks += s_renderer.present();
    ++frames;

    const int64_t now = esp_timer_get_time();
    if (now >= next_log_us)
    {
      ESP_LOGI(kTag, "%d fps, %d chunks/frame, heap free=%u largest=%u", frames / 5,
               frames ? chunks / frames : 0,
               static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
               static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
      frames = chunks = 0;
      next_log_us = now + 5000 * 1000;
    }
    xTaskDelayUntil(&wake, pdMS_TO_TICKS(kFrameMs));
  }
}
