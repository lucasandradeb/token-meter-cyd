# Setup guide — from a fresh board to a working meter

Step-by-step for anyone with the **same board** (ESP32-2432S028 / "CYD") to run
this project and reach the same state: the board shows your live Claude Code
usage, driven by a daemon on your Mac over Bluetooth.

Estimated time: ~30 min (first PlatformIO build downloads the ESP32 toolchain).

## 0. What you need

- An **ESP32-2432S028 (CYD)** board — 2.8" 320×240, resistive touch, micro-USB.
- A **micro-USB data cable** (must carry data, not power-only).
- A **Mac** logged into **Claude Code** (the daemon reads its token). Linux works
  too — see `daemon/README.md`.
- **VSCode**, **Python 3.11+**, and **git**.

## 1. Install the tools

1. Install [VSCode](https://code.visualstudio.com/).
2. Install the **PlatformIO IDE** extension (VSCode will suggest it when you open
   this repo, or install `platformio.platformio-ide` from the Extensions panel).

## 2. Get the code

```bash
git clone https://github.com/lucasandradeb/token-meter-cyd.git
cd token-meter-cyd
```

Open the folder in VSCode and let PlatformIO finish initializing (bottom bar
stops showing activity).

> PlatformIO ships its own `pio` at `~/.platformio/penv/bin/pio`. The commands
> below use plain `pio` — either add that path to your shell or use the VSCode
> PlatformIO buttons (Build / Upload / Serial Monitor) instead.

## 3. Plug in the board and find its port

Connect the board by USB. Find the serial port:

```bash
ls /dev/cu.usbserial-*        # macOS (CH340), e.g. /dev/cu.usbserial-110
```

PlatformIO usually auto-detects it, so you can omit `--upload-port` below.

## 4. Bring-up: prove the hardware works (do this first)

```bash
cd firmware
pio run -e bringup -t upload
pio device monitor -e bringup      # 115200 baud
```

You should see color bars + "CYD bring-up OK" on the screen, and touching it
prints raw coordinates on the monitor.

- **White/blank screen?** Wrong driver. Edit `firmware/src/bringup/main.cpp`,
  set `DISPLAY_DRIVER` to `1` (ST7789), re-upload.
- **Colors look inverted?** Adjust the `ips` parameter in the same file.

(These same settings are already applied in the `app` firmware; note whichever
worked for your board.)

## 5. Flash the full firmware

```bash
pio run -e app -t upload
```

The screen shows the top image, two cards (Session / Weekly), and a footer
reading "Waiting…" (no data source yet — that's the daemon, next).

## 6. Calibrate the touch

On boot, **touch the screen within 2 seconds** to enter calibration (the first
boot enters it automatically). Tap the center of each of the 4 corner targets.
When it finishes, a dot follows your finger — then it saves the calibration to
flash (NVS) and won't ask again. To recalibrate later, touch during that 2s
boot window.

## 7. Set up the daemon (your Mac)

```bash
cd ../daemon
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
python daemon.py
```

- Make sure you're logged into **Claude Code** first (the daemon reads the token
  from the macOS Keychain, service `Claude Code-credentials`).
- On first run, macOS asks for **Bluetooth** permission — grant it (Settings >
  Privacy & Security > Bluetooth).

Within a few seconds the daemon finds `TokenMeter`, connects, and the board
switches to "Connected" and shows your real usage, updating every ~30s.

## 8. Autostart the daemon (so it just works)

So the daemon runs on login and reconnects automatically:

- **macOS (launchd)** and **Linux (systemd)**: follow `daemon/README.md`. On a
  Raspberry Pi / always-on host, this is what lets the board work without your
  Mac present.

## 9. (Optional) Use your own image on top

```bash
python tools/img_to_c.py path/to/your-image.png     # PNG/WEBP/JPG/GIF
cd firmware && pio run -e app -t upload
```

The image is resized to fit (~300×62) and stays **local only** (gitignored). Use
only images you have the right to use.

## Troubleshooting

| Symptom | Fix |
|---|---|
| White/blank screen | Wrong display driver — see step 4 (`DISPLAY_DRIVER`). |
| Wrong colors | Toggle `ips` — see step 4. |
| Touch off / mirrored | Recalibrate (touch during the 2s boot window). |
| Footer stuck on "Waiting…" | Daemon not running, out of BLE range, or Bluetooth permission denied. |
| Board not found by daemon | Board powered? Same room (~10 m)? Bluetooth on? |
| First build seems stuck | It's downloading the ESP32 toolchain — wait it out. |

## How it all fits

See the [README](../README.md) for the architecture, the board gotchas in
[CLAUDE.md](../CLAUDE.md), and the design docs in `docs/specs/`.
