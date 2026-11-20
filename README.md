# cydium

Ambient pixel-art simulations for the ESP32-2432S028R ("Cheap Yellow
Display"), written in C++ on plain ESP-IDF. See [PLAN.md](PLAN.md) for the
design and roadmap.

**Status:** Phase 2 (renderer), the start of Phase 3 (simulation framework)
and of Phase 4 (menu): indexed layers composited to the panel in dirty 16-row
chunks, two demo simulations chosen from the menu, and display settings
saved in NVS. A
[diagnostic image](#diagnostic-image) helps work out a board's panel and touch
settings, and an [emulator](#emulator) runs the image on a PC.

## Dependencies

The firmware is compiled inside a container, so the ESP-IDF toolchain is not
installed on the host. The host needs:

| Dependency | Needed for |
| --- | --- |
| git | cloning the repo |
| Podman or Docker CE, installed and runnable by your user | building (`./build.sh`) |
| Python ≥ 3.10 and esptool (in `.venv/`) | flashing and the serial console |
| `dialout` group membership | access to `/dev/ttyUSB0` |
| a web browser | the [emulator](#emulator) |

`build.sh` uses whichever container engine is installed. If both are, set
`CONTAINER_ENGINE=docker` or `CONTAINER_ENGINE=podman` to choose.

The commands in [Quick start](#quick-start) are for Enterprise Linux 9
(RHEL, AlmaLinux, Rocky). EL9's default `python3` is 3.9, which is too old
for esptool, so they use `python3.12`.

### Optional: Windows with WSL2

The same steps work inside a WSL2 Enterprise Linux distribution, with two
extras:

1. **Enable systemd**, which the container engine's services expect. Add
   this to `/etc/wsl.conf` in the distribution:

   ```
   [boot]
   systemd=true
   ```

   Then run `wsl --shutdown` from Windows and reopen the distribution.

2. **Pass the board's USB port through** with
   [usbipd-win](https://github.com/dorssel/usbipd-win). In an
   **administrator** PowerShell on Windows:

   ```
   winget install usbipd
   usbipd list                          # find the CH340 (USB-SERIAL) BUSID, e.g. 2-3
   usbipd bind --busid 2-3              # once per device
   ```

   Then, in a normal PowerShell, every time the board is plugged in:

   ```
   usbipd attach --wsl --busid 2-3
   ```

   The board then appears in WSL as `/dev/ttyUSB0`. Use
   `usbipd detach --busid 2-3` to hand it back to Windows.

## Quick start

```
# One-off: install dependencies and allow access to the serial port.
sudo dnf install -y git podman python3.12
sudo usermod -aG dialout "$USER"     # log out and back in afterwards

git clone git@github.com:ChiefSi/cydium.git
cd cydium

# One-off: create a Python environment for esptool.
python3.12 -m venv .venv
.venv/bin/pip install esptool

# Build. The first run builds the ESP-IDF image and takes a while.
./build.sh

# Build, flash and watch the serial log (Ctrl+] to exit).
./build.sh --flash --console
```

## Build

```
./build.sh                      # the main image, cydium, and its emulator
./build.sh --no-build-emulator  # just the main image
./build.sh --target diagnostic  # another image, from src/apps/diagnostic/
./build.sh --test               # run the host unit tests first
./build.sh --help               # all options
```

`build.sh` can be run from any directory. `--target` picks the image:
`cydium` (the default) is the project in `src/`, and any other name is the
project in `src/apps/<name>/`. Each target builds into `build/<target>/`.

This builds `Containerfile` in these stages:

1. **deps**: Ubuntu packages, the ESP-IDF source and its toolchain. This is
   cached, so it only reruns when the `Containerfile` or `IDF_VERSION`
   changes. apt, ESP-IDF tool and pip downloads use cache mounts.
2. **build**: only `src/` is bind-mounted, read-only. Inside the container
   it's copied to a scratch folder and built with `idf.py`, so the host tree
   is never written to. `.dockerignore` limits the build context to
   `src/`, so the layer cache reruns this stage only when something in
   `src/` changes. Edits to the README, `build/` and so on don't trigger it.
3. **artifacts**: exported to `build/<target>/`: `<target>.bin`, `.elf`, `.map`,
   bootloader, partition table, `flash_args`, the resolved `sdkconfig` and
   `dependencies.lock`. The files are owned by your user.
4. **emulator** (cydium target only, skipped with `--no-build-emulator`):
   the same `src/` built for ESP-IDF's `linux` target, exported to
   `build/cydium/emulator/cydium-emulator`. See [Emulator](#emulator).

The toolchain is ESP-IDF **v6.1.1**, pinned by `IDF_VERSION` in
`Containerfile`. Managed components are declared in `src/main/idf_component.yml`
and `src/components/*/idf_component.yml`. A project's first build writes its
`dependencies.lock` (`src/dependencies.lock`, or
`src/apps/<name>/dependencies.lock`); commit it.

Options: `IDF_VERSION=v6.0.3 ./build.sh`, `OUT_DIR=out ./build.sh` (relative
to the current directory), and any unrecognised arguments go to the container
engine's `build` command (e.g. `./build.sh --no-cache`).

## Flash and console

```
./build.sh --flash                       # build, then flash
./build.sh --console                     # build, then open the serial console
./build.sh --flash --console             # build, flash, then open the console
./build.sh --no-build --console          # just open the console
./build.sh --no-build --flash            # reflash the existing build
./build.sh --target diagnostic --flash --console --port /dev/ttyUSB1
```

`--flash` runs esptool and `--console` runs pyserial's miniterm, both from
`.venv/` (exit the console with Ctrl+]). The image is built first unless
`--no-build` is given, which uses the existing output in `build/<target>/`. The port defaults to `/dev/ttyUSB0`
(or `$PORT`), and the baud rate to 115200 (or `$BAUD`): some CH340 USB chips
on these boards fail at higher rates.

To flash by hand, run esptool from the output folder so the relative paths
in `flash_args` resolve:

```
cd build/cydium
../../.venv/bin/python -m esptool --chip esp32 -p /dev/ttyUSB0 -b 115200 write-flash @flash_args
```

If `/dev/ttyUSB0` gives "permission denied", check that you're in the
`dialout` group (see [Quick start](#quick-start)).

## Host unit tests

```
./build.sh --test             # tests, then build
./build.sh --test --no-build  # tests only
```

The hardware-free code has unit tests in `src/test/`: `gfx` and the touch
mapping, the simulation framework and each simulation, in C++ with Unity (from ESP-IDF), and the PNG converter in Python.
They build and run in the container's `test` stage. A cached result means
nothing in `src/` has changed since they last passed.

## Running cydium

The main image runs one simulation at a time, chosen on the menu's
Simulation page and restored at boot:

- **Bounce** (renderer test): a gradient and grid on the background layer,
  with bouncing balls on the foreground layer. Only the 16-row chunks the
  balls touch are sent to the panel. A tap moves the red ball there.
- **Starfield:** stars drifting past in three parallax layers. A tap toggles
  warp speed.
- **Walkers:** small walkers drop from a hatch and have to reach the door.
  Each level is generated at random, with walls, gaps, deep drops, metal
  and floors to dig through, and the bar at the bottom gives five skills:
  digger, builder (a staircase), exploder (3 s fuse, also the only way to
  stop a walker's skill), blocker and umbrella (an add-on for slow falls).
  The level banner says how many walkers must be saved out of how many.
  - An autopilot plays by default, like a player working the level out: it
    pauses to look at new obstacles, makes the odd mistake and gets better
    on retries. Every level is played through by the autopilot before it is
    shown, so each one can be won.
  - Touch to take over: tap a skill button, then a walker. The status line
    shows `YOU`; the autopilot takes back over after 20 s without a touch.
  - A lost level offers Retry or New (the autopilot picks one if you
    don't). Rotating the screen starts a new level.

Taps away from the menu icon go to the simulation. Every 5 seconds the serial log shows
`N fps, N chunks/frame, heap free=… largest=…`, and each tap is logged with
its raw and screen coordinates.

### Menu

Tap the menu icon (top left by default), or press the BOOT button, to open
the menu over the dimmed scene:

- **Simulation:** the simulations, with the running one highlighted. Tap one
  to switch to it; the menu closes.
- **About:** the product name (`CONFIG_CYD_PRODUCT_NAME`, default `cydium`)
  and the version.
- **Settings > Display:**
  - **Invert colours** (checkbox);
  - **Orientation:** Portrait or Landscape;
  - **Menu icon:** which corner the icon sits in.
- **Settings > Walkers > Difficulty:** Easy (unlimited skills), Medium (a
  few spare skills) or Hard (one spare of each). Applies from the next level.

Tap a checkbox to toggle it and a choice to cycle it. Changes apply at once
and are saved in NVS, so they survive a reboot. `<` goes back; `x`, a tap
outside the panel or the BOOT button closes; and the menu closes by itself
after 10 seconds without a touch.

The defaults and the icon come from Kconfig. To change them, add the options
to `src/sdkconfig.defaults` (shared by both images) and rebuild:

| Option | Default | Meaning |
| --- | --- | --- |
| `CONFIG_CYD_MENU_ICON_SIZE` | `32` | icon width and height, in pixels |
| `CONFIG_CYD_MENU_ICON_OFFSET` | `4` | gap between the icon and the screen edges |
| `CONFIG_CYD_MENU_ICON_PNG` | `"icons/menu.png"` | icon image, relative to `src/components/ui/` |
| `CONFIG_CYD_MENU_ICON_TOP_LEFT` (or `_TOP_RIGHT`, `_BOTTOM_LEFT`, `_BOTTOM_RIGHT`) | top left | default icon corner |
| `CONFIG_CYD_MENU_TIMEOUT_S` | `10` | auto-close delay; `0` never closes |
| `CONFIG_CYD_LCD_INVERT_DEFAULT` | off | default for Invert colours |
| `CONFIG_CYD_DISPLAY_DEFAULT_LANDSCAPE` | off (portrait) | default orientation |
| `CONFIG_CYD_DISPLAY_LANDSCAPE_USB_LEFT` | off (USB on the right) | which way landscape turns |
| `CONFIG_CYD_PRODUCT_NAME` | `"cydium"` | name at the top of the About page |
| `CONFIG_CYD_VERSION_CUSTOM` + `CONFIG_CYD_VERSION_STRING` | generated | fixed version string |

The icon PNG must be inside `src/`, because the container build can see
nothing else. It is scaled to the icon size at build time, and transparency is
kept. Non-interlaced 8-bit greyscale, RGB, RGBA and palette PNGs are supported.
`src/components/ui/tools/make_menu_icon.py` regenerates the default icon.

The Kconfig defaults only apply until a setting is changed in the menu. To go
back to them, erase the NVS partition:

```
.venv/bin/python -m esptool -p /dev/ttyUSB0 erase-region 0x9000 0x6000
```

### Version

By default, the About page shows `<git describe>[-dirty]-<build time>`, with
the build time in seconds since the Unix epoch (UTC), e.g.
`7973efb-dirty-1795189980`. `build.sh` runs `git describe --tags
--always` on the host, because the container can't see `.git`, and appends
`-dirty` when `git status --porcelain` lists any change, including untracked
files. Set `CONFIG_CYD_VERSION_CUSTOM=y` and `CONFIG_CYD_VERSION_STRING="…"`
to use a fixed string instead.

## Emulator

```
./build.sh                                  # builds it alongside the firmware
build/cydium/emulator/cydium-emulator       # then open http://127.0.0.1:8240/
build/cydium/emulator/cydium-emulator --addr 0.0.0.0 --port 9000
```

The emulator is the cydium image (`app_main`, settings, menu, simulations)
built for ESP-IDF's `linux` target, so it runs on FreeRTOS's POSIX port as a
normal program. Only the hardware is swapped out:

- **Display:** the screen is shown on a local web page. Rotation, colour
  inversion and the backlight work as on the board.
- **Touch:** press the mouse on the screen. The press is turned into the raw
  XPT2046 reading the Kconfig calibration expects there, so it goes through
  the same mapping as a real touch.
- **BOOT button:** the button under the screen, or hold the `B` key.
- **Flash:** NVS settings are kept in
  `~/.local/state/cydium/emulator-flash.bin` (or `$XDG_STATE_HOME/cydium/`),
  so they survive restarts. Delete the file to reset them.

It is statically linked, so it runs on hosts with an older C library than
the build container's (such as EL9), and needs nothing else installed. Stop
it with Ctrl+C. Under WSL2, open the address in a Windows browser.

| Option | Environment variable | Default | |
| --- | --- | --- | --- |
| `-a`, `--addr ADDR` | `CYD_EMULATOR_ADDR` | `127.0.0.1` | IPv4 address to listen on; `0.0.0.0` exposes it to the network |
| `-p`, `--port PORT` | `CYD_EMULATOR_PORT` | `8240` | port for the web page |
| | `CYD_EMULATOR_FLASH` | see above | flash image file |

Options override the environment. `--help` lists them.

The emulator isn't timing-accurate: frame rate and SPI bandwidth are
whatever the PC manages, and the heap figures in the log are meaningless.
Check display, touch and timing behaviour on the board.

## Diagnostic image

CYD boards differ between batches in panel orientation, red/blue order and
colour inversion. The diagnostic image shows which settings your board needs:

```
./build.sh --target diagnostic --flash --console
```

Hold the board in portrait with the USB port at the bottom. The screen starts
black. Each touch draws the next 40×40 block, numbered 0–47, filling the screen
left to right and top to bottom in 6 columns × 8 rows. Neighbouring blocks are
red, green and blue, and the serial log names the colour of each one. Press
**BOOT** to toggle colour inversion.

| What you see | Setting to change |
| --- | --- |
| A strip of old image or noise that never gets cleared | `CONFIG_CYD_LCD_SWAP_XY` |
| Numbers mirrored left to right, or block 0 at the top right | `CONFIG_CYD_LCD_MIRROR_X` |
| Numbers upside down, or block 0 at the bottom | `CONFIG_CYD_LCD_MIRROR_Y` |
| Blocks logged as red look blue, and vice versa | `CONFIG_CYD_LCD_BGR` |
| The crosshair isn't under the stylus | `CONFIG_CYD_TOUCH_*` (see below) |
| Black background looks white; inverting with BOOT fixes it | `CONFIG_CYD_LCD_INVERT_DEFAULT` |

The defaults are `SWAP_XY`, `MIRROR_X` and `MIRROR_Y` on, `BGR` and
`INVERT_DEFAULT` off. To change one, add the opposite setting to
`src/sdkconfig.defaults` (for example `# CONFIG_CYD_LCD_MIRROR_X is not set`
to turn it off, or `CONFIG_CYD_LCD_BGR=y` to turn it on). Then rebuild both
images, because the diagnostic image also reads that file. At boot, the log
also shows the LCD controller's ID registers (`display: probe: reg …`).

### Touch calibration

The diagnostic image starts by calibrating touch. It shows four yellow
targets, one at a time, 20 px in from each corner, and you tap each one
(holding briefly is fine; the reading is averaged). It then fits a calibration
and prints it as Kconfig lines:

```
CONFIG_CYD_TOUCH_X_MIN=…
CONFIG_CYD_TOUCH_X_MAX=…
…
```

The block test then uses the new calibration, so you can check that each
press's white crosshair lands under the stylus. While the screen is held, the
log shows `raw x=… y=… -> screen x=… y=…`. To make the calibration permanent,
paste the printed lines into `src/sdkconfig.defaults` and rebuild. Both images
read that file.

| Option | Default | Meaning |
| --- | --- | --- |
| `CONFIG_CYD_TOUCH_X_MIN`, `_X_MAX` | `261`, `3843` | raw X at its two ends |
| `CONFIG_CYD_TOUCH_Y_MIN`, `_Y_MAX` | `196`, `3847` | raw Y at its two ends |
| `CONFIG_CYD_TOUCH_SWAP_XY` | off | raw X runs along the long (320 px) edge |
| `CONFIG_CYD_TOUCH_FLIP_X` | on | screen x is mirrored (raw X falls left to right) |
| `CONFIG_CYD_TOUCH_FLIP_Y` | off | screen y is mirrored |

The defaults come from running the calibration on the board this was
developed on: in portrait, raw X falls from left to right and raw Y rises from
top to bottom. Other boards may differ, so run the calibration if touches land
in the wrong place.

The calibration is for portrait; landscape is worked out from it.

## Layout

```
build.sh              builds in a container (artefacts to build/<target>/),
                      optionally flashes and opens the serial console
Containerfile         toolchain image and build stages
.dockerignore         restricts the build context to src/
.clang-format         C++ style (Google, Allman braces, 100 columns)
CLAUDE.md             guidance for Claude Code
PLAN.md               design and roadmap
src/                  the ESP-IDF project (the only thing the build sees)
  CMakeLists.txt
  partitions.csv
  sdkconfig.defaults
  main/               app_main.cpp (init order, tasks, simulation list)
  apps/
    diagnostic/       diagnostic image (./build.sh --target diagnostic)
  components/
    board/            pin map, backlight PWM, BOOT button, light sensor
    buildinfo/        product name and version (Kconfig or git describe + time)
    display/          esp_lcd ILI9341 setup, orientation, Renderer (layers,
                      palette, dirty chunks, overlays)
    emulator/         linux target only: web page for the screen, mouse as
                      touch, flash file (board, display and touch have
                      *_linux.cpp variants that use it)
    gfx/              hardware-free: layers, canvases, palette, 8x8 font
    settings/         bool/enum settings registry saved in NVS
    sim/              hardware-free: Simulation interface, Manager, Surface,
                      Rng
    sims/             one component per simulation (bounce, starfield,
                      walkers)
    touch/            bit-banged XPT2046, Kconfig calibration
    ui/               menu overlay, icon PNG and converter
  test/               host unit tests (./build.sh --test)
```

Further components (`storage`, `timesync`, …) arrive in
`src/components/` in later phases, as described in PLAN.md (whose paths are
relative to `src/`).
