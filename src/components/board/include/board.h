#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "esp_err.h"

// Pin map for the ESP32-2432S028R ("Cheap Yellow Display").
namespace board
{

namespace pins
{
// TFT (ILI9341) on SPI2_HOST
constexpr gpio_num_t kTftMiso = GPIO_NUM_12;
constexpr gpio_num_t kTftMosi = GPIO_NUM_13;
constexpr gpio_num_t kTftSclk = GPIO_NUM_14;
constexpr gpio_num_t kTftCs = GPIO_NUM_15;
constexpr gpio_num_t kTftDc = GPIO_NUM_2;
constexpr gpio_num_t kTftRst = GPIO_NUM_NC;
constexpr gpio_num_t kBacklight = GPIO_NUM_21;

// microSD on SPI3_HOST
constexpr gpio_num_t kSdCs = GPIO_NUM_5;
constexpr gpio_num_t kSdSck = GPIO_NUM_18;
constexpr gpio_num_t kSdMiso = GPIO_NUM_19;
constexpr gpio_num_t kSdMosi = GPIO_NUM_23;

// XPT2046 touch (bit-banged)
constexpr gpio_num_t kTouchClk = GPIO_NUM_25;
constexpr gpio_num_t kTouchMosi = GPIO_NUM_32;
constexpr gpio_num_t kTouchMiso = GPIO_NUM_39;
constexpr gpio_num_t kTouchCs = GPIO_NUM_33;
constexpr gpio_num_t kTouchIrq = GPIO_NUM_36;

// Misc
constexpr gpio_num_t kLightSensor = GPIO_NUM_34;  // ADC1 channel 6
constexpr gpio_num_t kLedR = GPIO_NUM_4;          // active low
constexpr gpio_num_t kLedG = GPIO_NUM_16;         // active low
constexpr gpio_num_t kLedB = GPIO_NUM_17;         // active low
constexpr gpio_num_t kBootButton = GPIO_NUM_0;    // active low
constexpr gpio_num_t kSpeaker = GPIO_NUM_26;
}  // namespace pins

// Initialises backlight PWM (off), BOOT button input, light sensor ADC, and
// turns the RGB LED off.
esp_err_t init();

// 0 = off, 255 = full brightness.
void set_backlight(uint8_t level);

// Raw 12-bit light sensor reading. Note: LOWER values mean a BRIGHTER room.
// Returns -1 on error.
int read_light_raw();

// Raw (undebounced) BOOT button state.
bool boot_button_down();

}  // namespace board
