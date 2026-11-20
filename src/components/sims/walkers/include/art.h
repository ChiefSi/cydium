#pragma once

#include <cstdint>

#include "gfx/canvas.h"
#include "gfx/palette.h"
#include "world.h"

// Original pixel art for Walkers: palette, walker sprites, skill icons, the
// hatch and the goal door. Sprites are text art: one character per pixel.
namespace walkers::art
{

// Foreground and UI palette indices (see also pal:: in terrain.h).
namespace ix
{
constexpr uint8_t kHatchDark = 32;
constexpr uint8_t kHatchLight = 33;
constexpr uint8_t kHatchMetal = 34;
constexpr uint8_t kHatchHole = 35;
constexpr uint8_t kDoorDark = 36;
constexpr uint8_t kDoorLight = 37;
constexpr uint8_t kDoorInside = 38;
constexpr uint8_t kDoorGlow = 39;
constexpr uint8_t kBarBg = 48;
constexpr uint8_t kBarBorder = 49;
constexpr uint8_t kButtonBg = 50;
constexpr uint8_t kButtonBorder = 51;
constexpr uint8_t kButtonActive = 52;
constexpr uint8_t kBarText = 53;
constexpr uint8_t kBarMuted = 54;
constexpr uint8_t kIconFirst = 55;   // 55..63: icon colours
constexpr uint8_t kHairFirst = 160;  // 160..167
constexpr uint8_t kSkin = 168;
constexpr uint8_t kClothes = 170;
constexpr uint8_t kClothesDark = 171;
constexpr uint8_t kUmbrellaRed = 172;
constexpr uint8_t kUmbrellaWhite = 173;
constexpr uint8_t kHandle = 174;
constexpr uint8_t kText = 175;
constexpr uint8_t kPanel = 176;
constexpr uint8_t kAccent = 177;
constexpr uint8_t kRed = 178;
constexpr uint8_t kGreen = 179;
constexpr uint8_t kSparkFirst = 180;  // 180..187: hot to cold
constexpr uint8_t kCursor = 188;
constexpr uint8_t kCursorEdge = 189;
constexpr uint8_t kFlame = 190;  // 190..191
}  // namespace ix

void setup_palette(gfx::Palette& p);

constexpr int kSpriteW = 8;
constexpr int kSpriteH = 10;
constexpr int kIconSize = 16;

// Draws a walker with its feet at (x, y). Returns the area drawn.
gfx::Rect draw_walker(gfx::Canvas& c, const Walker& w, uint32_t tick);

// Draws a skill icon with its top-left corner at (x, y).
void draw_icon(gfx::Canvas& c, Skill s, int x, int y);

// The hatch, centred on x with its bottom edge on row y.
gfx::Rect hatch_rect(int x, int y);
void draw_hatch(gfx::Canvas& c, int x, int y, bool open);

// The goal door, centred on x and standing on row y (the floor surface).
gfx::Rect door_rect(int x, int y);
void draw_door(gfx::Canvas& c, int x, int y);

// The cursor (an arrow), pointing at (x, y).
gfx::Rect draw_cursor(gfx::Canvas& c, int x, int y);

}  // namespace walkers::art
