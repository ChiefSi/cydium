#include "touch.h"

#include "board.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "sdkconfig.h"

namespace touch
{
namespace
{

using namespace board::pins;

// Control bytes: start bit, channel, 12-bit, differential, PD=00 (power down
// between conversions with the pen IRQ enabled).
constexpr uint8_t kCmdX = 0xD0;
constexpr uint8_t kCmdY = 0x90;
constexpr uint8_t kCmdZ1 = 0xB0;
constexpr uint8_t kCmdZ2 = 0xC0;

constexpr int kSamples = 4;
constexpr uint16_t kMinPressure = 300;
constexpr uint32_t kHalfClockUs = 1;  // ~500 kHz; the XPT2046 allows up to 2.5 MHz

inline void clk(int level)
{
  gpio_set_level(kTouchClk, level);
  esp_rom_delay_us(kHalfClockUs);
}

// Sends a control byte and reads back the 12-bit conversion result.
uint16_t transfer(uint8_t cmd)
{
  for (int i = 7; i >= 0; --i)
  {
    gpio_set_level(kTouchMosi, (cmd >> i) & 1);
    clk(1);
    clk(0);
  }
  gpio_set_level(kTouchMosi, 0);
  // The controller shifts data out on falling edges; sample after the rising
  // edge. The first clock is the busy cycle, then 12 data bits, then padding.
  uint16_t v = 0;
  for (int i = 0; i < 16; ++i)
  {
    clk(1);
    v = static_cast<uint16_t>((v << 1) | gpio_get_level(kTouchMiso));
    clk(0);
  }
  return static_cast<uint16_t>((v >> 3) & 0x0FFF);
}

}  // namespace

esp_err_t init()
{
  gpio_config_t out = {};
  out.pin_bit_mask = (1ULL << kTouchClk) | (1ULL << kTouchMosi) | (1ULL << kTouchCs);
  out.mode = GPIO_MODE_OUTPUT;
  esp_err_t err = gpio_config(&out);
  if (err != ESP_OK)
    return err;

  // GPIO36/39 are input-only and have no internal pulls; the XPT2046 pen IRQ
  // has its own pull-up.
  gpio_config_t in = {};
  in.pin_bit_mask = (1ULL << kTouchMiso) | (1ULL << kTouchIrq);
  in.mode = GPIO_MODE_INPUT;
  err = gpio_config(&in);
  if (err != ESP_OK)
    return err;

  gpio_set_level(kTouchCs, 1);
  gpio_set_level(kTouchClk, 0);
  gpio_set_level(kTouchMosi, 0);
  return ESP_OK;
}

bool irq_active()
{
  return gpio_get_level(kTouchIrq) == 0;
}

bool read_raw(RawSample* out)
{
  gpio_set_level(kTouchCs, 0);

  uint16_t z1 = transfer(kCmdZ1);
  uint16_t z2 = transfer(kCmdZ2);
  int z = z1 + 4095 - z2;
  if (z < kMinPressure)
  {
    gpio_set_level(kTouchCs, 1);
    return false;
  }

  transfer(kCmdX);  // discard the first reading; it settles slowly
  uint32_t sx = 0, sy = 0;
  for (int i = 0; i < kSamples; ++i)
  {
    sx += transfer(kCmdX);
    sy += transfer(kCmdY);
  }
  gpio_set_level(kTouchCs, 1);

  out->x = static_cast<uint16_t>(sx / kSamples);
  out->y = static_cast<uint16_t>(sy / kSamples);
  out->z = static_cast<uint16_t>(z);
  return true;
}

}  // namespace touch
