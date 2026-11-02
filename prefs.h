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
  g_app.gpsOn = g_prefs.getBool("gps", GPS_DEFAULT_ON);
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
  g_prefs.putBool("gps", g_app.gpsOn);
  g_prefs.end();
}

// The location string last geocoded (for change detection).
inline String prefsGetLoc(){
  g_prefs.begin(PREFS_NS, true);
  String s = g_prefs.getString("loc", "");
  g_prefs.end();
  return s;
}

// ---- Favorite map locations (saved from the MAP "FIND" screen) --------------
// Up to FAV_MAX named places, stored in contiguous slots 0..count-1 as
// fLa<i>/fLo<i> (double) + fNm<i> (string), with the count in "favN".
#define FAV_MAX 10

inline int prefsFavCount(){
  g_prefs.begin(PREFS_NS, true);
  int n = g_prefs.getInt("favN", 0);
  g_prefs.end();
  if(n<0) n=0; if(n>FAV_MAX) n=FAV_MAX;
  return n;
}
inline bool prefsFavGet(int i,char* name,size_t cap,double& lat,double& lon){
  if(i<0 || i>=FAV_MAX) return false;
  char kLa[10],kLo[10],kNm[10];
  snprintf(kLa,sizeof kLa,"fLa%d",i); snprintf(kLo,sizeof kLo,"fLo%d",i); snprintf(kNm,sizeof kNm,"fNm%d",i);
  g_prefs.begin(PREFS_NS, true);
  lat = g_prefs.getDouble(kLa, 1000.0);
  lon = g_prefs.getDouble(kLo, 1000.0);
  String s = g_prefs.getString(kNm, "");
  g_prefs.end();
  if(lat>900 || lon>900) return false;
  strlcpy(name, s.c_str(), cap);
  return true;
}
inline bool prefsFavAdd(const char* name,double lat,double lon){
  int n = prefsFavCount();
  if(n>=FAV_MAX) return false;
  char kLa[10],kLo[10],kNm[10];
  snprintf(kLa,sizeof kLa,"fLa%d",n); snprintf(kLo,sizeof kLo,"fLo%d",n); snprintf(kNm,sizeof kNm,"fNm%d",n);
  g_prefs.begin(PREFS_NS, false);
  g_prefs.putDouble(kLa, lat);
  g_prefs.putDouble(kLo, lon);
  g_prefs.putString(kNm, (name && name[0]) ? name : "(unnamed)");
  g_prefs.putInt("favN", n+1);
  g_prefs.end();
  return true;
}
inline void prefsFavDelete(int idx){
  int n = prefsFavCount();
  if(idx<0 || idx>=n) return;
  for(int i=idx; i<n-1; i++){                     // shift slots down
    char nm[16]; double la,lo;
    if(prefsFavGet(i+1,nm,sizeof nm,la,lo)){
      char kLa[10],kLo[10],kNm[10];
      snprintf(kLa,sizeof kLa,"fLa%d",i); snprintf(kLo,sizeof kLo,"fLo%d",i); snprintf(kNm,sizeof kNm,"fNm%d",i);
      g_prefs.begin(PREFS_NS, false);
      g_prefs.putDouble(kLa,la); g_prefs.putDouble(kLo,lo); g_prefs.putString(kNm,nm);
      g_prefs.end();
    }
  }
  char kLa[10],kLo[10],kNm[10];                    // clear the now-unused last slot
  snprintf(kLa,sizeof kLa,"fLa%d",n-1); snprintf(kLo,sizeof kLo,"fLo%d",n-1); snprintf(kNm,sizeof kNm,"fNm%d",n-1);
  g_prefs.begin(PREFS_NS, false);
  g_prefs.remove(kLa); g_prefs.remove(kLo); g_prefs.remove(kNm);
  g_prefs.putInt("favN", n-1);
  g_prefs.end();
}
