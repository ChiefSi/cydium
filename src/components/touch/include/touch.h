#pragma once

#include <cstdint>

#include "esp_err.h"

// Bit-banged XPT2046 resistive touch controller. Both SPI hosts are taken by
// the display and SD card, and touch only needs a few samples per frame.
namespace touch
{

struct RawSample
{
  uint16_t x;  // 12-bit ADC value, uncalibrated
  uint16_t y;  // 12-bit ADC value, uncalibrated
  uint16_t z;  // pressure estimate; higher = firmer press
};

esp_err_t init();

// True while the pen IRQ line is low (screen touched). Cheap; no SPI traffic.
bool irq_active();

// Samples the controller (several readings averaged). Returns false if the
// pressure is below the threshold, i.e. nothing is touching the screen.
bool read_raw(RawSample* out);

}  // namespace touch
