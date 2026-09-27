# God's Eye CYD

A native ESP32 reimagining of [God's Eye View](https://github.com/bilawalsidhu/gods-eye-view) for the **Cheap Yellow Display** (ESP32-2432S028, 2‑USB / ST7789).

The original is CesiumJS + Google Photorealistic 3D Tiles + WebGL — a full 3D globe that cannot run on a 520 KB, no‑PSRAM, no‑GPU WROOM. So this isn't a port of the *renderer*; it's a port of the *idea*: pull the same keyless public OSINT feeds over WiFi and paint them on a **north‑up tactical PPI** (plan‑position indicator / radar) with tap‑to‑select telemetry and sensor‑style optics.

**All five v1 layers are keyless.** WiFi and location are set on the device itself at first boot — from your phone, no editing files — so it's flash-and-go for anyone you hand it to.

---

## What it does

- **PPI radar** centered on your location, north‑up, 3 range rings, tap the range chip to cycle 20 → 250 NM.
- **✈️ Flights** — live ADS‑B from `adsb.lol`. Heading‑aligned glyphs, military traffic highlighted, tap for callsign / alt / GS / track / range.
- **🌍 Earthquakes** — USGS M2.5+ last 24 h, sized by magnitude, tap for depth + place.
- **🛰️ Satellites** — CelesTrak TLEs + on‑device **SGP4** (ISS, HST, NOAA‑19 by default). Sub‑satellite point plots when it's over your scope; tap for alt/lat/lon.
- **🚀 Launches** — Launch Library 2 upcoming, next‑launch countdown in the panel.
- **🗺️ MAP mode** — pannable/zoomable **satellite imagery** of anywhere on Earth (Esri World Imagery, keyless) with **live flights drawn on top**. Tap **MAP** on the radar bar. Tap the map to recenter, **−/+** to zoom, **HOME** to jump back, **BACK** to the radar. It's a *slow* map by design (one JPEG tile fetched + decoded at a time — seconds per repaint; no smooth 3D globe is possible on this chip), but you can explore the whole planet from above.
- **📹 CCTV** — a **scrollable worldwide list of public MJPEG webcams** (ski, ports, airports, city cams…) streamed **live** over HTTP. Tap a camera to watch; tap the image or **NEXT** to skip, **BACK** to the list. Frames are carved from the MJPEG stream (SOI→EOI) and decoded on‑device. Public cams come and go, so some will read "camera offline" — just skip. Edit the list in `cctv_list.h`. (Camera list adapted from 7h30th3r0n3's RaspyJack CCTV viewer.)
- **Optics** — NORMAL / NVG / FLIR‑ironbow / AMBER‑tactical palettes, switch in the menu.

---

## Flash it

### 1. Libraries (Arduino Library Manager)
- **TFT_eSPI** (Bodmer)
- **XPT2046_Touchscreen** (PaulStoffregen)
- **ArduinoJson** — **v7.x**
- **TJpg_Decoder** (Bodmer)
- **Sgp4** (by *Hopperpop* — the `SparkFun_SGP4_Arduino_Library` is the same code and also works)
- **WiFiManager** (by *tzapu*) — the first‑boot captive portal

Board: **ESP32 Dev Module** (esp32 core 2.x or 3.x). Set **Partition Scheme → "Huge APP"** (the TLS stack + libs push past the default app partition).

### 2. The display setup — do this or you get a blank screen
TFT_eSPI is configured at **compile time**, not from the sketch. Copy the included `User_Setup.h` over:

```
Arduino/libraries/TFT_eSPI/User_Setup.h
```

It defaults to **ST7789** (your 2‑USB board). On boot you'll see **R / G / B color bars + text**:
- **Blank / white screen** → wrong driver. Open `User_Setup.h`, comment Option A (ST7789), uncomment Option B (ILI9341), reflash.
- **Colors look like a photo negative** → uncomment `#define TFT_INVERSION_ON` (comment the OFF line).
- **Reds and blues swapped** → change `TFT_RGB_ORDER TFT_BGR` to `TFT_RGB`.
- **Glitchy pixels** → drop `SPI_FREQUENCY` to `40000000`.

### 3. Flash — no file editing needed
Flash `GodsEyeCYD.ino`. WiFi and location are set on‑device (see **First boot** below).
`config.h` still holds tunables you *can* change if you want — default layers, poll
intervals, the satellite catalog numbers, CCTV URLs, and the fallback location — but
none of that is required to get running.

---

## First boot (and re‑setup)

The device stores your WiFi + location in the ESP32's own flash (NVS). **No SD card or PSRAM required.**

1. On first power‑up (or any time it can't reach the saved network) it broadcasts a WiFi hotspot named **`GodsEye‑Setup`**. The screen shows the join instructions.
2. On your phone, join that hotspot. A setup page opens automatically (or browse to `192.168.4.1`).
3. Pick your home WiFi, type the password, and fill the **Home location** field — a city, e.g. `Nashville, TN` or `Berlin, DE`.
4. Save. It connects, geocodes the city once (keyless, via Open‑Meteo) into lat/lon, and remembers everything.

Every boot after that it **auto‑reconnects** silently and centers on your saved location. To change WiFi or location later: **hold the BOOT button while powering on** (or tap **SETUP** in the on‑screen menu) to reopen the portal. It's 2.4 GHz only — the ESP32 has no 5 GHz radio.

---

## Using it

- **Tap a blip** → telemetry card in the right panel. Tap empty scope to deselect.
- **Range chip** (top‑right) → cycle range.
- **MENU** (bottom bar) → toggle layers, cycle optics.
- **CCTV** (bottom bar) → snapshot viewer; tap left/right to change camera, BACK to exit.
- Link dot (top‑right) + green LED = a poll landed in the last 20 s.

### Touch feels off?
Tune the 4 constants at the top of `ui.h` (`TS_MINX/MAXX/MINY/MAXY`). If taps never register, your unit's touch IRQ may be unwired — change `mapTouch()` to drop the `touch.tirqTouched()` check and use `touch.touched()` alone.

---

## Design notes / honest caveats

- **I have not flashed this on hardware** — no ESP32 or CYD in my sandbox. It's written to compile and run, but expect to nudge the display driver toggles and touch calibration on first boot. That's exactly why the boot self‑test and the clearly‑marked toggles exist. Tell me what the screen does and I'll fix it fast.
- **One network fetch at a time.** With no PSRAM, running five TLS feeds concurrently would blow the heap, so feeds poll on a **rotating schedule** (see intervals in `config.h`). A fetch briefly blocks the UI (TLS handshake). The clean upgrade is to move polling onto a FreeRTOS task on core 0 with a mutex around the contact arrays — a good v2.
- **Rate limits.** `adsb.lol` and USGS are generous. **Launch Library 2 free tier is ~15 req/hr** — hence the 30‑min poll and off‑by‑default. CelesTrak TLEs refresh every 6 h.
- **Selection across refreshes** is by array index, so a selected plane can "jump" when the flight list updates. Matching by ICAO hex is a small v2 improvement.
- **CCTV** needs a **direct `.jpg` snapshot** URL, not an HLS/RTSP stream or an HTML page. Swap in TxDOT / Caltrans / Fintraffic / DriveBC snapshot URLs (same sources God's Eye View uses).
- **`setInsecure()`** skips TLS cert validation — fine for public read‑only feeds; don't reuse this client for anything you care about authenticating.
- **Satellites** only appear on the scope when their ground track crosses your range — a real "ISS overhead" moment, not a persistent dot.

## File map
```
GodsEyeCYD.ino   main: globals, HW init, boot test, scheduler, touch, loop
config.h         optional tunables (defaults; no editing required)
User_Setup.h     >>> copy into the TFT_eSPI library folder <<<
app_state.h      shared structs + extern globals (runtime home location)
net.h            WiFi reconnect + TLS JSON/text/binary fetch
prefs.h          NVS persistence (location, theme, range, layers)
provision.h      WiFiManager captive portal + Open-Meteo geocoding
geo.h            haversine range/bearing + north-up PPI projection
theme.h          the four optic palettes
layers.h         flights / quakes / sats(SGP4) / launches pollers
cctv.h           CCTV: scrollable list + live MJPEG-over-HTTP viewer
cctv_list.h      the bundled worldwide public-webcam list (edit to taste)
ui.h             radar, HUD, telemetry, menu, touch mapping
mapview.h        satellite MAP mode (Esri tiles + flight overlay, pan/zoom)
```

## Credits

- **God's Eye View** by **[Bilawal Sidhu](https://github.com/bilawalsidhu)** & **Sameh Khamis** ([Halfpixel](https://halfpixel.ai)) — the original open-source live-OSINT globe that inspired this whole project. This firmware is an independent, hardware-scaled reimagining of that idea for the ESP32; all the credit for the concept and the "spatial intelligence for everyone" spirit is theirs. Original repo: <https://github.com/bilawalsidhu/gods-eye-view> (MIT).
- **CCTV viewer & camera list** adapted from **[7h30th3r0n3](https://github.com/7h30th3r0n3)**'s **RaspyJack** (the MJPEG-over-HTTP approach and the public-webcam list). Repo: <https://github.com/7h30th3r0n3/Raspyjack>.
- **Data & imagery:** aircraft — [adsb.fi](https://adsb.fi) / [adsb.lol](https://adsb.lol); earthquakes — [USGS](https://earthquake.usgs.gov); satellites — [CelesTrak](https://celestrak.org) TLEs + the **Sgp4** library by *Hopperpop*; launches — [Launch Library 2 / The Space Devs](https://thespacedevs.com); geocoding — [Open-Meteo](https://open-meteo.com); satellite basemap — **Esri World Imagery**; captive portal — **WiFiManager** by *tzapu*; display/touch/JPEG — **TFT_eSPI** & **TJpg_Decoder** by *Bodmer*, **XPT2046_Touchscreen** by *PaulStoffregen*.

Built by **Kul3y3-Thric3**. Released under the MIT License (see `LICENSE`).

Data may be delayed, incomplete, modeled, or wrong. This is an exploratory visualization of public data — **do not use it for navigation, aviation, emergency, or any safety-critical purpose.** Each data source carries its own terms of use; respect them. The bundled public webcams are third-party feeds that go up and down without notice.
