#include "renderer.h"

#include "display.h"
#include "esp_log.h"

namespace display
{
namespace
{
constexpr const char* kTag = "renderer";
}  // namespace

static_assert(320 / kChunkRows <= 32, "dirty mask needs one bit per chunk");

esp_err_t Renderer::begin()
{
  if (!bg_.begin(width(), height()) || !fg_.begin(width(), height()))
  {
    ESP_LOGE(kTag, "out of memory for layers");
    return ESP_ERR_NO_MEM;
  }
  bg_.clear(0);
  fg_.clear(0);
  resize();
  return ESP_OK;
}

void Renderer::resize()
{
  bg_.resize(width(), height());
  fg_.resize(width(), height());
  chunks_ = (height() + kChunkRows - 1) / kChunkRows;
  invalidate_all();
}

void Renderer::invalidate(const gfx::Rect& r)
{
  const gfx::Rect a = r.intersect({0, 0, width(), height()});
  if (a.empty())
    return;
  for (int c = a.y / kChunkRows; c <= (a.bottom() - 1) / kChunkRows; ++c)
    dirty_ |= 1u << c;
}

void Renderer::invalidate_all()
{
  dirty_ = chunks_ >= 32 ? ~0u : (1u << chunks_) - 1;
}

int Renderer::present()
{
  const int w = width();
  const int h = height();
  const uint16_t* pal = palette_.data();
  int sent = 0;
  for (int c = 0; c < chunks_; ++c)
  {
    if (!(dirty_ & (1u << c)))
      continue;
    const int y0 = c * kChunkRows;
    const int rows = (h - y0 < kChunkRows) ? (h - y0) : kChunkRows;
    uint16_t* buf = acquire_buffer();
    for (int y = 0; y < rows; ++y)
    {
      const uint8_t* b = bg_.row(y0 + y);
      const uint8_t* f = fg_.row(y0 + y);
      uint16_t* out = buf + y * w;
      for (int x = 0; x < w; ++x)
        out[x] = pal[f[x] ? f[x] : b[x]];
    }
    if (overlay_)
    {
      gfx::ChunkCanvas canvas(buf, w, y0, rows);
      overlay_->compose(canvas);
    }
    send(y0, rows, buf);
    ++sent;
  }
  dirty_ = 0;
  return sent;
}

}  // namespace display
