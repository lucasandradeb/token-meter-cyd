# Contributing

Thanks for taking a look. This is a small, hobby-scale open-source project and
contributions are welcome — bug reports, docs fixes, new features, or just
sharing that you got it running on your own board.

## Ways to help

- **Report a bug or ask a question** — open an
  [issue](https://github.com/lucasandradeb/token-meter-cyd/issues). Include your
  board variant, OS, and the serial log if it's a firmware problem.
- **Improve the docs** — typos, unclear steps, or a gotcha you hit that others
  will too. Small doc PRs are the easiest to merge.
- **Add a feature or fix** — see the open items in the
  [Roadmap](README.md#roadmap) or propose your own in an issue first.

## Project layout

See [Repository layout](README.md#repository-layout). In short:

- `firmware/` — PlatformIO project. Two environments: `bringup` (isolated
  display + touch test) and `app` (full: LVGL + touch + BLE).
- `daemon/` — Python daemon (macOS Keychain / Linux credentials file) that reads
  usage and pushes it over BLE.
- `tools/` — `img_to_c.py` for the custom top image.

## Development setup

### Firmware

Install **VSCode + the PlatformIO extension** (auto-recommended when you open the
repo), or use the CLI:

```bash
cd firmware
pio run -e bringup            # build the hardware test first
pio run -e bringup -t upload  # flash
pio device monitor            # logs @ 115200
```

Always get `bringup` working before touching `app`. If the screen is white or
colors are wrong, that's a display-driver mismatch — see
[Bring-up](README.md#2-bring-up-do-this-first).

### Daemon

```bash
cd daemon
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
python daemon.py
```

## Before you open a PR

- **Build the firmware for real** — run `pio run -e app` (and `-e bringup` if you
  touched shared code) and confirm it compiles before claiming it works.
- **Keep the diff focused** — one logical change per PR.
- **Follow the existing style** — match the surrounding code; comments and docs
  can be Portuguese or English (both exist in the repo).
- **Write a clear PR description** — what changed and *why*. Screenshots or a
  photo of the board help a lot for anything visual.

## Hardware notes (please don't regress these)

This board has hard-won quirks. If you touch the firmware, keep these in mind —
they're documented in more detail in [`CLAUDE.md`](CLAUDE.md):

- **Toolchain is Arduino-ESP32 core 2.x.** Some library versions are pinned
  because 3.x breaks the build (`GFX Library for Arduino@1.4.9`,
  `NimBLE-Arduino@^1.4.1`). Backlight PWM uses the core-2.x LEDC API.
- **Display and touch are on separate SPI buses.** Sharing the bus breaks touch.
- **No PSRAM** — keep static `.bss` small; the LVGL draw buffer lives on the heap.
- **BLE callback runs on another task** — never call LVGL from it (LVGL is not
  thread-safe).

## What not to commit

- **No secrets** — OAuth tokens, Wi-Fi credentials, device credentials. These are
  gitignored; keep it that way.
- **No third-party art or fonts** — the repo is public and MIT. User-supplied top
  images are local-only and gitignored (`party_img_user.c/.h`). Ship only your
  own original assets.

## License

By contributing you agree your contributions are licensed under the
[MIT License](LICENSE).
