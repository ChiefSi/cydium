#include "display.h"

#include "board.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

namespace display
{
namespace
{

constexpr const char* kTag = "display";
constexpr spi_host_device_t kHost = SPI2_HOST;
constexpr size_t kChunkPixels = kMaxWidth * kChunkRows;
constexpr int kNumBuffers = 2;

esp_lcd_panel_io_handle_t s_io = nullptr;
esp_lcd_panel_handle_t s_panel = nullptr;
uint16_t* s_buf[kNumBuffers] = {};
// Counts buffers that are free (not queued for DMA).
SemaphoreHandle_t s_free = nullptr;
int s_next = 0;  // next buffer handed out by acquire_buffer()
bool s_inverted = false;
gfx::Orientation s_orientation = gfx::Orientation::kPortrait;

#ifdef CONFIG_CYD_LCD_SWAP_XY
constexpr bool kSwapXY = true;
#else
constexpr bool kSwapXY = false;
#endif
#ifdef CONFIG_CYD_LCD_MIRROR_X
constexpr bool kMirrorX = true;
#else
constexpr bool kMirrorX = false;
#endif
#ifdef CONFIG_CYD_LCD_MIRROR_Y
constexpr bool kMirrorY = true;
#else
constexpr bool kMirrorY = false;
#endif
#ifdef CONFIG_CYD_LCD_BGR
constexpr lcd_rgb_element_order_t kElementOrder = LCD_RGB_ELEMENT_ORDER_BGR;
#else
constexpr lcd_rgb_element_order_t kElementOrder = LCD_RGB_ELEMENT_ORDER_RGB;
#endif
#ifdef CONFIG_CYD_LCD_INVERT_DEFAULT
constexpr bool kInvertDefault = true;
#else
constexpr bool kInvertDefault = false;
#endif

bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*)
{
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(s_free, &woken);
  return woken == pdTRUE;
}

#ifdef CONFIG_CYD_LCD_PROBE
// Reads the controller's ID registers over MISO at a slow clock (reads are
// limited to a few MHz) and logs them. ILI9341: RDDIDIF (D3h) = xx 00 93 41.
// ST7789: RDDID (04h) is 85 85 52 shifted by one dummy bit.
void probe_controller()
{
  esp_lcd_panel_io_spi_config_t cfg = {};
  cfg.cs_gpio_num = board::pins::kTftCs;
  cfg.dc_gpio_num = board::pins::kTftDc;
  cfg.spi_mode = 0;
  cfg.pclk_hz = 4 * 1000 * 1000;
  cfg.trans_queue_depth = 1;
  cfg.lcd_cmd_bits = 8;
  cfg.lcd_param_bits = 8;
  esp_lcd_panel_io_handle_t io = nullptr;
  if (esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(kHost), &cfg, &io) != ESP_OK)
  {
    ESP_LOGW(kTag, "probe: could not create slow panel IO");
    return;
  }
  esp_lcd_panel_io_tx_param(io, 0x01, nullptr, 0);  // SWRESET
  vTaskDelay(pdMS_TO_TICKS(150));

  constexpr int kRegs[] = {0x04, 0xD3, 0x09, 0x0B};
  for (int reg : kRegs)
  {
    uint8_t b[4] = {};
    esp_err_t err = esp_lcd_panel_io_rx_param(io, reg, b, sizeof(b));
    ESP_LOGI(kTag, "probe: reg %02Xh -> %02X %02X %02X %02X (%s)", reg, b[0], b[1], b[2], b[3],
             esp_err_to_name(err));
  }
  esp_lcd_panel_io_del(io);
}
#endif

struct Madctl
{
  bool swap;
  bool mirror_x;
  bool mirror_y;
};

// Panel address mapping for each orientation. Portrait comes from Kconfig
// (checked with the diagnostic image). Rotating by 90 degrees toggles the
// axis swap; which mirror bit also flips decides the direction, and a
// 180-degree turn between the two landscapes flips both mirrors. The
// landscape directions were confirmed on a board: flipping MX with the swap
// puts the USB port on the right.
Madctl madctl_for(gfx::Orientation o)
{
  switch (o)
  {
    case gfx::Orientation::kLandscapeCw:  // USB on the left
      return {!kSwapXY, kMirrorX, !kMirrorY};
    case gfx::Orientation::kLandscapeCcw:  // USB on the right
      return {!kSwapXY, !kMirrorX, kMirrorY};
    case gfx::Orientation::kPortrait:
    default:
      return {kSwapXY, kMirrorX, kMirrorY};
  }
}

}  // namespace

esp_err_t init()
{
  for (auto& b : s_buf)
  {
    b = static_cast<uint16_t*>(heap_caps_malloc(kChunkPixels * sizeof(uint16_t), MALLOC_CAP_DMA));
    if (!b)
    {
      ESP_LOGE(kTag, "out of DMA memory for chunk buffers");
      return ESP_ERR_NO_MEM;
    }
  }
  s_free = xSemaphoreCreateCounting(kNumBuffers, kNumBuffers);
  if (!s_free)
    return ESP_ERR_NO_MEM;

  spi_bus_config_t bus = {};
  bus.sclk_io_num = board::pins::kTftSclk;
  bus.mosi_io_num = board::pins::kTftMosi;
  bus.miso_io_num = board::pins::kTftMiso;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = kChunkPixels * sizeof(uint16_t);
  ESP_ERROR_CHECK(spi_bus_initialize(kHost, &bus, SPI_DMA_CH_AUTO));
#ifdef CONFIG_CYD_LCD_PROBE
  probe_controller();
#endif

  esp_lcd_panel_io_spi_config_t io = {};
  io.cs_gpio_num = board::pins::kTftCs;
  io.dc_gpio_num = board::pins::kTftDc;
  io.spi_mode = 0;
  io.pclk_hz = CONFIG_CYD_LCD_PCLK_MHZ * 1000 * 1000;
  io.trans_queue_depth = 10;
  io.on_color_trans_done = on_color_trans_done;
  io.lcd_cmd_bits = 8;
  io.lcd_param_bits = 8;
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(kHost), &io, &s_io));

  esp_lcd_panel_dev_config_t dev = {};
  dev.reset_gpio_num = board::pins::kTftRst;
  dev.rgb_ele_order = kElementOrder;
  dev.bits_per_pixel = 16;
  ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(s_io, &dev, &s_panel));

  ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
  set_orientation(gfx::Orientation::kPortrait);
  set_invert(kInvertDefault);

  fill(0);  // clear any power-on garbage before the backlight comes on
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

  ESP_LOGI(kTag, "ILI9341 %dx%d ready, pclk %d MHz, inverted=%d", width(), height(),
           CONFIG_CYD_LCD_PCLK_MHZ, s_inverted);
  return ESP_OK;
}

esp_lcd_panel_handle_t panel()
{
  return s_panel;
}

void set_invert(bool invert)
{
  s_inverted = invert;
  esp_lcd_panel_invert_color(s_panel, invert);
}

bool inverted()
{
  return s_inverted;
}

void set_orientation(gfx::Orientation o)
{
  wait_idle();
  const Madctl m = madctl_for(o);
  esp_lcd_panel_swap_xy(s_panel, m.swap);
  esp_lcd_panel_mirror(s_panel, m.mirror_x, m.mirror_y);
  s_orientation = o;
}

gfx::Orientation orientation()
{
  return s_orientation;
}

int width()
{
  return gfx::screen_width(s_orientation);
}

int height()
{
  return gfx::screen_height(s_orientation);
}

uint16_t* acquire_buffer()
{
  xSemaphoreTake(s_free, portMAX_DELAY);
  uint16_t* buf = s_buf[s_next];
  s_next = (s_next + 1) % kNumBuffers;
  return buf;
}

void send(int y, int rows, uint16_t* buf)
{
  esp_lcd_panel_draw_bitmap(s_panel, 0, y, width(), y + rows, buf);
}

void wait_idle()
{
  for (int i = 0; i < kNumBuffers; ++i)
    xSemaphoreTake(s_free, portMAX_DELAY);
  for (int i = 0; i < kNumBuffers; ++i)
    xSemaphoreGive(s_free);
}

void fill_rect(int x0, int y0, int x1, int y1, uint16_t color_be)
{
  x0 = x0 < 0 ? 0 : x0;
  y0 = y0 < 0 ? 0 : y0;
  x1 = x1 > width() ? width() : x1;
  y1 = y1 > height() ? height() : y1;
  if (x0 >= x1 || y0 >= y1)
    return;
  const int w = x1 - x0;
  const int rows_per_chunk = static_cast<int>(kChunkPixels) / w;
  for (int y = y0; y < y1; y += rows_per_chunk)
  {
    const int rows = (y1 - y < rows_per_chunk) ? (y1 - y) : rows_per_chunk;
    uint16_t* buf = acquire_buffer();
    for (int i = 0; i < w * rows; ++i)
      buf[i] = color_be;
    esp_lcd_panel_draw_bitmap(s_panel, x0, y, x1, y + rows, buf);
  }
  wait_idle();
}

void blit(int x, int y, int w, int h, const uint16_t* pixels_be)
{
  if (w <= 0 || h <= 0 || static_cast<size_t>(w * h) > kChunkPixels)
    return;
  uint16_t* buf = acquire_buffer();
  for (int i = 0; i < w * h; ++i)
    buf[i] = pixels_be[i];
  esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, buf);
  wait_idle();
}

void fill(uint16_t color_be)
{
  fill_rect(0, 0, width(), height(), color_be);
}

}  // namespace display
