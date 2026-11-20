// Emulator touch: turns the mouse position into the raw readings the XPT2046
// would give there, so the normal calibration and rotation path maps it back.
#include "emulator.h"
#include "touch.h"

namespace touch
{

esp_err_t init()
{
  return ESP_OK;
}

bool irq_active()
{
  return emulator::touch().down;
}

bool read_raw(RawSample* out)
{
  const emulator::Touch t = emulator::touch();
  if (!t.down)
    return false;
  const Point portrait =
      rotate_to_portrait({static_cast<int16_t>(t.x), static_cast<int16_t>(t.y)}, t.orientation);
  const RawPoint raw = map_from_portrait(portrait, calibration());
  *out = {raw.x, raw.y, 1000};
  return true;
}

}  // namespace touch
