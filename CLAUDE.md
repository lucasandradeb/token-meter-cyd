# CLAUDE.md — Token Meter (ESP32 CYD)

Directives for Claude Code working in this repo. Read before making changes.

## What this is

A physical Claude Code token-usage meter on an **ESP32-2432S028 ("CYD")** board.
A Mac/Linux daemon reads usage from the Claude OAuth rate-limit headers and
pushes `{session,weekly,s_reset,w_reset}` over **BLE** to the board, which shows
it with LVGL. Inspired by Clawdmeter; original code; MIT.

## Layout

- `firmware/` — PlatformIO. Envs: `bringup` (display+touch test) and `app`
  (full: LVGL + touch + BLE). Source in `firmware/src/{bringup,app}/`.
- `daemon/` — Python daemon (macOS Keychain or Linux `~/.claude/.credentials.json`)
  + launchd/systemd units.
- `tools/img_to_c.py` — converts an image to the LVGL top image.
- `docs/specs/` — design + the WiFi-standalone plan (not implemented).
- `docs/CONTEXT.md` — project state / where to resume.

## Build / flash / run (this machine)

PlatformIO Core lives in the extension venv; there is no global `pio`:

```bash
PIO=~/.platformio/penv/bin/pio
cd firmware
$PIO run -e app                          # build
$PIO run -e app -t upload --upload-port /dev/cu.usbserial-110   # flash
$PIO device monitor -e app               # serial (115200)
```

Board serial port on this Mac: `/dev/cu.usbserial-110` (CH340). The same venv's
Python has `bleak`/`httpx`/`Pillow` for the daemon/tools.

Daemon: `daemon/` → venv + `pip install -r requirements.txt` → `python daemon.py`
(or launchd/systemd; see `daemon/README.md`).

## Board gotchas (hard-won — do not regress)

- **Toolchain is Arduino-ESP32 core 2.x** (stock `platform = espressif32`,
  `board = esp32dev`). Consequences, all load-bearing:
  - `moononournation/GFX Library for Arduino@1.4.9` **pinned** — 1.5+ needs core
    3.x (`esp32-hal-periman.h`).
  - `h2zero/NimBLE-Arduino@^1.4.1` — 2.x needs core 3.x.
  - Backlight PWM uses the **core-2.x LEDC API**: `ledcSetup` + `ledcAttachPin`
    + `ledcWrite(channel, …)` (not the 3-arg `ledcAttach`).
- **Display driver: ILI9341 with `ips=true`** — this panel ships color-inverted;
  `ips=false` shows wrong colors. Confirmed on hardware.
- **No RGB565 byte-swap in the LVGL flush** — ESP32 is little-endian and
  Arduino_GFX reads the buffer natively. Swapping breaks colors.
- **Display and touch are on separate SPI buses.** Touch (XPT2046) uses its own
  `SPIClass(VSPI)`. Sharing the display bus = no touch.
- **No PSRAM.** Keep static `.bss` small (dram0_0_seg cap ~180 KB). The LVGL
  draw buffer is allocated on the **heap** (`display.cpp`), not a static array;
  big static arrays overflow the segment.
- **Partition: `huge_app.csv`** — LVGL + NimBLE don't fit the default 4 MB app
  slot.
- **Touch calibration** is a 4-corner routine with axis-swap detection,
  persisted in NVS (`Preferences`, namespace `touchcal`). Re-calibrate by
  touching the screen within 2s of boot.
- Pins: display SCK14/MOSI13/MISO12/CS15/DC2/BL21; touch CLK25/MOSI32/MISO39/
  CS33/IRQ36. See `firmware/src/app/pins.h`.

## Data / protocol

- Daemon → board BLE GATT: service `c0de0001-…`, writable char `c0de0002-feed-
  4d61-9a11-0123456789ab`, JSON `{"session":NN,"weekly":NN,"s_reset":S,"w_reset":S}`.
- Usage headers are **fractions 0..1** — multiply by 100 for percent.
- BLE callback runs on another task; **never call LVGL from it** (LVGL is not
  thread-safe). It stores volatile state; the loop consumes it.

## IP / assets policy (important)

- **Never commit third-party art/characters** (e.g. the Clawd mascot, LOTR
  pixel-art). The repo is public + MIT.
- User-supplied top images are **local only and gitignored**
  (`party_img_user.c/.h`). `tools/img_to_c.py` generates them; the user runs it
  on their own asset. The repo ships only original default art (`party_img.c`).
- Do not embed/convert copyrighted assets into the firmware yourself; provide the
  tooling and let the user do it locally.

## Conventions

- No emojis. Comments/docs may be Portuguese (as in the codebase) or English.
- Verify builds with a real `pio run` before claiming success.
- Secrets (OAuth tokens, WiFi creds) never in the repo or logs; all such files
  are gitignored.
```
