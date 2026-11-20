#include "menu.h"

#include <cstring>

#include "buildinfo.h"
#include "display.h"
#include "menu_icon.h"
#include "sdkconfig.h"
#include "settings.h"
#include "sim/manager.h"

namespace ui
{
namespace
{

using gfx::Align;
using gfx::Rect;

constexpr int kIconSize = kMenuIconSize;
constexpr int kIconOffset = CONFIG_CYD_MENU_ICON_OFFSET;
constexpr uint32_t kTimeoutMs = CONFIG_CYD_MENU_TIMEOUT_S * 1000u;

// Layout, in pixels.
constexpr int kMargin = 6;    // scene visible around the panel
constexpr int kTitleH = 36;   // title bar; also the Back and Close buttons' size
constexpr int kPad = 8;       // inner padding
constexpr int kNavRowH = 40;  // category rows
constexpr int kSetRowH = 48;  // setting rows: label line + value line
constexpr int kSimRowH = 38;  // simulation rows: name + description
constexpr int kBox = 20;      // checkbox size

constexpr uint16_t kPanelBg = gfx::rgb565_be(20, 24, 36);
constexpr uint16_t kTitleBg = gfx::rgb565_be(36, 44, 64);
constexpr uint16_t kBorder = gfx::rgb565_be(110, 130, 170);
constexpr uint16_t kSeparator = gfx::rgb565_be(52, 60, 84);
constexpr uint16_t kText = gfx::rgb565_be(255, 255, 255);
constexpr uint16_t kMuted = gfx::rgb565_be(150, 160, 180);
constexpr uint16_t kAccent = gfx::rgb565_be(255, 196, 64);

// Largest scale (2 or 1) at which s fits in max_w pixels.
int fit_scale(const char* s, int max_w)
{
  return gfx::kFont8x8.text_width(s, 2) <= max_w ? 2 : 1;
}

// Splits s into lines of at most `cols` characters, breaking at spaces where
// possible. Returns the number of lines written.
int wrap(const char* s, int cols, char (*lines)[48], int max_lines)
{
  if (cols > 47)
    cols = 47;
  int n = 0;
  while (*s && n < max_lines)
  {
    while (*s == ' ')
      ++s;
    int len = static_cast<int>(std::strlen(s));
    int take = len <= cols ? len : cols;
    if (len > cols)
    {
      int space = take;
      while (space > 0 && s[space] != ' ')
        --space;
      if (space > 0)
        take = space;
    }
    std::memcpy(lines[n], s, take);
    lines[n][take] = '\0';
    ++n;
    s += take;
  }
  return n;
}

}  // namespace

void Menu::begin(display::Renderer& renderer, const sim::Manager& sims)
{
  renderer_ = &renderer;
  sims_ = &sims;
  group_count_ = 0;
  for (int i = 0; i < settings::kCount; ++i)
  {
    const char* g = settings::info(static_cast<settings::Id>(i)).group;
    if (!g)
      continue;  // not shown on the Settings page
    bool seen = false;
    for (int j = 0; j < group_count_; ++j)
      seen = seen || std::strcmp(groups_[j], g) == 0;
    if (!seen && group_count_ < kMaxGroups)
      groups_[group_count_++] = g;
  }
  renderer_->set_overlay(this);
  relayout();
}

void Menu::relayout()
{
  const int w = display::width();
  const int h = display::height();
  const auto corner = static_cast<settings::Corner>(settings::get(settings::Id::kIconPosition));
  const bool right = corner == settings::kTopRight || corner == settings::kBottomRight;
  const bool bottom = corner == settings::kBottomLeft || corner == settings::kBottomRight;
  const int ix = right ? w - kIconOffset - kIconSize : kIconOffset;
  const int iy = bottom ? h - kIconOffset - kIconSize : kIconOffset;
  icon_ = {ix, iy, kIconSize, kIconSize};
  icon_hit_ = Rect{ix - kIconOffset, iy - kIconOffset, kIconSize + 2 * kIconOffset,
                   kIconSize + 2 * kIconOffset}
                  .intersect({0, 0, w, h});

  panel_ = {kMargin, kMargin, w - 2 * kMargin, h - 2 * kMargin};
  content_ = {panel_.x, panel_.y + kTitleH, panel_.w, panel_.h - kTitleH};
  build_items();
  if (renderer_)
    renderer_->invalidate_all();
}

void Menu::open(Page page)
{
  const bool was_open = is_open();
  page_ = page;
  build_items();
  // Opening dims the whole scene; page changes only touch the panel.
  if (was_open)
    renderer_->invalidate(panel_);
  else
    renderer_->invalidate_all();
}

void Menu::close()
{
  page_ = Page::kClosed;
  item_count_ = 0;
  renderer_->invalidate_all();
}

void Menu::add_item(Kind kind, int arg, const Rect& rect)
{
  if (item_count_ < kMaxItems)
    items_[item_count_++] = {kind, arg, rect};
}

void Menu::build_items()
{
  item_count_ = 0;
  if (page_ == Page::kClosed)
    return;

  if (page_ != Page::kRoot)
    add_item(Kind::kBack, 0, {panel_.x, panel_.y, kTitleH, kTitleH});
  add_item(Kind::kClose, 0, {panel_.right() - kTitleH, panel_.y, kTitleH, kTitleH});

  int y = content_.y;
  auto row = [&](Kind kind, int arg, int h)
  {
    if (y + h <= content_.bottom())  // rows that don't fit are left out
      add_item(kind, arg, {content_.x, y, content_.w, h});
    y += h;
  };

  switch (page_)
  {
    case Page::kRoot:
      row(Kind::kPage, static_cast<int>(Page::kSimulation), kNavRowH);
      row(Kind::kPage, static_cast<int>(Page::kSettings), kNavRowH);
      row(Kind::kPage, static_cast<int>(Page::kAbout), kNavRowH);
      break;
    case Page::kSimulation:
      // TODO: page or scroll once there are more simulations than fit.
      for (int i = 0; i < sims_->count(); ++i)
        row(Kind::kSim, i, kSimRowH);
      break;
    case Page::kSettings:
      for (int g = 0; g < group_count_; ++g)
        row(Kind::kGroup, g, kNavRowH);
      break;
    case Page::kGroup:
      for (int i = 0; i < settings::kCount; ++i)
      {
        const char* g = settings::info(static_cast<settings::Id>(i)).group;
        if (g && std::strcmp(g, groups_[group_]) == 0)
          row(Kind::kSetting, i, kSetRowH);
      }
      break;
    default:
      break;
  }
}

const char* Menu::page_label(Page page)
{
  switch (page)
  {
    case Page::kSimulation:
      return "Simulation";
    case Page::kAbout:
      return "About";
    default:
      return "Settings";
  }
}

const char* Menu::title() const
{
  switch (page_)
  {
    case Page::kSimulation:
    case Page::kAbout:
    case Page::kSettings:
      return page_label(page_);
    case Page::kGroup:
      return groups_[group_];
    default:
      return "Menu";
  }
}

bool Menu::tap(int x, int y, uint32_t now_ms)
{
  last_tap_ms_ = now_ms;
  if (!is_open())
  {
    if (!icon_hit_.contains(x, y))
      return false;
    open(Page::kRoot);
    return true;
  }

  if (!panel_.contains(x, y))
  {
    close();
    return true;
  }
  for (int i = 0; i < item_count_; ++i)
  {
    const Item& it = items_[i];
    if (!it.rect.contains(x, y))
      continue;
    switch (it.kind)
    {
      case Kind::kBack:
        open(page_ == Page::kGroup ? Page::kSettings : Page::kRoot);
        break;
      case Kind::kClose:
        close();
        break;
      case Kind::kPage:
        open(static_cast<Page>(it.arg));
        break;
      case Kind::kGroup:
        group_ = it.arg;
        open(Page::kGroup);
        break;
      case Kind::kSetting:
      {
        const auto id = static_cast<settings::Id>(it.arg);
        const settings::Info& info = settings::info(id);
        // Bools toggle, enums cycle. The change listener may call relayout().
        settings::set(id, static_cast<uint8_t>((settings::get(id) + 1) % info.option_count));
        renderer_->invalidate(panel_);
        break;
      }
      case Kind::kSim:
        // The settings listener switches simulation; close to show it.
        settings::set(settings::Id::kSimulation, static_cast<uint8_t>(it.arg));
        close();
        break;
    }
    return true;
  }
  return true;  // inside the panel but not on an item
}

void Menu::toggle(uint32_t now_ms)
{
  last_tap_ms_ = now_ms;
  if (is_open())
    close();
  else
    open(Page::kRoot);
}

void Menu::tick(uint32_t now_ms)
{
  if (is_open() && kTimeoutMs > 0 && now_ms - last_tap_ms_ >= kTimeoutMs)
    close();
}

void Menu::compose(gfx::ChunkCanvas& c)
{
  if (is_open())
  {
    c.dim(c.bounds());
    draw_panel(c);
  }
  else
  {
    draw_icon(c);
  }
}

void Menu::draw_icon(gfx::ChunkCanvas& c)
{
  if (c.bounds().intersects(icon_))
    c.blend(kMenuIconPixels, kMenuIconAlpha, kIconSize, kIconSize, icon_.x, icon_.y);
}

void Menu::draw_panel(gfx::ChunkCanvas& c)
{
  if (!c.bounds().intersects(panel_))
    return;
  c.fill_rect(panel_, kPanelBg);

  // Title bar with Back (<) and Close (x) buttons.
  const Rect bar{panel_.x, panel_.y, panel_.w, kTitleH};
  if (c.bounds().intersects(bar))
  {
    c.fill_rect(bar, kTitleBg);
    const int ty = bar.y + (kTitleH - 16) / 2;
    c.text(bar.x + bar.w / 2, ty, title(), kText, 2, Align::kCenter);
    if (page_ != Page::kRoot)
      c.text(bar.x + kTitleH / 2, ty, "<", kAccent, 2, Align::kCenter);
    c.text(bar.right() - kTitleH / 2, ty, "x", kAccent, 2, Align::kCenter);
  }
  c.hline(panel_.x, panel_.y + kTitleH - 1, panel_.w, kBorder);

  if (page_ == Page::kAbout)
    draw_about(c);
  for (int i = 0; i < item_count_; ++i)
    if (items_[i].kind != Kind::kBack && items_[i].kind != Kind::kClose &&
        c.bounds().intersects(items_[i].rect))
      draw_item(c, items_[i]);

  c.rect(panel_, kBorder);
}

void Menu::draw_about(gfx::ChunkCanvas& c)
{
  const int x = content_.x + kPad;
  const int w = content_.w - 2 * kPad;
  int y = content_.y + kPad;

  c.text(x, y, buildinfo::name(), kAccent, fit_scale(buildinfo::name(), w));
  y += 16 + kPad;
  c.text(x, y, "Version", kMuted, 1);
  y += 8 + kPad / 2;

  // Same size as the label, wrapped if it is too long for the panel.
  char lines[6][48];
  const int n = wrap(buildinfo::version(), w / 8, lines, 6);
  for (int i = 0; i < n; ++i, y += 8 + 4)
    c.text(x, y, lines[i], kText, 1);
}

void Menu::draw_item(gfx::ChunkCanvas& c, const Item& item)
{
  const Rect& r = item.rect;
  const int x = r.x + kPad;
  const int inner_w = r.w - 2 * kPad;
  c.hline(r.x + kPad, r.bottom() - 1, r.w - 2 * kPad, kSeparator);

  if (item.kind == Kind::kSim)
  {
    const sim::Simulation& s = sims_->at(item.arg);
    const bool active = item.arg == sims_->active();
    if (active)
      c.fill_rect({r.x + 2, r.y + 4, 3, r.h - 8}, kAccent);
    c.text(x, r.y + 4, s.name(), active ? kAccent : kText, fit_scale(s.name(), inner_w));
    char line[1][48];
    if (wrap(s.description(), inner_w / 8, line, 1) == 1)
      c.text(x, r.y + 24, line[0], kMuted, 1);
    return;
  }

  if (item.kind == Kind::kPage || item.kind == Kind::kGroup)
  {
    const char* label =
        item.kind == Kind::kGroup ? groups_[item.arg] : page_label(static_cast<Page>(item.arg));
    const int ty = r.y + (r.h - 16) / 2;
    c.text(x, ty, label, kText, 2);
    c.text(r.right() - kPad, ty, ">", kMuted, 2, Align::kRight);
    return;
  }

  // Setting: label on the first line, value (or checkbox) on the second.
  const auto id = static_cast<settings::Id>(item.arg);
  const settings::Info& info = settings::info(id);
  const uint8_t value = settings::get(id);
  const bool is_bool = info.type == settings::Type::kBool;
  const int label_w = is_bool ? inner_w - kBox - kPad : inner_w;
  c.text(x, r.y + 6, info.label, kText, fit_scale(info.label, label_w));
  const int vy = r.y + 26;
  if (is_bool)
  {
    const Rect box{r.right() - kPad - kBox, r.y + (r.h - kBox) / 2, kBox, kBox};
    c.rect(box, kBorder, 2);
    if (value)
      c.fill_rect({box.x + 5, box.y + 5, box.w - 10, box.h - 10}, kAccent);
    c.text(x, vy, value ? "On" : "Off", kMuted, 2);
  }
  else
  {
    const char* opt = info.options[value];
    c.text(x, vy, opt, kAccent, fit_scale(opt, inner_w - 16));
    c.text(r.right() - kPad, vy, ">", kMuted, 2, Align::kRight);
  }
}

}  // namespace ui
