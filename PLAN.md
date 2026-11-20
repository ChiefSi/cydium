# Project Plan: Multi-Simulation Display for the CYD

Status (2026-11-20): Phases 1 and 2 are done. Phase 3 has started (the
`sim` framework with two demo scenes, Bounce and Starfield, chosen from the
menu) and so has Phase 4 (the menu with Simulation, About and display
settings). Deviations from this plan so far:

- **Orientation:** landscape (320×240) is supported as well as portrait, as a
  menu setting. Layers keep the same 4 strips and are just reinterpreted.
- **Settings:** stored as single bytes in NVS for now, not
  `/fs/settings.json`. Defaults come from Kconfig.
- **Touch calibration:** Kconfig values (tuned with the diagnostic image)
  instead of the 3-point calibration screen, for now.
- **Simulation interface:** trimmed for now. Simulations draw through a
  `sim::Surface` (layers, palette, invalidation) rather than the renderer,
  with a seedable `sim::Rng` instead of `esp_random()`, so they run in host
  tests. `update`/`render` are one `step(ctx, dt_ms)`; `begin` also runs
  again after an orientation change; events and `onHold` come with Phase 5.
- **Overlays:** the menu draws straight into the output chunks (a renderer
  `Overlay`), so it needs no third layer.

## Overview

Firmware for the ESP32-2432S028R ("Cheap Yellow Display", CYD), written in C++
on **plain ESP-IDF** (no Arduino layer). It runs a set of ambient pixel-art
simulations at the panel's full native resolution:

- **2D:** aquarium, space, traffic junction, walkers.
- **Isometric:** countryside, cityscape.

Each simulation has its own random events, which can also be triggered by hand
from a menu. Settings and saved state live in on-board flash, with an optional
microSD card for editable settings and extra assets.

## Requirements

1. **Simulations**
   - 2D: aquarium, space, traffic junction, walkers. "Walkers" is an original
     scene with Lemmings-style mechanics (small creatures that walk, fall,
     climb, dig and build), not a copy of that game's name, sprites or levels.
   - Isometric: countryside, cityscape.
2. **Menu button** with pages to:
   - change the simulation;
   - change colour and display settings.
3. **Simulation-specific events** that:
   - occur at random;
   - can be triggered manually with a button.
4. **Maximise pixel count.** Render at the panel's native 240×320 (or 320×240
   in landscape). A 256-colour
   palette per simulation is acceptable for pixel art.
5. **Use the microSD slot** for editable settings, saved state and optional
   assets. Everything must still work without a card.
6. **Plain ESP-IDF.** Build with `idf.py`, use IDF drivers and managed
   components, and don't depend on the Arduino core.

## Hardware

### ESP32-2432S028R

| Part | Detail |
| --- | --- |
| Module | ESP32-WROOM-32, 240 MHz dual core, no PSRAM |
| Flash | 4 MB |
| RAM | 520 KB SRAM; roughly 300 KB usable heap, no single block larger than ~110 KB |
| Display | 2.8" ILI9341, 240×320, SPI |
| Touch | XPT2046 resistive |
| Storage | microSD slot |
| Other | LDR light sensor, RGB LED, speaker header, BOOT button |

### Pin map and peripheral allocation

| Function | Pins | ESP-IDF peripheral |
| --- | --- | --- |
| TFT | MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, RST none | `SPI2_HOST` (HSPI, IO_MUX pins) |
| microSD | CS 5, SCK 18, MISO 19, MOSI 23 | `SPI3_HOST` (VSPI, IO_MUX pins) |
| Touch (XPT2046) | CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36 | GPIO bit-bang (no SPI host left) |
| Backlight | 21, active high | LEDC PWM |
| Light sensor (LDR) | 34, input only | ADC1 channel 6 (`esp_adc` oneshot) |
| RGB LED | 4 (R), 16 (G), 17 (B), active low | LEDC PWM (optional) |
| BOOT button | 0, usable as an input after boot | GPIO input with pull-up |
| Speaker | 26 | DAC / LEDC (optional, out of scope for v1) |

The ESP32 has only two SPI hosts free for user peripherals (`SPI2_HOST`,
`SPI3_HOST`). The display and SD card each take one. Touch is on a third
group of pins, so it uses a small bit-banged driver. That's fine: it only
needs a few samples per frame at a low clock rate.

### Known quirks

Clones vary, so verify these on each board:

- **Colour inversion differs between panels.** Some need inversion on, others
  off. A wrong setting turns dark scenes bright and washed out. Make it a
  runtime setting applied with `esp_lcd_panel_invert_color()`.
- **Touch X can be reversed** relative to the display. Make calibration and
  axis inversion configurable.
- **The light sensor reads backwards:** a lower ADC value means a brighter
  room. Use `ADC_ATTEN_DB_0` for usable range.
- **Some CH340 USB chips fail at high baud rates.** Flash with
  `idf.py -b 115200 flash`.
- **Linux/WSL serial access:** the user must be in the `dialout` group, and
  under WSL2 the USB device has to be attached with `usbipd`.

## Toolchain

- **ESP-IDF:** install it with the ESP-IDF installer or `install.sh`, and pin
  one stable release (v5.5.x or newer). Record the exact version in
  `README.md` and check `IDF_VER` in CI.
- **Build:**

  ```
  idf.py set-target esp32
  idf.py build
  idf.py -p /dev/ttyUSB0 -b 115200 flash monitor
  ```

- **Managed components:** declared in `main/idf_component.yml` with exact
  versions, and `dependencies.lock` committed.

  | Component | Use |
  | --- | --- |
  | `espressif/esp_lcd_ili9341` | ILI9341 panel driver on top of IDF `esp_lcd` |
  | `joltwallet/littlefs` | LittleFS filesystem for internal flash |

- **Built into IDF, no extra components needed:**

  | Need | IDF API |
  | --- | --- |
  | SPI display I/O | `esp_lcd_new_panel_io_spi`, `spi_bus_initialize` |
  | SD card | `esp_vfs_fat_sdspi_mount` (FATFS) |
  | Backlight / LED PWM | `driver/ledc.h` |
  | Light sensor | `esp_adc/adc_oneshot.h` |
  | Buttons, touch bit-bang | `driver/gpio.h`, `esp_rom_delay_us` |
  | Settings JSON | cJSON (`json` component) |
  | Small key-value state | NVS (`nvs_flash`) |
  | Wi-Fi + time | `esp_wifi`, `esp_netif_sntp`, `setenv("TZ", …)` / `tzset()` |
  | Tasks, timing | FreeRTOS, `esp_timer` |
  | Logging | `esp_log` |

- **`sdkconfig.defaults`** (committed; `sdkconfig` itself is generated):

  ```
  CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
  CONFIG_PARTITION_TABLE_CUSTOM=y
  CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
  CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
  CONFIG_COMPILER_OPTIMIZATION_PERF=y
  CONFIG_FREERTOS_HZ=1000
  CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
  CONFIG_FATFS_LFN_HEAP=y
  CONFIG_LOG_DEFAULT_LEVEL_INFO=y
  ```

- **`partitions.csv`** (no OTA; one large app plus LittleFS, filling 4 MB):

  ```
  # Name,   Type, SubType,  Offset,   Size
  nvs,      data, nvs,      0x9000,   0x6000
  phy_init, data, phy,      0xf000,   0x1000
  factory,  app,  factory,  0x10000,  0x300000
  storage,  data, littlefs, 0x310000, 0xF0000
  ```

  This gives a 3 MB app and 960 KB of LittleFS.
- **Host tests:** keep simulation logic free of hardware calls, so it can be
  unit-tested on the IDF `linux` target (`idf.py --preview set-target linux`)
  or with a plain host CMake build. Use Unity, which ships with IDF.

## Rendering at native resolution

### Format choice

| Format | Layers | RAM at 240×320 | Verdict |
| --- | --- | --- | --- |
| 24-bit RGB, 3 B/px | 2 | ~460 KB | impossible (more than total RAM) |
| RGB565, 2 B/px | 1 | ~154 KB | fits, but no background/foreground split |
| **Indexed, 1 B/px + palette** | **2** | **~154 KB** | **chosen** |

### Design

- **Layers:** two indexed layers at 240×320 (76.8 KB each), allocated with
  `heap_caps_malloc(…, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL)`.
  - The background layer holds static or slow scenery.
  - The foreground layer holds moving things. Index 0 means transparent.
- **Allocation in strips:** the heap can't reliably provide one 76.8 KB block,
  so each layer is 4 strips of 80 rows (~19 KB each). Allocate them once at
  boot and never free them.
- **Palettes:** one 256-entry RGB565 palette per simulation (512 bytes).
  - Store entries **byte-swapped** (big-endian), as the ILI9341 expects, so no
    swap is needed per pixel.
  - Display settings are applied to the palette, not to each pixel:
    brightness, background/foreground level, saturation and tone curve.
    Changes are therefore instant and cost nothing per frame.
  - Palette tricks give cheap animation: colour cycling (water, lights,
    stars) and day/night tinting by blending the whole palette.
- **Output path (`esp_lcd`):**
  - Two DMA-capable chunk buffers of 16 rows each (240 × 16 × 2 B = 7.5 KB
    each), allocated with `MALLOC_CAP_DMA`.
  - Composite a 16-row chunk (foreground over background, through the
    palette) into one buffer and send it with `esp_lcd_panel_draw_bitmap()`.
    Composite the next chunk into the other buffer while DMA runs. The
    `on_color_trans_done` callback signals that a buffer is free again.
  - Only chunks containing dirty rows are composited and sent.
  - Panel I/O: `SPI2_HOST`, 40 MHz pixel clock to start. The TFT pins are
    HSPI's native IO_MUX pins, so try 60–80 MHz once it works and keep the
    fastest clock that's stable.
- **Drawing API:** the project's own `Canvas` class over an indexed layer.
  There's no Adafruit_GFX without Arduino, and these are all short to write:
  - pixel, horizontal/vertical line, line (Bresenham), filled and outlined
    rectangles, circle;
  - 8-bit sprite blits with a transparent index, optional flip and palette
    offset;
  - tiled/isometric blit helpers;
  - bitmap fonts (5×7 and 8×8, as `const` arrays) with text measuring and
    alignment.
- **Frame rate:** a full 240×320 RGB565 frame is 153.6 KB. That takes about
  31 ms at 40 MHz (about 32 fps) and about 15 ms at 80 MHz. Typical scenes
  change only part of the screen and run faster thanks to dirty chunks.
  Target 30 fps.
- **RAM budget:**
  - layers ~154 KB;
  - DMA chunk buffers 15 KB;
  - palettes ~1 KB;
  - simulation state ≤ ~50 KB;
  - Wi-Fi during NTP sync ~40–50 KB.

  Large assets are streamed from flash (embedded with `EMBED_FILES` or
  `const` arrays) or from SD, not held in RAM.

## Tasks

| Task | Core | Priority | Job |
| --- | --- | --- | --- |
| `render` | 1 | high | fixed-timestep loop: input → simulation update → draw → composite and push |
| `input` | 1 | medium | touch sampling (only while IRQ 36 is low), BOOT button debounce, posts events to a queue |
| `system` | 0 | low | settings save (debounced), SD mount and remount, light sensor, NTP sync |
| Wi-Fi / lwIP | 0 | IDF default | started only for time sync, then stopped |

The render task owns the layers and the active simulation. Other tasks talk to
it only through FreeRTOS queues, so the drawing code needs no locks.

## Architecture

```
CMakeLists.txt
partitions.csv
sdkconfig.defaults
main/
  CMakeLists.txt
  idf_component.yml
  app_main.cpp           init order, task creation
components/
  board/                 pin map, LEDC backlight, ADC light sensor, BOOT button
  display/               esp_lcd ILI9341 setup, Renderer (chunks, DMA, dirty rows)
  gfx/                   IndexedLayer, Canvas primitives, Palette, fonts, sprites
  touch/                 bit-banged XPT2046, calibration, gestures (tap, hold)
  storage/               LittleFS mount, SD mount on SPI3, Store with SD override
  settings/              Settings struct, cJSON load/save
  timesync/              Wi-Fi connect, SNTP, timezone
  sim/                   Simulation interface, SimulationManager, EventScheduler
  ui/                    Menu pages, Toast, clock overlay
  sims/
    aquarium/ space/ traffic/ walkers/          (2D)
    iso/ countryside/ cityscape/                (isometric)
test/                    host unit tests for gfx, sim logic, event scheduler
assets/                  source sprite sheets and palettes + conversion script
```

Each directory under `components/` is an ESP-IDF component with its own
`CMakeLists.txt` declaring `REQUIRES`. Only `board`, `display`, `touch`,
`storage` and `timesync` touch hardware, so `gfx`, `sim` and `sims` can be
built and tested on the host.

### Simulation interface

```cpp
struct SimEvent {
  const char* name;
  uint16_t minGapSec;   // minimum time between random occurrences
  uint8_t weight;       // relative chance when picked at random
};

class Simulation {
 public:
  virtual ~Simulation() = default;
  virtual const char* name() const = 0;
  virtual const char* description() const = 0;
  virtual const Palette& palette() const = 0;   // 256 entries
  virtual void begin(Canvas& bg) = 0;           // allocate, draw scenery
  virtual void end() = 0;                       // free everything
  virtual void update(uint32_t dtMs, const tm* now) = 0;
  virtual void render(Canvas& bg, Canvas& fg) = 0;
  virtual void onTap(int16_t x, int16_t y) {}
  virtual void onHold(int16_t x, int16_t y) {}
  virtual const SimEvent* events(uint8_t* count) const = 0;
  virtual void triggerEvent(uint8_t index) = 0;
  virtual bool isEventActive() const = 0;
};
```

Only one simulation is in memory at a time. Switching calls `end()` on the old
one and `begin()` on the new one, which keeps RAM usage flat. The layers
belong to the renderer and are shared. Simulations use `esp_random()`
(seedable on host tests) for randomness.

## Input and menu

- **Menu button:**
  - an icon in the top-left corner of the screen;
  - also the **BOOT button (GPIO0)** as a physical menu button.

  Everywhere else on screen belongs to the simulation (taps and holds).
- **Menu pages:**
  1. **Simulation:** the six scenes, each with a one-line description. The
     last one used is restored at boot.
  2. **Display:**
     - backlight;
     - background level;
     - foreground level;
     - saturation;
     - colour inversion;
     - auto-backlight from the light sensor;
     - clock overlay on/off.

     The live scene stays visible behind a dimmed menu panel, so changes can
     be seen as they're made.
  3. **Events:** the current scene's events as buttons, plus a random-event
     frequency setting (off / low / normal / high).
- The menu closes itself after 10 seconds without a touch.
- **Touch calibration:** a 3-point calibration screen on first boot, or when
  the screen is held at boot. The result is saved in settings.

## Events

- `EventScheduler` picks a random event based on:
  - each event's weight;
  - its minimum gap between repeats;
  - the global frequency setting.
- Manual buttons call the same trigger path, so random and manual events
  behave identically.
- A toast shows the event's name when it starts.
- Only one event runs at a time. A manual trigger during an event is queued.

## Simulations

### 2D

| Sim | Scene | Events |
| --- | --- | --- |
| Aquarium | fish, plants, bubbles, animated water | feeding, visitor at the glass, plankton bloom, shark pass |
| Space | parallax starfield, planets, drifting ships | meteor shower, comet, supernova flash, convoy flyby |
| Traffic junction | 4-way crossroads, light cycle, queuing cars, pedestrians | emergency vehicle (cars pull over), rush hour, breakdown, rain |
| Walkers | small creatures on platform terrain; they walk, fall, climb, dig and build | spawner opens, flood, bridge builder, digging spree |

Taps are scene-specific, for example drop food in the aquarium or toggle a
traffic light.

### Isometric

- **Shared `iso` component:**
  - 2:1 diamond tiles drawn back to front. 32×16 px tiles give roughly an
    8×20-tile visible area in portrait.
  - 8-bit sprite sheets, embedded or replaceable from SD.
  - Day/night tint by blending the palette, driven by the clock.
  - Static terrain is drawn once into the background layer and redrawn only
    when it changes. Moving objects go on the foreground layer, so most frames
    touch only a few dirty chunks.
- **Countryside:** fields, river, trees, tractor, animals.
  - Events: harvest, storm, hot-air balloon, season change.
- **Cityscape:** buildings, roads with cars, windows lit at night.
  - Events: fireworks, power cut, crane construction, parade.

### Assets

Source art (PNG) lives in `assets/`. A host-side script converts each image to
an 8-bit indexed sprite sheet plus a 256-entry palette, and writes either:

- a binary file embedded with `EMBED_FILES` in the component's `CMakeLists.txt`;
- or the same file format for the SD card.

## Storage

- **LittleFS** (`storage` partition, mounted with `esp_vfs_littlefs_register`
  at `/fs`):
  - `/fs/settings.json`;
  - `/fs/<sim>/state.json` for any persistent per-simulation progress.
- **NVS:** only tiny values that change often (e.g. last simulation index), to
  spare LittleFS writes.
- **SD card (optional):** mounted at boot with `esp_vfs_fat_sdspi_mount` on
  `SPI3_HOST` at `/sd`.
  - `/sd/cyd/settings.json`: if present, overrides the LittleFS copy, so
    settings can be edited on a PC;
  - `/sd/cyd/<sim>/`: optional replacement sprite sheets and palettes;
  - `/sd/cyd/screenshots/`: optional screenshot capture (the two indexed
    layers plus the palette, converted on a PC).
- **Missing or bad card:** the firmware runs entirely from LittleFS and
  embedded assets. The `system` task retries the mount occasionally so a
  card can be inserted later.

## Clock

- Optional Wi-Fi/NTP time sync:
  - `esp_wifi` in station mode, credentials from settings;
  - `esp_netif_sntp`;
  - a POSIX timezone string applied with `setenv("TZ", …)` and `tzset()`.
- Wi-Fi is stopped and de-initialised after a successful sync to free its RAM,
  then restarted periodically to resync.
- Without network access, fall back to the build time (`__DATE__`/`__TIME__`).
- The clock drives the day/night tint and an optional clock overlay.

## Phases

Each phase can ship on its own.

1. **Skeleton.**
   - ESP-IDF project with pinned IDF version, managed components and lock
     file, `sdkconfig.defaults`, `partitions.csv`.
   - `board` component: LEDC backlight, BOOT button, light sensor.
   - `display` init through `esp_lcd` with runtime inversion, showing a solid
     colour test.
   - Bit-banged touch with serial logging of raw coordinates.
2. **Renderer.**
   - `IndexedLayer` strips, `Canvas` primitives, fonts, palette.
   - Chunked double-buffered DMA output with dirty tracking.
   - A test-pattern scene that logs fps and free heap
     (`heap_caps_get_free_size`, `heap_caps_get_largest_free_block`) with and
     without Wi-Fi.
   - Host unit tests for `gfx`.
3. **Framework.**
   - `Simulation` interface, `SimulationManager`, render and input tasks.
   - Settings with cJSON on LittleFS, plus touch calibration.
   - First simulation: Aquarium.
4. **Menu.** The three menu pages, auto-close, and settings applied through
   the palette.
5. **Events.** `EventScheduler` with host tests, toasts, and the aquarium's
   events.
6. **SD card.** Mounting, settings override, asset loading, screenshots.
7. **Remaining 2D simulations.** Space, traffic junction, walkers.
8. **Isometric.** `iso` component, asset pipeline, countryside, cityscape.
9. **Clock.** Wi-Fi/SNTP, day/night tint, clock overlay.

## Risks and open questions

1. **RAM headroom.** About 115 KB remains after the drawing layers and DMA
   buffers. Simulation state must stay compact, and Wi-Fi is only running
   during sync. If it gets too tight, the fallback is band rendering: draw the
   screen in horizontal bands, holding only one band in RAM. That uses far
   less memory, but every simulation has to be able to redraw any band on
   request.
2. **Frame rate.** Full-screen redraws are capped by SPI bandwidth (about
   32 fps at 40 MHz). Keep scenery on the background layer, rely on dirty
   chunks, and raise the SPI clock if the panel tolerates it.
3. **Palette limits.** Smooth gradients can band at 256 colours. Use palettes
   built for each scene, plus ordered dithering where needed.
4. **Asset size.** Isometric sprite sheets for two scenes may be large. Keep
   the embedded set small and put higher-detail packs on SD.
5. **Board variation.** Inversion, touch axes and light-sensor polarity
   differ between clones. Expose them all as settings and calibrate touch on
   first boot.
6. **No Arduino ecosystem.** Drawing, fonts and the touch driver are written
   in-house. Each is small, but budget time for them in Phases 1–2.
