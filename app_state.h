// =============================================================================
//  God's Eye CYD  -  shared state, data structures, extern globals
//  All real global objects are DEFINED in GodsEyeCYD.ino; declared extern here.
// =============================================================================
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "config.h"

// ---- Layer identifiers -------------------------------------------------------
enum Layer { L_FLIGHTS = 0, L_QUAKES, L_SATS, L_LAUNCHES, L_CCTV, L_COUNT };

// ---- App screens -------------------------------------------------------------
enum Screen { SCR_RADAR = 0, SCR_MENU, SCR_CCTV, SCR_MAP };

// ---- Contact data models -----------------------------------------------------
struct Aircraft {
  char     hex[8];
  char     flight[10];
  float    lat, lon;
  int32_t  altFt;        // baro altitude, feet (-99999 = unknown/ground)
  int16_t  gs;           // ground speed, knots
  int16_t  track;        // deg, true
  int16_t  vs;           // vertical speed, ft/min (0 if unknown)
  bool     mil;          // military (dbFlags bit)
  uint32_t seen;         // millis() of last update (for aging/dead-reckoning)
  bool     used;
};

struct Quake {
  float    lat, lon;
  float    mag;
  float    depthKm;
  char     place[40];
  uint32_t timeUtc;      // epoch seconds
  bool     used;
};

struct SatObj {
  long     catnr;
  char     name[18];
  float    lat, lon;     // sub-satellite point
  float    altKm;
  bool     haveTle;
  bool     used;
};

struct Launch {
  char     name[48];
  char     pad[28];
  uint32_t net;          // epoch seconds of NET (no-earlier-than)
  bool     used;
};

// ---- Global contact stores (defined in .ino) --------------------------------
extern Aircraft  g_ac[FLIGHTS_MAX];   extern volatile int g_acN;
extern Quake     g_qk[24];            extern volatile int g_qkN;
extern SatObj    g_sat[SATS_MAX];
extern Launch    g_lx[6];             extern volatile int g_lxN;

// ---- Selection / UI runtime state -------------------------------------------
struct AppState {
  Screen  screen      = SCR_RADAR;
  int     theme       = DEFAULT_THEME;
  int     rangeNm     = DEFAULT_RANGE_NM;
  bool    layerOn[L_COUNT];
  int     selLayer    = L_FLIGHTS;     // which layer the selection points into
  int     selIndex    = -1;            // index within that layer, -1 = none
  int     cctvIdx     = 0;              // selected camera
  int     cctvScroll  = 0;             // top row of the camera list
  bool    cctvPlaying = false;         // false = list view, true = live stream
  bool    wifiUp      = false;
  bool    timeUp      = false;
  uint32_t lastRx     = 0;             // millis of last successful poll (link LED)
  char    status[40]  = "BOOT";
  // ---- runtime HOME (set from portal + NVS; defaults until configured) ----
  double  homeLat     = DEFAULT_HOME_LAT;
  double  homeLon     = DEFAULT_HOME_LON;
  char    homeLabel[10] = DEFAULT_HOME_LABEL;
  bool    wantPortal  = false;         // set by the SETUP menu to re-open the portal
  // ---- active flight-query center: home on the radar, map center in MAP mode --
  double  qLat        = DEFAULT_HOME_LAT;
  double  qLon        = DEFAULT_HOME_LON;
};
extern AppState g_app;

// ---- Hardware objects (defined in .ino) -------------------------------------
extern TFT_eSPI            tft;
extern XPT2046_Touchscreen touch;

// ---- Small helpers used across modules --------------------------------------
inline int clampi(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }
void setStatus(const char* s);           // defined in .ino
