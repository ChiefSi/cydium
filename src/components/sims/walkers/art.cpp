#include "art.h"

#include <algorithm>
#include <cstring>

#include "terrain.h"

namespace walkers::art
{
namespace
{

// Walker frames, facing right, 8 x 10. H hair, S skin, B clothes, D dark
// clothes, G tool grey.
const char* const kWalk[4][kSpriteH] = {
    {"...HHH..", "..HHHHH.", "...HSS..", "....SS..", "...BBB..", "...BBBS.", "...BBB..", "...DDD..",
     "..D..D..", ".D....D."},
    {"...HHH..", "..HHHHH.", "...HSS..", "....SS..", "...BBB..", "...BBB..", "..SBBB..", "...DDD..",
     "...D.D..", "...D.D.."},
    {"...HHH..", "..HHHHH.", "...HSS..", "....SS..", "...BBB..", "..SBBB..", "...BBB..", "...DDD..",
     "...D.D..", "..D...D."},
    {"...HHH..", "..HHHHH.", "...HSS..", "....SS..", "...BBB..", "...BBB..", "...BBBS.", "...DDD..",
     "....D...", "....D..."},
};
const char* const kFall[kSpriteH] = {"..S...S.", "..SHHHS.", "..HHHHH.", "...HSS..", "....SS..",
                                     "...BBB..", "...BBB..", "...DDD..", "...D.D..", "..D...D."};
const char* const kDig[2][kSpriteH] = {
    {"........", "...HHH..", "..HHHHH.", "...HSS..", "...BBBS.", "..BBBB.S", "..BBBB.G", "...DDD.G",
     "...D.D.G", "...D.D.G"},
    {"........", "........", "...HHH..", "..HHHHH.", "...HSSS.", "..BBBB.S", "..BBBB.G", "...DDD.G",
     "...D.D.G", "..D...DG"},
};
const char* const kBuild[2][kSpriteH] = {
    {"...HHH..", "..HHHHH.", "...HSS..", "....SS..", "...BBB..", "...BBBSS", "...BBB..", "...DDD..",
     "...D.D..", "...D.D.."},
    {"........", "...HHH..", "..HHHHH.", "...HSS..", "....SS..", "...BBB..", "...BBB.S", "...DDDSS",
     "..D..D..", "..D..D.."},
};
const char* const kBlock[kSpriteH] = {"...HHH..", "..HHHHH.", "...SSS..", "....S...", "SBBBBBBS",
                                      "...BBB..", "...BBB..", "...DDD..", "..D...D.", ".D.....D"};
const char* const kShrug[kSpriteH] = {"........", "...HHH..", "..HHHHH.", ".S.HSS.S", "..S.SS.S",
                                      "...BBB..", "...BBB..", "...DDD..", "...D.D..", "...D.D.."};
const char* const kSplat[3][kSpriteH] = {
    {"........", "........", "........", "...HHH..", "..HHHHH.", "...SSS..", "..BBBBB.", "..BBBBB.",
     "..DDDDD.", ".D.....D"},
    {"........", "........", "........", "........", "........", "..HHHHH.", ".HHSSSHH", ".BBBBBBB",
     "DBBBBBBD", "DDDDDDDD"},
    {"........", "........", "........", "........", "........", "........", "........", "........",
     ".H.HHH.H", "BDBDBDBD"},
};
const char* const kUmbrella[5] = {"..RRRRR..", ".RWRWRWR.", "RRRRRRRRR", "....G....", "....G...."};

// Skill icons, 16 x 16. b brown, g grey, k black, r red, w white, y yellow,
// u blue, n green, s skin.
const char* const kIcons[kSkillCount][kIconSize] = {
    // Digger: a spade cutting into the ground.
    {"......gg........", ".....gggg.......", "......gg........", "......bb........",
     "......bb........", "......bb........", "......bb........", "......bb........",
     ".....gggg.......", ".....gggg.......", "bbbbb.gg.bbbbbbb", "bbbbbb..bbbbbbbb",
     "bbbbbbbbbbbbbbbb", "bbkbbbbbbbbkbbbb", "bbbbbbbkbbbbbbbb", "bbbbbbbbbbbbbbbb"},
    // Builder: a staircase of bricks.
    {"................", "................", "................", "................",
     "............yyyy", "............bbbb", "........yyyy....", "........bbbb....",
     "....yyyy........", "....bbbb........", "yyyy............", "bbbb............",
     "................", "nnnnnnnnnnnnnnnn", "bbbbbbbbbbbbbbbb", "bbbbbbbbbbbbbbbb"},
    // Exploder: a bomb with a lit fuse.
    {"..........y.y...", "...........r....", "..........yry...", ".........g......",
     "........g.......", ".....kkkkk......", "....kkkkkkk.....", "...kkwkkkkkk....",
     "...kwkkkkkkk....", "...kkkkkkkkk....", "...kkkkkkkkk....", "...kkkkkkkkk....",
     "....kkkkkkk.....", ".....kkkkk......", "................", "................"},
    // Blocker: a figure with arms out.
    {"................", "......nnnn......", ".....nnnnnn.....", "......ssss......",
     ".......ss.......", "suuuuuuuuuuuuuus", ".....uuuuuu.....", ".....uuuuuu.....",
     ".....uuuuuu.....", ".....kkkkkk.....", ".....kk..kk.....", "....kk....kk....",
     "....kk....kk....", "................", "rrrrrrrrrrrrrrrr", "................"},
    // Umbrella.
    {"................", ".....rrrrrr.....", "...rrwwrrwwrr...", "..rrrwwrrwwrrr..",
     ".rrrrwwrrwwrrrr.", "rrrrrwwrrwwrrrrr", "rr..r..rr..r..rr", ".......gg.......",
     ".......gg.......", ".......gg.......", ".......gg.......", ".......gg.......",
     ".......gg.......", "....gg.gg.......", ".....ggg........", "................"},
};

// The hatch, 28 x 12: a wooden box with trapdoors underneath.
const char* const kHatch[12] = {
    "MMMMMMMMMMMMMMMMMMMMMMMMMMMM", "MLLLLLLLLLLLLLLLLLLLLLLLLLLM", "MLDDDDDDDDDDDDDDDDDDDDDDDDLM",
    "MLDLLLLLLLLLLLLLLLLLLLLLLDLM", "MLDLDDDDDDDDDDDDDDDDDDDDLDLM", "MLDLDKKKKKKKKKKKKKKKKKKDLDLM",
    "MLDLDKKKKKKKKKKKKKKKKKKDLDLM", "MLDLDKKKKKKKKKKKKKKKKKKDLDLM", "MLDLDKKKKKKKKKKKKKKKKKKDLDLM",
    "MLDDDDDDDDDDDDDDDDDDDDDDDDLM", "MLLLLLLLLLLLLLLLLLLLLLLLLLLM", "MMMMMMMMMMMMMMMMMMMMMMMMMMMM",
};

// The door, 16 x 20: a stone arch with a dark doorway.
const char* const kDoor[20] = {
    "......LLLL......", "....LLDDDDLL....", "...LDDDDDDDDL...", "..LDDLLLLLLDDL..",
    "..LDLKKKKKKLDL..", ".LDLKKKKKKKKLDL.", ".LDLKKKKKKKKLDL.", ".LDLKKKKKKKKLDL.",
    ".LDLKKKKKKKKLDL.", ".LDLKKKKKKKKLDL.", ".LDLKKKKKKKKLDL.", ".LDLKKKKKKKKLDL.",
    ".LDLKKKKKKKKLDL.", ".LDLKKKKKKKKLDL.", ".LDLKKKKKKKKLDL.", ".LDLKKKKKKKKLDL.",
    ".LDLKKKKKKKKLDL.", "LLDLKKKKKKKKLDLL", "LDDLKKKKKKKKLDDL", "LLLLKKKKKKKKLLLL",
};

const char* const kCursor[7] = {"X......", "XX.....", "XWX....", "XWWX...",
                                "XWWWX..", "XWXX...", "XX.X..."};

uint8_t walker_index(char ch, uint8_t hair)
{
  switch (ch)
  {
    case 'H':
      return static_cast<uint8_t>(ix::kHairFirst + hair);
    case 'S':
      return ix::kSkin;
    case 'B':
      return ix::kClothes;
    case 'D':
      return ix::kClothesDark;
    case 'G':
    case 'g':
      return ix::kHandle;
    case 'R':
      return ix::kUmbrellaRed;
    case 'W':
      return ix::kUmbrellaWhite;
    default:
      return 0;
  }
}

uint8_t icon_index(char ch)
{
  const char* map = "bgkrwyuns";
  const char* p = std::strchr(map, ch);
  return p && ch ? static_cast<uint8_t>(ix::kIconFirst + (p - map)) : 0;
}

uint8_t scenery_index(char ch, uint8_t dark, uint8_t light, uint8_t metal, uint8_t hole)
{
  switch (ch)
  {
    case 'D':
      return dark;
    case 'L':
      return light;
    case 'M':
      return metal;
    case 'K':
      return hole;
    default:
      return 0;
  }
}

// Draws text art; `map` turns a character into a palette index (0 = skip).
template <typename Map>
void draw_art(gfx::Canvas& c, const char* const* rows, int w, int h, int x, int y, bool flip,
              Map map)
{
  for (int r = 0; r < h; ++r)
    for (int col = 0; col < w; ++col)
    {
      const uint8_t v = map(rows[r][flip ? w - 1 - col : col]);
      if (v)
        c.pixel(x + col, y + r, v);
    }
}

void rgb(gfx::Palette& p, uint8_t i, int r, int g, int b)
{
  auto clamp = [](int v) { return static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v); };
  p.set(i, clamp(r), clamp(g), clamp(b));
}

}  // namespace

void setup_palette(gfx::Palette& p)
{
  for (int i = 0; i < pal::kSkyCount; ++i)  // night sky, darkest at the bottom
    rgb(p, pal::kSkyFirst + i, 10 - i / 2, 12 - i / 2, 40 - i * 2);
  // Dirt: 5 shades from the crust down, 8 grains each.
  const int base[5][3] = {
      {196, 128, 72}, {170, 104, 58}, {150, 90, 50}, {132, 78, 44}, {116, 68, 40}};
  for (int s = 0; s < 5; ++s)
    for (int g = 0; g < 8; ++g)
    {
      const int d = (g * 37 % 9) - 4;  // grain: -4 .. +4
      rgb(p, pal::kDirtFirst + s * 8 + g, base[s][0] + d * 4, base[s][1] + d * 3,
          base[s][2] + d * 2);
    }
  for (int i = 0; i < pal::kBrickCount; ++i)
    rgb(p, pal::kBrickFirst + i, 214 - (i & 1) * 26 - (i >= 2) * 10, 170 - (i & 1) * 22,
        100 - (i & 1) * 16);
  rgb(p, pal::kMetalFirst + 0, 64, 70, 86);     // shadow edge
  rgb(p, pal::kMetalFirst + 1, 196, 202, 216);  // lit edge
  rgb(p, pal::kMetalFirst + 2, 236, 236, 244);  // rivet
  rgb(p, pal::kMetalFirst + 4, 134, 140, 156);  // face
  rgb(p, pal::kMetalFirst + 5, 124, 130, 146);

  rgb(p, ix::kHatchDark, 96, 60, 28);
  rgb(p, ix::kHatchLight, 150, 100, 52);
  rgb(p, ix::kHatchMetal, 150, 152, 164);
  rgb(p, ix::kHatchHole, 6, 6, 10);
  rgb(p, ix::kDoorDark, 92, 92, 104);
  rgb(p, ix::kDoorLight, 150, 150, 164);
  rgb(p, ix::kDoorInside, 24, 12, 4);
  rgb(p, ix::kDoorGlow, 255, 200, 80);
  rgb(p, ix::kBarBg, 20, 24, 36);
  rgb(p, ix::kBarBorder, 110, 130, 170);
  rgb(p, ix::kButtonBg, 36, 44, 64);
  rgb(p, ix::kButtonBorder, 80, 90, 120);
  rgb(p, ix::kButtonActive, 255, 196, 64);
  rgb(p, ix::kBarText, 236, 236, 244);
  rgb(p, ix::kBarMuted, 150, 160, 180);
  const int icon[9][3] = {{150, 92, 44},  {168, 168, 180}, {28, 28, 32},
                          {222, 52, 40},  {244, 244, 244}, {250, 212, 64},
                          {64, 112, 232}, {80, 200, 92},   {240, 190, 150}};
  for (int i = 0; i < 9; ++i)
    rgb(p, ix::kIconFirst + i, icon[i][0], icon[i][1], icon[i][2]);

  const int hair[kHairColours][3] = {{64, 210, 64},   {244, 142, 40}, {244, 222, 64},
                                     {226, 54, 54},   {176, 84, 228}, {64, 204, 226},
                                     {244, 124, 184}, {232, 232, 232}};
  for (int i = 0; i < kHairColours; ++i)
    rgb(p, ix::kHairFirst + i, hair[i][0], hair[i][1], hair[i][2]);
  rgb(p, ix::kSkin, 244, 194, 152);
  rgb(p, ix::kSkin + 1, 204, 144, 112);
  rgb(p, ix::kClothes, 64, 96, 228);
  rgb(p, ix::kClothesDark, 34, 52, 146);
  rgb(p, ix::kUmbrellaRed, 226, 50, 56);
  rgb(p, ix::kUmbrellaWhite, 244, 244, 244);
  rgb(p, ix::kHandle, 140, 140, 150);
  rgb(p, ix::kText, 244, 244, 250);
  rgb(p, ix::kPanel, 14, 14, 24);
  rgb(p, ix::kAccent, 255, 196, 64);
  rgb(p, ix::kRed, 236, 72, 64);
  rgb(p, ix::kGreen, 96, 220, 110);
  const int spark[8][3] = {{255, 255, 190}, {255, 232, 96}, {255, 184, 44}, {242, 124, 32},
                           {212, 64, 24},   {150, 44, 24},  {96, 92, 92},   {64, 62, 64}};
  for (int i = 0; i < 8; ++i)
    rgb(p, ix::kSparkFirst + i, spark[i][0], spark[i][1], spark[i][2]);
  rgb(p, ix::kCursor, 250, 250, 250);
  rgb(p, ix::kCursorEdge, 10, 10, 10);
  rgb(p, ix::kFlame, 255, 168, 40);
  rgb(p, ix::kFlame + 1, 255, 236, 120);
}

gfx::Rect draw_walker(gfx::Canvas& c, const Walker& w, uint32_t tick)
{
  const char* const* frame = kWalk[(w.x & 0x7FFF) / 2 % 4];
  switch (w.state)
  {
    case State::kFalling:
      frame = w.umbrella && w.y - w.fall_from >= kUmbrellaOpen ? kFall : kWalk[1];
      break;
    case State::kDigging:
      frame = kDig[(tick / 2) & 1];
      break;
    case State::kBuilding:
      frame = kBuild[(tick / 2) & 1];
      break;
    case State::kShrugging:
    case State::kExiting:
      frame = kShrug;
      break;
    case State::kBlocking:
      frame = kBlock;
      break;
    case State::kSplatting:
      frame = kSplat[std::min(2, w.timer / 4)];
      break;
    default:
      break;
  }
  const int x0 = w.x - kSpriteW / 2;
  const int y0 = w.y - kSpriteH + 1;
  const bool flip = w.dir < 0;
  if (w.state == State::kExiting)  // sinks into the doorway
  {
    const int rows = kSpriteH - std::min(kSpriteH, static_cast<int>(w.timer));
    draw_art(c, frame, kSpriteW, rows, x0, y0 + kSpriteH - rows, flip,
             [&](char ch) { return walker_index(ch, w.hair); });
  }
  else
  {
    draw_art(c, frame, kSpriteW, kSpriteH, x0, y0, flip,
             [&](char ch) { return walker_index(ch, w.hair); });
  }
  gfx::Rect r{x0, y0, kSpriteW, kSpriteH};
  if (w.state == State::kFalling && w.umbrella && w.y - w.fall_from >= kUmbrellaOpen)
  {
    draw_art(c, kUmbrella, 9, 5, w.x - 4, y0 - 5, false,
             [&](char ch) { return walker_index(ch, w.hair); });
    r = {x0 - 1, y0 - 5, kSpriteW + 2, kSpriteH + 5};
  }
  if (w.fuse > 0 && !w.doomed())
  {
    const char digit[2] = {static_cast<char>('1' + (w.fuse - 1) / kTicksPerSecond), 0};
    c.text(w.x - 3, y0 - 9, digit, ix::kText);
    r = {r.x - 1, y0 - 9, r.w + 2, r.bottom() - (y0 - 9)};
  }
  return r;
}

void draw_icon(gfx::Canvas& c, Skill s, int x, int y)
{
  draw_art(c, kIcons[static_cast<int>(s)], kIconSize, kIconSize, x, y, false, icon_index);
}

gfx::Rect hatch_rect(int x, int y)
{
  return {x - 14, y - 12, 28, 12};
}

void draw_hatch(gfx::Canvas& c, int x, int y, bool open)
{
  const gfx::Rect r = hatch_rect(x, y);
  draw_art(c, kHatch, 28, 12, r.x, r.y, false,
           [](char ch)
           {
             return scenery_index(ch, ix::kHatchDark, ix::kHatchLight, ix::kHatchMetal,
                                  ix::kHatchHole);
           });
  // Trapdoors: closed across the bottom, or hanging open.
  if (open)
  {
    c.vline(r.x + 5, y, 6, ix::kHatchLight);
    c.vline(r.right() - 6, y, 6, ix::kHatchLight);
  }
  else
  {
    c.hline(r.x + 5, y - 1, 18, ix::kHatchLight);
  }
}

gfx::Rect door_rect(int x, int y)
{
  return {x - 8, y - 20, 16, 20};
}

void draw_door(gfx::Canvas& c, int x, int y)
{
  const gfx::Rect r = door_rect(x, y);
  draw_art(c, kDoor, 16, 20, r.x, r.y, false, [](char ch)
           { return scenery_index(ch, ix::kDoorDark, ix::kDoorLight, 0, ix::kDoorInside); });
}

gfx::Rect draw_cursor(gfx::Canvas& c, int x, int y)
{
  draw_art(c, kCursor, 7, 7, x, y, false, [](char ch)
           { return ch == 'X'   ? ix::kCursorEdge
                    : ch == 'W' ? ix::kCursor
                                : uint8_t{0}; });
  return {x, y, 7, 7};
}

}  // namespace walkers::art
