// Emulator board: backlight and BOOT button via the emulator's web page.
#include "board.h"
#include "emulator.h"

namespace board
{

esp_err_t init()
{
  set_backlight(0);
  return ESP_OK;
}

void set_backlight(uint8_t level)
{
  emulator::set_backlight(level);
}

int read_light_raw()
{
  return 2048;  // a mid-lit room
}

bool boot_button_down()
{
  return emulator::boot_button();
}

}  // namespace board
