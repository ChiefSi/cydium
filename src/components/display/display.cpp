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
constexpr int kW = board::kScreenWidth;
constexpr int kH = board::kScreenHeight;
constexpr size_t kChunkPixels = kW * kChunkRows;
constexpr int kNumBuffers = 2;

esp_lcd_panel_io_handle_t s_io = nullptr;
esp_lcd_panel_handle_t s_panel = nullptr;
uint16_t* s_buf[kNumBuffers] = {};
// Counts buffers that are free (not queued for DMA).
SemaphoreHandle_t s_free = nullptr;
bool s_inverted = false;

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
      esp_lcd_new_panel_io_spi(reinterpret_cast<esp_lcd_spi_bus_handle_t>(kHost), &io, &s_io));

  esp_lcd_panel_dev_config_t dev = {};
  dev.reset_gpio_num = board::pins::kTftRst;
  dev.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
  dev.bits_per_pixel = 16;
  ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(s_io, &dev, &s_panel));

  ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, kMirrorX, kMirrorY));
  set_invert(kInvertDefault);

  fill(0);  // clear any power-on garbage before the backlight comes on
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

  ESP_LOGI(kTag, "ILI9341 %dx%d ready, pclk %d MHz, inverted=%d", kW, kH, CONFIG_CYD_LCD_PCLK_MHZ,
           s_inverted);
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

void fill(uint16_t color_be)
{
  int next = 0;
  for (int y = 0; y < kH; y += kChunkRows)
  {
    xSemaphoreTake(s_free, portMAX_DELAY);
    uint16_t* buf = s_buf[next];
    next = (next + 1) % kNumBuffers;
    for (size_t i = 0; i < kChunkPixels; ++i)
      buf[i] = color_be;
    esp_lcd_panel_draw_bitmap(s_panel, 0, y, kW, y + kChunkRows, buf);
  }
  // Wait for all in-flight chunks, then release the buffers again.
  for (int i = 0; i < kNumBuffers; ++i)
    xSemaphoreTake(s_free, portMAX_DELAY);
  for (int i = 0; i < kNumBuffers; ++i)
    xSemaphoreGive(s_free);
}

}  // namespace display
