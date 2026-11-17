# Bruce 1.17 (mainline) for Heltec WiFi LoRa 32 V4 + M5Stack Cap CC1101

**This branch is a fork of mainline [BruceDevices/firmware](https://github.com/BruceDevices/firmware) (v1.17)** with one added board, `heltec-v4-oled`, that:

- Renders the **full Bruce UI natively on the Heltec's onboard 128×64 SSD1306 OLED** (monochrome, via a ported `UI_COMPACT` compact layout).
- Drives an **externally-wired M5Stack Cap CC1101** for Sub-GHz (incl. the Cap's antenna switch).
- Is navigated with the Heltec's **single PRG button**.

> **Why mainline?** The 128×64 compact UI originally existed only in the fork
> [Yoursel71/BruceButBetter](https://github.com/Yoursel71/BruceButBetter), which is pinned at **Bruce v1.5.0** — ~3,485 commits / 12 releases behind. That staleness caused real, already-fixed bugs. This port lifts BruceButBetter's 128×64 capability onto **current mainline 1.17** so you get the OLED UI *and* current Bruce.

Upstream's original README is preserved as [`README.upstream.md`](./README.upstream.md).

---

## ⚠️ Build requires a submodule patch — read this first

The critical display fix lives in the **`lib/DisplayDrivers` submodule** (upstream: bmorcelli/DisplayDrivers), which we can't push to. It is captured as a patch. **After cloning, you MUST apply it or the OLED renders as garbled speckle:**

```bash
git submodule update --init --recursive
git -C lib/DisplayDrivers apply ../../patches/displaydrivers-tft-monochrome.patch
```

(The patch applies on DisplayDrivers SHA `79d2dc757d0684a3f63ab027920edd152972f93f`. See "The monochrome fix" below for what it does and why.)

---

## Hardware

| | |
|---|---|
| **Host** | Heltec WiFi LoRa 32 **V4** — ESP32-S3**R2** (16 MB flash, **2 MB QSPI** PSRAM), onboard **SSD1306 128×64 I²C OLED**, single **PRG** button on GPIO0. Board under test MAC `44:1b:f6:70:a2:28`. |
| **Radio** | **M5Stack Cap CC1101** (SKU U219) — TI CC1101 Sub-GHz (315/433/868/915 MHz) + ST25R3916 NFC, 14-pin Cap-Bus, SPI. |
| **Power** | USB-C (Cap fed from the Heltec 5V header — assembly is USB-tethered). |

### Heltec → M5 Cap CC1101 wiring (the authoritative pinout)

| M5 Cap pad | Heltec pin | Function |
|---|---|---|
| `5V OUT` | 5V (J2-2) | Supply into the Cap's regulator |
| `GND` | GND (J2-1) | Ground |
| `SCK` | **GPIO33** | SPI clock |
| `MOSI` | **GPIO47** | SPI MOSI |
| `MISO` | **GPIO48** | SPI MISO |
| `CC CS` | **GPIO39** | **CC1101 chip-select** (see note) |
| `GDO0` | **GPIO4** | CC1101 GDO0 |
| `RF SW0` | **GPIO38** | Antenna-switch control line 0 |

- **CC1101 CS is GPIO39.** The Cap's silkscreen swaps the `CC CS`/`NFC CS` labels vs the schematic; GPIO39 is the real CC1101 select (verified by reading VERSION=0x14).
- **`RF SW1` is not wired** — it's the CC1101's own GDO2, forced in firmware via `IOCFG2` (mainline 1.17 already has this code).
- **OLED (onboard):** I²C `SDA=GPIO17`, `SCL=GPIO18`, `RST=GPIO21`, panel power via **`Vext=GPIO36` (active LOW)** — Vext is enabled in `interface.cpp` before the display inits.
- **Disabled to avoid pin conflicts:** GPS (would claim 38/39) and Grove I²C (SCL=4). GPS and the Cap can't coexist on this wiring; the Cap wins.

---

## How to orient in this codebase (for future agents)

Everything specific to this board is in **`boards/heltec-v4-oled/`** plus a set of `#if UI_COMPACT`-gated additions to Bruce core. Nothing here regresses other boards — all compact code is behind `UI_COMPACT`, which only this board defines.

### Files that make up the board
- `boards/heltec-v4-oled/heltec-v4-oled.ini` — the env. `board = esp32s3`, `qio_qspi` (R2 PSRAM), `custom_16Mb.csv`. All pins are `-D` flags (mainline convention; no per-board `pins_arduino.h`). Defines `USE_LOVYANGFX` + `Panel_SSD1306` mono, the CC1101/Cap pins, `CAP_CC1101_SS_PIN=39`/`SW0_PIN=38`, single-button flags, **`-DUI_COMPACT=1`** + the compact layout macros (`STATUS_BAR_HEIGHT=14`, `BORDER_PAD_*`, `FP/FM/FG=1/1/2`), **`-DTFT_MONOCHROME=1`**, **`-DROTATION=0`**, and `lib_ignore = TFT_eSPI FastLED`.
- `boards/heltec-v4-oled/interface.cpp` — `_setup_gpio()` (Vext power, I²C bring-up, CC1101 pin seed + default `rfModule`) and `InputHandler()` (single-button gesture → nav globals). Mainline board HAL uses weak defaults; only these two are needed.
- `boards/heltec-v4-oled/ok_button.h` — the single-PRG-button gesture engine: **tap = Advance (DownPress), hold ≈650 ms = Select, double-tap = Back**. `DoubleMs=200` (tuned for faster cycling).

### The `UI_COMPACT` compact UI (ported from BruceButBetter)
Makes the full Bruce UI fit 128×64: 4×2 icon grid main menu, 4-row lists, compact status bar + footer. All gated `#if UI_COMPACT`.
- `src/core/menu_items/compact_icons.{cpp,h}` — the vector-drawn (not bitmap) icon set. **New files.**
- `src/core/display.cpp` / `display.h` — compact helpers (`drawCompactList`, `drawStatusBarCompact`, `drawCompactFooter`, `drawIconGrid`, `iconGridX/Y`, `iconGridMove`, `compactFrameBegin/End`) + `#if UI_COMPACT` branches grafted into `loopOptions`, `drawStatusBar`, `drawSubmenu`, `drawOptions`, `progressHandler`, `displayScrollingText`, `drawMainBorder`; grid geometry macros (`ICON_GRID_*`, `GRID_FRAME_*`, `CMENU_*`).
- `include/MenuItemInterface.h` — `compactIcon()` virtual + `drawGridIcon()`; `draw()` routes to `drawCompactMainGrid()` under `UI_COMPACT`.
- `src/core/main_menu.{h,cpp}` — `drawCompactGrid` / `drawCompactGridCell` / `drawCompactMainGrid`; main-menu options get a hover lambda that renders the compact grid.
- 17 `src/core/menu_items/*Menu.h` — each adds a `compactIcon()` override (WiFi, BLE, RF, IR, …).

### The monochrome fix (the hard-won one) — `patches/displaydrivers-tft-monochrome.patch`
mainline's `DisplayDrivers` LovyanGFX backend **ignored `TFT_MONOCHROME`**, so every non-pure-white color (the theme color, greys, the boot splash) was dithered into speckle on the 1-bit panel — garbling icons and the splash while leaving 1-bit bitmaps (like the old full-size bat logo) clean. The patch adds BruceButBetter's `monoColor()` snap (`TFT_COLOR()` wrapped around the color arg of every primitive: drawPixel/line/rect/roundRect/circle/arc/triangle/ellipse/fillScreen/setTextColor, etc.; `pushImage`/bitmaps left alone) + public `startWrite/endWrite` for frame batching. **This is the fix that made the UI legible.**

---

## Build & flash

```bash
# One-time after clone:
git submodule update --init --recursive
git -C lib/DisplayDrivers apply ../../patches/displaydrivers-tft-monochrome.patch

# Build:
pio run -e heltec-v4-oled          # or: uvx --from platformio pio run -e heltec-v4-oled   (~2-4 min)

# Flash (board on USB):
pio run -e heltec-v4-oled -t upload
```
- Toolchain: PlatformIO + the pinned pioarduino platform (see root `platformio.ini`).
- **macOS port quirk:** the ESP32-S3 native USB re-enumerates between the app port (`/dev/cu.usbmodem<serial>`) and the ROM-bootloader port (`/dev/cu.usbmodem1101`) across resets. Retry on whichever `/dev/cu.usbmodem*` is present; `--connect-attempts 15` helps.

### Verify without looking at the screen
```bash
# Serial CLI (115200, open with DTR asserted, '#' prompt):
rf rx -frequency 433.92     # -> "cc1101 Connection OK"  (confirms Cap/CC1101)
stopscan
```
Flash/RAM as of this writing: **Flash 24.6%** (~4.12 MB), **RAM 39.2%**.

---

## Using it

- **On-device (single PRG button):** tap = next, hold ≈0.65 s = select/enter, double-tap = back. Main menu is a 4×2 icon grid; enter for lists.
- **Serial CLI** (USB): `help`, `rf rx -frequency <MHz>`, `rf scan <startHz> <stopHz>`, `stopscan`.
- **Phone over Bluetooth:** mainline ships the BLE companion API. Enable on-device: **Config → Advanced → Toggle BLE API** (stateless toggle, no indicator — press once; it advertises as **"Bruc"**). Connect with **Bruce Remote for iOS** (sideloaded from BruceDevices/App-IOS) or any Nordic-UART BLE client (nRF Connect). BLE (not WiFi) means it survives WiFi attacks. **Note: the BLE-API enable flag does NOT persist across reboot** — re-toggle after each power-cycle (see TODO).

---

## Key gotchas (don't re-learn these)

- **Apply the DisplayDrivers patch** or everything speckles. (`TFT_MONOCHROME` not honored by mainline's backend.)
- **`-DROTATION=0`** for this board. mainline's DisplayDrivers backend applies rotation 180° off from BruceButBetter's — BBB used `ROTATION=2`; here that's upside-down, so we use `0`.
- **CC1101 CS = GPIO39** (silkscreen labels swapped).
- **`lib_ignore = FastLED`** — on ESP32-S3 LTO builds FastLED triggers a `windowed longcall crosses 1GB boundary` link error; the Heltec has no RGB LED so it's unused anyway.
- **`lib_ignore = TFT_eSPI`** — LovyanGFX replaces it for the OLED.
- **`NO_LR_BUTTONS`**: the main icon grid navigates on UP/DOWN, so the single-button "advance" emits `DownPress`.
- **R2 silicon → `qio_qspi`.** GPIO33–37 are free GPIOs here (unlike N16R8/OPI-PSRAM boards).
- **ESP32-S3 BLE + WiFi share one 2.4 GHz radio** — coexist fine normally; aggressive WiFi attack modes may degrade BLE.

---

## Known gaps / TODO

- **BLE API doesn't persist / auto-start** — the enable flag is a RAM-only static (`ble_api_enabled` in `src/core/settings.cpp`); resets OFF every boot. TODO: persist it + auto-`bleApi.setup()` at boot (simplest: call it at end of `interface.cpp` setup).
- **On-screen keyboard** not compacted — usable but cramped (single-button entry is painful anyway).
- **Some popup/message boxes** may render tall on 64 px (functional, not polished).
- **Frame batching** uses the restored `startWrite/endWrite` (via the patch).
- Not yet exercised: **live RX decode of a real transmitter / TX / RSSI scan** (detection + per-band antenna switching are confirmed).

---

## Lineage / related

- Mainline Bruce: https://github.com/BruceDevices/firmware (this fork's base, v1.17)
- 128×64 UI source: https://github.com/Yoursel71/BruceButBetter (BruceButBetter, Bruce v1.5.0 — the compact UI was lifted from here, with credit)
- Display driver: https://github.com/bmorcelli/DisplayDrivers (the `lib/DisplayDrivers` submodule; our mono fix is the committed patch)
- Prior Heltec Bruce fork referenced for pin constants: https://github.com/Unkl3Errl/HeltecFirmware
