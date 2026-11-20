# CLAUDE.md

Firmware for the ESP32-2432S028R ("Cheap Yellow Display"), C++ on plain
ESP-IDF (no Arduino). `PLAN.md` holds the design and phased roadmap; its
paths are relative to `src/`. `README.md` covers setup, build and flashing.

## Build and verify

- Build only with `./build.sh`, which uses Podman or Docker. Never install
  ESP-IDF or run `idf.py` on the host, and never use `sudo` to build. If no
  container engine is available, say the change is unbuilt; don't guess.
- `./build.sh --target NAME` picks the image: `cydium` (default) is `src/`,
  other names are extra ESP-IDF projects in `src/apps/NAME/` that share
  `src/components/` and `src/sdkconfig.defaults` (e.g. `diagnostic`).
- Only `src/` is in the build context. Everything outside it (README,
  `build/`, scripts) must never affect the firmware build.
- Hardware behaviour (display, touch, timing, colours) can only be checked by
  the user flashing the board. When finishing a change, say what still needs
  checking on the board.

## Code rules

- Style: `.clang-format` (Google, Allman braces, 100 columns). Format with
  `clang-format -i` (`.venv/bin/pip install clang-format` if missing).
- One component per directory in `src/components/`, one C++ `namespace` per
  component, `kConstant` naming, `REQUIRES` kept minimal.
- `gfx`, `sim` and `sims/*` must stay hardware-free (no IDF driver or
  `esp_lcd` headers) so they build and run unit tests on the host.
- Memory: no PSRAM. No single allocation above ~100 KB, and none in the
  render loop. Allocate at `begin()` or boot, never per frame.
- Colours are RGB565 stored byte-swapped (big-endian) for the ILI9341.
- Kconfig `bool` options are undefined when off: use `#ifdef`, never `== 0`.
- ESP-IDF is v6.x (`IDF_VERSION` in `Containerfile`): use `esp_driver_*`
  components, not `driver`. cJSON needs the `espressif/cjson` managed
  component.
- "Walkers" is original: Lemmings-like mechanics only, never its name,
  sprites or levels.

## Git

- Work on feature branches off `main`, and commit only when asked.
- Use Conventional Commits, with the component as scope, e.g.
  `feat(display): add DMA chunk renderer`, `fix(touch): …`.
- Commit each project's `dependencies.lock` (`src/` and `src/apps/*/`).
  Never commit `build/` or `sdkconfig`.
