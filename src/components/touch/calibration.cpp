// Kconfig calibration, shared by the XPT2046 and emulator builds.
#include "sdkconfig.h"
#include "touch.h"

namespace touch
{
namespace
{

constexpr Calibration kCalibration = {
    CONFIG_CYD_TOUCH_X_MIN,
    CONFIG_CYD_TOUCH_X_MAX,
    CONFIG_CYD_TOUCH_Y_MIN,
    CONFIG_CYD_TOUCH_Y_MAX,
#ifdef CONFIG_CYD_TOUCH_SWAP_XY
    true,
#else
    false,
#endif
#ifdef CONFIG_CYD_TOUCH_FLIP_X
    true,
#else
    false,
#endif
#ifdef CONFIG_CYD_TOUCH_FLIP_Y
    true,
#else
    false,
#endif
};

}  // namespace

const Calibration& calibration()
{
  return kCalibration;
}

}  // namespace touch
