// =============================================================================
//  God's Eye CYD  -  USER CONFIG
//  Edit this file, nothing else, to get flying.
// =============================================================================
#pragma once

// --- WiFi ---------------------------------------------------------------------
// NO WiFi credentials here anymore. On first boot (or when the saved network is
// unreachable, or when you hold BOOT at power-on) the device broadcasts its own
// setup hotspot; join it from a phone and pick your network + location on a web
// page. Credentials are saved in the ESP32's flash (NVS) and reused every boot.
#define AP_SSID         "GodsEye-Setup"   // the setup hotspot name
#define AP_PASS         ""                // "" = open portal; set 8+ chars to lock it
#define PORTAL_TIMEOUT_S  180             // portal auto-closes after this idle (s)

// --- Where the eye is centered (DEFAULTS ONLY) -------------------------------
// The radar is drawn north-up, centered on HOME. The live value is set from the
// setup portal's Location field (geocoded) and stored in NVS. These are only the
// fallback used before anything is configured. (Nashville, TN.)
#define DEFAULT_HOME_LAT    36.1627
#define DEFAULT_HOME_LON   -86.7816
#define DEFAULT_HOME_LABEL  "NASH"        // up to 9 chars, shown at scope center

// Radar range in nautical miles at the outer ring. Tap the range chip on-screen
// to cycle through the RANGE_STEPS below at runtime; this is just the default.
#define DEFAULT_RANGE_NM   80
static const int RANGE_STEPS[] = { 20, 40, 80, 150, 250 };

// --- Which layers are live ----------------------------------------------------
// You can also toggle these at runtime from the LAYERS menu; these are the
// power-on defaults.
#define LAYER_FLIGHTS_ON    true
#define LAYER_QUAKES_ON     true
#define LAYER_SATS_ON       true
#define LAYER_LAUNCHES_ON   false   // off by default: LL2 is heavily rate-limited
#define LAYER_CCTV_ON       false   // off by default: opens a separate viewer

// --- Poll intervals (ms) ------------------------------------------------------
// Feeds are polled ONE AT A TIME on a rotating schedule to stay within RAM and
// provider rate limits. Don't push these much lower.
#define POLL_FLIGHTS_MS     8000UL      // adsb.lol is generous; 8s is smooth
#define POLL_QUAKES_MS      120000UL    // USGS updates every ~1 min
#define POLL_SATS_MS        2000UL      // local SGP4 propagation, cheap
#define POLL_TLE_REFRESH_MS 21600000UL  // re-fetch TLEs every 6h
#define POLL_LAUNCHES_MS    1800000UL   // LL2 free tier ~15 req/hr -> 30 min
#define POLL_CCTV_MS        15000UL     // snapshot refresh while viewer is open

// --- Flights (adsb.lol, keyless) ---------------------------------------------
#define FLIGHTS_MAX         48          // hard cap on tracked contacts (RAM)
#define FLIGHTS_QUERY_NM    75          // ADS-B search radius (<=250). Kept small
                                        // so the JSON body reliably fits in heap
                                        // without PSRAM (a large body returns 200
                                        // but fails to parse -> "err200"). Covers
                                        // the busy area around most airports.

// --- Satellites (CelesTrak, keyless) -----------------------------------------
// NORAD catalog numbers to track. 25544 = ISS (ZARYA), 20580 = Hubble.
// Keep this short; each sat is an SGP4 propagator in RAM.
static const long SAT_CATNRS[] = { 25544, 20580, 33591 };  // ISS, HST, NOAA-19
#define SATS_MAX  (sizeof(SAT_CATNRS)/sizeof(SAT_CATNRS[0]))

// --- CCTV (public MJPEG webcams, keyless) ------------------------------------
// The camera list lives in cctv_list.h (a scrollable worldwide set of public
// MJPEG streams). Edit that file to add/remove cameras.

// --- NTP (needed for SGP4 satellite propagation) -----------------------------
#define NTP_SERVER   "pool.ntp.org"
#define GMT_OFFSET_SEC   0        // keep UTC; SGP4 wants UTC
#define DST_OFFSET_SEC   0

// --- Look & feel --------------------------------------------------------------
// Boot theme index into the palette table (see theme.h):
// 0 NORMAL  1 NVG (green)  2 FLIR/IRONBOW  3 AMBER TACTICAL
#define DEFAULT_THEME   3
