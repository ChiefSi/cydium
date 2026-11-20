#pragma once

#include <cstdint>

#include "esp_err.h"

// User settings: a fixed registry of bool and enum values, with defaults from
// Kconfig, saved in NVS as soon as they change.
namespace settings
{

enum class Type : uint8_t
{
  kBool,  // value 0 or 1, shown as a checkbox
  kEnum,  // value 0 .. option_count - 1, shown as a choice
};

enum class Id : uint8_t
{
  kInvert,        // bool: invert display colours
  kOrientation,   // enum: Orientation
  kIconPosition,  // enum: Corner
  kSimulation,    // index of the active simulation (not in a settings group)
  kCount,
};

// Values of Id::kOrientation.
enum Orientation : uint8_t
{
  kPortrait = 0,
  kLandscape = 1,
};

// Values of Id::kIconPosition.
enum Corner : uint8_t
{
  kTopLeft = 0,
  kTopRight = 1,
  kBottomLeft = 2,
  kBottomRight = 3,
};

struct Info
{
  const char* key;    // NVS key (at most 15 characters)
  const char* group;  // menu group, e.g. "Display"; nullptr hides it from Settings
  const char* label;  // menu label
  Type type;
  const char* const* options;  // enum option labels; nullptr if not shown
  uint8_t option_count;        // 2 for bool
  uint8_t default_value;
};

constexpr int kCount = static_cast<int>(Id::kCount);

// Initialises NVS (erasing it if its format is incompatible) and loads all
// values. Missing or out-of-range values fall back to the default.
esp_err_t init();

const Info& info(Id id);
uint8_t get(Id id);
inline bool get_bool(Id id)
{
  return get(id) != 0;
}

// Stores a value (clamped to the valid range), saves it to NVS and calls the
// listener if it changed.
void set(Id id, uint8_t value);

// Called after a value changes. One listener; set nullptr to remove.
using Listener = void (*)(Id id, uint8_t value, void* ctx);
void set_listener(Listener listener, void* ctx);

}  // namespace settings
