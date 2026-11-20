# Review to-do

Open items from the work on 2026-11-20: the simulation framework, the
emulator, the BOOT button fix and Walkers. Tick items off as they are done.

## Git

- [ ] `feat/walkers` (`c1f3a23`) is committed but not merged to `main` or
      pushed.
- [ ] Review the merged work. `gh` isn't installed, so these went to `main`
      by local `--no-ff` merges with no pull request:
  - [ ] simulation framework: `db8706f` (merge `4e6f6bd`)
  - [ ] emulator: `385d4c7` (merge `7a42df0`)
  - [ ] BOOT button fix: `ee9e762` (merge `24cf29e`)
  - [ ] Walkers: `c1f3a23` (not merged yet)

## Check on the board

Only the board can show display, touch and timing behaviour; the emulator
isn't timing-accurate.

- [ ] Simulations: switching between Bounce, Starfield and Walkers redraws
      cleanly with the right palette.
- [ ] The chosen simulation survives a reboot.
- [ ] Rotating while each simulation runs restarts it correctly (Walkers
      starts a new level).
- [ ] Starfield: frame rate in the serial log (most chunks are dirty), and a
      tap toggles warp.
- [ ] Touch and display still behave normally after the emulator refactor
      (code moved into `display/orientation.cpp` and
      `touch/calibration.cpp`; the input task's core is now chosen in code).
- [ ] BOOT opens and closes the menu reliably, with no double triggers.
- [ ] Walkers: the pause when a level starts while the autopilot checks it
      (expected well under 100 ms).
- [ ] Walkers: frame rate with many walkers on screen.
- [ ] Walkers: picking a walker by touch on the resistive panel (nearest
      within 12 px).
- [ ] Walkers: the 10 px sprites are readable on the panel.
- [ ] Heap in the serial log while Walkers runs.

## Check in a browser

- [ ] Emulator page: clicks, the BOOT button and `B` key, zoom, backlight
      and invert. So far it has only been driven with `curl`.
- [ ] Under WSL2, open the page from a Windows browser.

## Decisions to confirm

- [ ] Walkers tuning: splat height 56 px, logic at 20 Hz, spawn every
      2–3 s, 10–20 walkers per level.
- [ ] Walkers level ends: early as soon as the target can't be reached;
      after 60 s without progress once all are out; and after 8 minutes.
- [ ] Walkers art and palette (original sprites; blue clothes, random hair).
- [ ] The Walkers Difficulty default (Medium) is hard-coded in
      `settings.cpp`; other setting defaults come from Kconfig.

## Known limitations and follow-ups

- [ ] The menu's Simulation page doesn't scroll: landscape fits 5 rows, so a
      6th simulation needs paging (TODO in `ui/menu.cpp`).
- [ ] Walkers: landscape levels have only about 3 floors, so they are simpler
      than portrait ones.
- [ ] Walkers: about 2% of generated levels fail the autopilot check and are
      skipped. If 64 seeds in a row failed, an unchecked level would be
      shown.
- [ ] Walkers: no random events yet (PLAN Phase 5).
- [ ] Walkers: when the autopilot picks Retry or New after a lost level, it
      doesn't show a cursor for it.
- [ ] Emulator: IDF's `linux` target is still a preview feature.
- [ ] Emulator workarounds that depend on IDF internals; recheck on an IDF
      upgrade:
  - [ ] `emulator/static_link.cpp` stands in for `dlsym(RTLD_NEXT, ...)`
        for `ftruncate` (esp_partition) and `select` (FreeRTOS).
  - [ ] `src/CMakeLists.txt` strips the `--coverage` that `nvs_flash` adds
        on linux.
  - [ ] `emulator/emulator.cpp` provides `esp_timer_get_time()`, which IDF
        leaves out on linux.
- [ ] Emulator: heap figures in its log are meaningless.
