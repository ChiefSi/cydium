#include "settings.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

namespace settings
{
namespace
{

constexpr const char* kTag = "settings";
constexpr const char* kNamespace = "cydium";

constexpr const char* kOrientationOptions[] = {"Portrait", "Landscape"};
constexpr const char* kDifficultyOptions[] = {"Easy", "Medium", "Hard"};
constexpr const char* kCornerOptions[] = {"Top left", "Top right", "Bottom left", "Bottom right"};

#ifdef CONFIG_CYD_LCD_INVERT_DEFAULT
constexpr uint8_t kInvertDefault = 1;
#else
constexpr uint8_t kInvertDefault = 0;
#endif

#ifdef CONFIG_CYD_DISPLAY_DEFAULT_LANDSCAPE
constexpr uint8_t kOrientationDefault = kLandscape;
#else
constexpr uint8_t kOrientationDefault = kPortrait;
#endif

#if defined(CONFIG_CYD_MENU_ICON_TOP_RIGHT)
constexpr uint8_t kCornerDefault = kTopRight;
#elif defined(CONFIG_CYD_MENU_ICON_BOTTOM_LEFT)
constexpr uint8_t kCornerDefault = kBottomLeft;
#elif defined(CONFIG_CYD_MENU_ICON_BOTTOM_RIGHT)
constexpr uint8_t kCornerDefault = kBottomRight;
#else
constexpr uint8_t kCornerDefault = kTopLeft;
#endif

// Indexed by Id.
constexpr Info kInfo[kCount] = {
    {"disp.invert", "Display", "Invert colours", Type::kBool, nullptr, 2, kInvertDefault},
    {"disp.orient", "Display", "Orientation", Type::kEnum, kOrientationOptions, 2,
     kOrientationDefault},
    {"ui.icon_pos", "Display", "Menu icon", Type::kEnum, kCornerOptions, 4, kCornerDefault},
    // Chosen on the menu's Simulation page; an index past the registered
    // simulations falls back to the first one.
    {"sim.active", nullptr, "Simulation", Type::kEnum, nullptr, 255, 0},
    {"walk.level", "Walkers", "Difficulty", Type::kEnum, kDifficultyOptions, 3, kMedium},
};

uint8_t s_values[kCount] = {};
nvs_handle_t s_nvs = 0;
bool s_nvs_ok = false;
Listener s_listener = nullptr;
void* s_listener_ctx = nullptr;

int index(Id id)
{
  return static_cast<int>(id);
}

}  // namespace

esp_err_t init()
{
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_LOGW(kTag, "NVS needs erasing (%s)", esp_err_to_name(err));
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  if (err == ESP_OK)
    err = nvs_open(kNamespace, NVS_READWRITE, &s_nvs);
  s_nvs_ok = err == ESP_OK;
  if (!s_nvs_ok)
    ESP_LOGW(kTag, "NVS unavailable (%s); using defaults, changes won't be saved",
             esp_err_to_name(err));

  for (int i = 0; i < kCount; ++i)
  {
    uint8_t v = kInfo[i].default_value;
    if (s_nvs_ok && nvs_get_u8(s_nvs, kInfo[i].key, &v) == ESP_OK && v >= kInfo[i].option_count)
      v = kInfo[i].default_value;
    s_values[i] = v;
    ESP_LOGI(kTag, "%s = %u", kInfo[i].key, v);
  }
  return err;
}

const Info& info(Id id)
{
  return kInfo[index(id)];
}

uint8_t get(Id id)
{
  return s_values[index(id)];
}

void set(Id id, uint8_t value)
{
  const int i = index(id);
  if (value >= kInfo[i].option_count)
    value = kInfo[i].option_count - 1;
  if (s_values[i] == value)
    return;
  s_values[i] = value;
  if (s_nvs_ok)
  {
    esp_err_t err = nvs_set_u8(s_nvs, kInfo[i].key, value);
    if (err == ESP_OK)
      err = nvs_commit(s_nvs);
    if (err != ESP_OK)
      ESP_LOGW(kTag, "saving %s failed: %s", kInfo[i].key, esp_err_to_name(err));
  }
  ESP_LOGI(kTag, "%s -> %u", kInfo[i].key, value);
  if (s_listener)
    s_listener(id, value, s_listener_ctx);
}

void set_listener(Listener listener, void* ctx)
{
  s_listener = listener;
  s_listener_ctx = ctx;
}

}  // namespace settings
