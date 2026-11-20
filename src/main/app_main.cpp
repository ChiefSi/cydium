// cydium: runs the selected simulation with the menu on top.
//
// The menu icon or the BOOT button opens the Simulation, Settings and About
// pages; taps elsewhere go to the simulation. Frame rate and heap are logged every 5 seconds.

#include <cinttypes>

#include "board.h"
#include "bounce.h"
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
#include "sim.h"
#include "starfield.h"
#include "touch.h"

namespace
{

constexpr const char* kTag = "app";
constexpr int kFrameMs = 33;  // ~30 fps target
// Input runs on the app core (1), next to rendering; the emulator has one core.
constexpr BaseType_t kInputCore = portNUM_PROCESSORS > 1 ? 1 : 0;

display::Renderer s_renderer;
ui::Menu s_menu;
// Input events, from the input task to the render loop.
struct InputEvent
{
  enum class Kind : uint8_t
  {
    kTap,   // touch: a new press, raw sample in tap
    kBoot,  // BOOT button pressed
  };
  Kind kind;
  touch::RawSample tap;
};
QueueHandle_t s_input = nullptr;

// Lets simulations draw through the renderer.
class RendererSurface : public sim::Surface
{
 public:
  explicit RendererSurface(display::Renderer& r) : r_(r) {}
  gfx::IndexedLayer& bg() override
  {
    return r_.bg();
  }
  gfx::IndexedLayer& fg() override
  {
    return r_.fg();
  }
  gfx::Palette& palette() override
  {
    return r_.palette();
  }
  void invalidate(const gfx::Rect& r) override
  {
    r_.invalidate(r);
  }
  void invalidate_all() override
  {
    r_.invalidate_all();
  }

 private:
  display::Renderer& r_;
};

RendererSurface s_surface(s_renderer);
sim::Manager s_sims(s_surface);

// The registered simulations, in menu order.
bounce::Bounce s_bounce;
starfield::Starfield s_starfield;

void select_simulation(int index)
{
  if (!s_sims.select(index))
    ESP_LOGE(kTag, "simulation %d failed to start", index);
  else
    ESP_LOGI(kTag, "simulation: %s", s_sims.at(s_sims.active()).name());
}

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
  s_sims.restart();
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
    case settings::Id::kSimulation:
      select_simulation(value);
      break;
    default:
      break;
  }
}

constexpr int kInputPollMs = 20;
constexpr int kBootDebounce = 2;  // samples the BOOT button must hold steady

// Samples touch and the BOOT button. Queues one tap per touch press (a new
// press needs a clear release of at least 100 ms first) and one event per
// debounced BOOT press.
void input_task(void*)
{
  bool down = false;
  int up_ms = 0;
  bool boot_down = false;
  int boot_steady = 0;
  for (;;)
  {
    InputEvent e{InputEvent::Kind::kTap, {}};
    if (touch::irq_active() && touch::read_raw(&e.tap))
    {
      if (!down)
        xQueueSend(s_input, &e, 0);
      down = true;
      up_ms = 0;
    }
    else if (down && (up_ms += kInputPollMs) >= 100)
    {
      down = false;
    }

    const bool boot = board::boot_button_down();
    boot_steady = boot == boot_down ? 0 : boot_steady + 1;
    if (boot_steady >= kBootDebounce)
    {
      boot_down = boot;
      boot_steady = 0;
      if (boot_down)
      {
        e.kind = InputEvent::Kind::kBoot;
        xQueueSend(s_input, &e, 0);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(kInputPollMs));
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
  s_sims.seed(esp_random());
  s_sims.add(s_bounce);
  s_sims.add(s_starfield);
  select_simulation(settings::get(settings::Id::kSimulation));
  s_menu.begin(s_renderer, s_sims);
  settings::set_listener(on_setting_changed, nullptr);
  s_renderer.present();
  board::set_backlight(255);

  ESP_ERROR_CHECK(touch::init());
  s_input = xQueueCreate(8, sizeof(InputEvent));
  xTaskCreatePinnedToCore(input_task, "input", 3072, nullptr, 5, nullptr, kInputCore);

  int frames = 0;
  int chunks = 0;
  int64_t next_log_us = esp_timer_get_time() + 5000 * 1000;
  TickType_t wake = xTaskGetTickCount();
  uint32_t last_ms = now_ms();
  for (;;)
  {
    InputEvent e;
    while (xQueueReceive(s_input, &e, 0) == pdTRUE)
    {
      if (e.kind == InputEvent::Kind::kBoot)
      {
        ESP_LOGI(kTag, "BOOT button");
        s_menu.toggle(now_ms());
        continue;
      }
      const touch::RawSample& s = e.tap;
      const touch::Point p = touch::to_screen(s, display::orientation());
      ESP_LOGI(kTag, "tap raw=(%u,%u) screen=(%d,%d)", s.x, s.y, p.x, p.y);
      if (!s_menu.tap(p.x, p.y, now_ms()))
        s_sims.tap(p.x, p.y);
    }
    const uint32_t ms = now_ms();
    s_menu.tick(ms);

    s_sims.step(ms - last_ms);
    last_ms = ms;
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
