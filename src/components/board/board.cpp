#include "board.h"

#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

namespace board
{
namespace
{

constexpr const char* kTag = "board";

constexpr ledc_mode_t kLedcMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kLedcTimer = LEDC_TIMER_0;
constexpr ledc_channel_t kBacklightChannel = LEDC_CHANNEL_0;
constexpr ledc_timer_bit_t kLedcResolution = LEDC_TIMER_8_BIT;
constexpr uint32_t kLedcFreqHz = 5000;

constexpr adc_channel_t kLightChannel = ADC_CHANNEL_6;  // GPIO34

adc_oneshot_unit_handle_t s_adc = nullptr;

esp_err_t init_backlight()
{
  ledc_timer_config_t timer = {};
  timer.speed_mode = kLedcMode;
  timer.duty_resolution = kLedcResolution;
  timer.timer_num = kLedcTimer;
  timer.freq_hz = kLedcFreqHz;
  timer.clk_cfg = LEDC_AUTO_CLK;
  esp_err_t err = ledc_timer_config(&timer);
  if (err != ESP_OK)
    return err;

  ledc_channel_config_t ch = {};
  ch.gpio_num = pins::kBacklight;
  ch.speed_mode = kLedcMode;
  ch.channel = kBacklightChannel;
  ch.timer_sel = kLedcTimer;
  ch.duty = 0;
  ch.hpoint = 0;
  return ledc_channel_config(&ch);
}

esp_err_t init_leds_off()
{
  gpio_config_t cfg = {};
  cfg.pin_bit_mask = (1ULL << pins::kLedR) | (1ULL << pins::kLedG) | (1ULL << pins::kLedB);
  cfg.mode = GPIO_MODE_OUTPUT;
  esp_err_t err = gpio_config(&cfg);
  if (err != ESP_OK)
    return err;
  // Active low: drive high to turn off.
  gpio_set_level(pins::kLedR, 1);
  gpio_set_level(pins::kLedG, 1);
  gpio_set_level(pins::kLedB, 1);
  return ESP_OK;
}

esp_err_t init_button()
{
  gpio_config_t cfg = {};
  cfg.pin_bit_mask = 1ULL << pins::kBootButton;
  cfg.mode = GPIO_MODE_INPUT;
  cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  return gpio_config(&cfg);
}

esp_err_t init_light_sensor()
{
  adc_oneshot_unit_init_cfg_t unit = {};
  unit.unit_id = ADC_UNIT_1;
  esp_err_t err = adc_oneshot_new_unit(&unit, &s_adc);
  if (err != ESP_OK)
    return err;

  adc_oneshot_chan_cfg_t ch = {};
  ch.atten = ADC_ATTEN_DB_0;  // the LDR divider only uses the low end of the range
  ch.bitwidth = ADC_BITWIDTH_DEFAULT;
  return adc_oneshot_config_channel(s_adc, kLightChannel, &ch);
}

}  // namespace

esp_err_t init()
{
  esp_err_t err;
  if ((err = init_backlight()) != ESP_OK)
  {
    ESP_LOGE(kTag, "backlight init failed: %s", esp_err_to_name(err));
    return err;
  }
  if ((err = init_leds_off()) != ESP_OK)
  {
    ESP_LOGE(kTag, "LED init failed: %s", esp_err_to_name(err));
    return err;
  }
  if ((err = init_button()) != ESP_OK)
  {
    ESP_LOGE(kTag, "button init failed: %s", esp_err_to_name(err));
    return err;
  }
  if ((err = init_light_sensor()) != ESP_OK)
  {
    // Not fatal: everything else still works without the light sensor.
    ESP_LOGW(kTag, "light sensor init failed: %s", esp_err_to_name(err));
  }
  return ESP_OK;
}

void set_backlight(uint8_t level)
{
  ledc_set_duty(kLedcMode, kBacklightChannel, level);
  ledc_update_duty(kLedcMode, kBacklightChannel);
}

int read_light_raw()
{
  if (!s_adc)
    return -1;
  int raw = 0;
  if (adc_oneshot_read(s_adc, kLightChannel, &raw) != ESP_OK)
    return -1;
  return raw;
}

bool boot_button_down()
{
  return gpio_get_level(pins::kBootButton) == 0;
}

}  // namespace board
