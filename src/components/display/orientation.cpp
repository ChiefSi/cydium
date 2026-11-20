// Kconfig orientation choices, shared by the panel and emulator builds.
#include "display.h"
#include "sdkconfig.h"

namespace display
{

gfx::Orientation landscape_orientation()
{
#ifdef CONFIG_CYD_DISPLAY_LANDSCAPE_USB_LEFT
  return gfx::Orientation::kLandscapeCw;
#else
  return gfx::Orientation::kLandscapeCcw;
#endif
}

gfx::Orientation default_orientation()
{
#ifdef CONFIG_CYD_DISPLAY_DEFAULT_LANDSCAPE
  return landscape_orientation();
#else
  return gfx::Orientation::kPortrait;
#endif
}

}  // namespace display
