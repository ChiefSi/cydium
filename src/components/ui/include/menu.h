#pragma once

#include <array>
#include <cstdint>

#include "gfx/canvas.h"
#include "gfx/rect.h"
#include "renderer.h"

namespace sim
{
class Manager;
}

namespace ui
{

// The on-screen menu: a hamburger icon in one corner that opens a panel with
// Simulation, Settings and About pages. Drawn as a renderer Overlay, straight into the
// output chunks, so it needs no layer memory of its own.
class Menu : public display::Overlay
{
 public:
  // Call after renderer.begin() and settings::init(). The Simulation page
  // lists the simulations in sims; choosing one sets settings::Id::kSimulation
  // (the settings listener switches to it) and closes the menu.
  void begin(display::Renderer& renderer, const sim::Manager& sims);

  // Recomputes the layout for the current display size and icon position
  // setting, and repaints. Call after the orientation or icon setting changes.
  void relayout();

  // Handles a tap at screen coordinates. Returns true if the menu used it;
  // false means it belongs to the scene underneath.
  bool tap(int x, int y, uint32_t now_ms);

  // Opens the menu at its top page, or closes it if open. For the BOOT
  // button.
  void toggle(uint32_t now_ms);

  // Closes the menu once CYD_MENU_TIMEOUT_S passes without a tap.
  void tick(uint32_t now_ms);

  bool is_open() const
  {
    return page_ != Page::kClosed;
  }

  void compose(gfx::ChunkCanvas& canvas) override;

 private:
  enum class Page : uint8_t
  {
    kClosed,
    kRoot,
    kSimulation,
    kAbout,
    kSettings,
    kGroup,  // the settings in group_
  };

  enum class Kind : uint8_t
  {
    kBack,
    kClose,
    kPage,     // arg: Page
    kGroup,    // arg: group index
    kSetting,  // arg: settings::Id
    kSim,      // arg: simulation index
  };

  struct Item
  {
    Kind kind;
    int arg;
    gfx::Rect rect;
  };

  static constexpr int kMaxItems = 12;
  static constexpr int kMaxGroups = 8;

  void open(Page page);
  void close();
  void build_items();
  void add_item(Kind kind, int arg, const gfx::Rect& rect);
  static const char* page_label(Page page);
  const char* title() const;
  void draw_icon(gfx::ChunkCanvas& c);
  void draw_panel(gfx::ChunkCanvas& c);
  void draw_about(gfx::ChunkCanvas& c);
  void draw_item(gfx::ChunkCanvas& c, const Item& item);

  display::Renderer* renderer_ = nullptr;
  const sim::Manager* sims_ = nullptr;
  Page page_ = Page::kClosed;
  int group_ = 0;
  uint32_t last_tap_ms_ = 0;

  gfx::Rect icon_;      // where the icon is drawn
  gfx::Rect icon_hit_;  // icon plus the offset margin
  gfx::Rect panel_;
  gfx::Rect content_;  // panel area below the title bar

  std::array<Item, kMaxItems> items_{};
  int item_count_ = 0;

  std::array<const char*, kMaxGroups> groups_{};
  int group_count_ = 0;
};

}  // namespace ui
