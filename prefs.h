// =============================================================================
//  God's Eye CYD  -  persistent settings in NVS (ESP32 flash)
//  No SD card, no PSRAM. Preferences stores a few dozen bytes in a flash
//  partition that survives reboots and reflashes-that-keep-NVS.
// =============================================================================
#pragma once
#include <Preferences.h>
#include "app_state.h"

static Preferences g_prefs;
#define PREFS_NS  "gecyd"

// Pack / unpack the layer toggles into one byte.
static uint8_t layersToByte(){
  uint8_t b=0; for(int i=0;i<L_COUNT;i++) if(g_app.layerOn[i]) b|=(1<<i); return b;
}
static void byteToLayers(uint8_t b){
  for(int i=0;i<L_COUNT;i++) g_app.layerOn[i]=(b>>i)&1;
}

// Load everything into g_app. Missing keys fall back to the passed defaults, so
// a first boot just keeps config.h values.
inline void prefsLoad(){
  g_prefs.begin(PREFS_NS, true);                 // read-only
  g_app.homeLat = g_prefs.getDouble("lat",  DEFAULT_HOME_LAT);
  g_app.homeLon = g_prefs.getDouble("lon",  DEFAULT_HOME_LON);
  String lbl    = g_prefs.getString("label", DEFAULT_HOME_LABEL);
  strlcpy(g_app.homeLabel, lbl.c_str(), sizeof(g_app.homeLabel));
  g_app.theme   = g_prefs.getInt("theme", DEFAULT_THEME);
  g_app.rangeNm = g_prefs.getInt("range", DEFAULT_RANGE_NM);
  uint8_t defL = (LAYER_FLIGHTS_ON<<L_FLIGHTS)|(LAYER_QUAKES_ON<<L_QUAKES)|
                 (LAYER_SATS_ON<<L_SATS)|(LAYER_LAUNCHES_ON<<L_LAUNCHES)|
                 (LAYER_CCTV_ON<<L_CCTV);
  byteToLayers(g_prefs.getUChar("layers", defL));
  g_prefs.end();
}

// Save the home location (called after a successful geocode) + the raw query
// string, so we only re-geocode when the user actually changes it.
inline void prefsSaveHome(double lat,double lon,const char* label,const char* loc){
  g_prefs.begin(PREFS_NS, false);
  g_prefs.putDouble("lat", lat);
  g_prefs.putDouble("lon", lon);
  g_prefs.putString("label", label);
  g_prefs.putString("loc", loc);
  g_prefs.end();
}

// Save UI prefs the user changes at runtime (theme, range, layer toggles).
inline void prefsSaveUI(){
  g_prefs.begin(PREFS_NS, false);
  g_prefs.putInt("theme", g_app.theme);
  g_prefs.putInt("range", g_app.rangeNm);
  g_prefs.putUChar("layers", layersToByte());
  g_prefs.end();
}

// The location string last geocoded (for change detection).
inline String prefsGetLoc(){
  g_prefs.begin(PREFS_NS, true);
  String s = g_prefs.getString("loc", "");
  g_prefs.end();
  return s;
}
