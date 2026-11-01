// =============================================================================
//  God's Eye CYD  -  microSD (software SPI) + WiGLE CSV writer
//  The CYD has no spare hardware SPI bus (display + touch use both), so the SD
//  card runs on SOFTWARE SPI via SdFat — display and touch are left untouched.
//
//  >>> Install the "SdFat" library, then set  SPI_DRIVER_SELECT  to  2  in its
//      SdFatConfig.h (enables software SPI). <<<
//
//  Produces a WiGLE-1.4 CSV that uploads directly to wigle.net.
// =============================================================================
#pragma once
#include <time.h>
#include "app_state.h"

#if USE_SD
#include <SdFat.h>

SoftSpiDriver<SD_MISO_PIN, SD_MOSI_PIN, SD_SCK_PIN> g_sdSoftSpi;
#define SD_CFG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(0), &g_sdSoftSpi)

static SdFat32 g_sd;                 // FAT16/32 (format SD cards as FAT32)
static File32  g_wdFile;
static bool    g_sdOk = false;
static char    g_wdFileName[32] = "";

inline bool sdInit(){
  g_sdOk = g_sd.begin(SD_CFG);
  return g_sdOk;
}

// escape an SSID for CSV: wrap in quotes, double any internal quotes
static void csvQuote(const char* in, char* out, size_t cap){
  size_t o=0; if(cap<3){ if(cap) out[0]=0; return; }
  out[o++]='"';
  for(size_t i=0; in[i] && o<cap-2; i++){
    if(in[i]=='"'){ if(o<cap-3){ out[o++]='"'; out[o++]='"'; } }
    else out[o++]=in[i];
  }
  out[o++]='"'; out[o]=0;
}

static void nowStamp(char* out, size_t cap){
  time_t now=time(nullptr);
  if(now>1700000000){ struct tm* t=gmtime(&now); strftime(out,cap,"%Y-%m-%d %H:%M:%S",t); }
  else snprintf(out,cap,"1970-01-01 00:00:00");
}

// Open a fresh WiGLE CSV with its two header lines. Returns true on success.
inline bool wigleOpen(){
  if(!g_sdOk) return false;
  time_t now=time(nullptr);
  if(now>1700000000){ struct tm* t=gmtime(&now); strftime(g_wdFileName,sizeof(g_wdFileName),"/WD_%Y%m%d_%H%M%S.csv",t); }
  else snprintf(g_wdFileName,sizeof(g_wdFileName),"/WD_%lu.csv",(unsigned long)millis());
  if(!g_wdFile.open(g_wdFileName, O_WRITE|O_CREAT|O_APPEND)) return false;
  g_wdFile.println(F("WigleWifi-1.4,appRelease=1.0,model=ESP32-2432S028,"
                     "release=1.0,device=GodsEyeCYD,display=,board=ESP32,brand=CYD"));
  g_wdFile.println(F("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,"
                     "CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type"));
  g_wdFile.sync();
  return true;
}

// Append one observation row (Type = "WIFI" or "BLE").
inline void wigleRow(const char* mac,const char* ssid,const char* auth,
                     int chan,int rssi,double lat,double lon,double alt,
                     double acc,const char* type){
  if(!g_sdOk) return;
  char ts[24]; nowStamp(ts,sizeof(ts));
  char ssidQ[72]; csvQuote(ssid?ssid:"",ssidQ,sizeof(ssidQ));
  g_wdFile.print(mac);     g_wdFile.print(',');
  g_wdFile.print(ssidQ);   g_wdFile.print(',');
  g_wdFile.print(auth);    g_wdFile.print(',');
  g_wdFile.print(ts);      g_wdFile.print(',');
  g_wdFile.print(chan);    g_wdFile.print(',');
  g_wdFile.print(rssi);    g_wdFile.print(',');
  g_wdFile.print(lat,6);   g_wdFile.print(',');
  g_wdFile.print(lon,6);   g_wdFile.print(',');
  g_wdFile.print(alt,1);   g_wdFile.print(',');
  g_wdFile.print(acc,1);   g_wdFile.print(',');
  g_wdFile.println(type);
}

inline void wigleFlush(){ if(g_sdOk) g_wdFile.sync(); }
inline void wigleClose(){ if(g_sdOk && g_wdFile) { g_wdFile.sync(); g_wdFile.close(); } }

#else   // USE_SD == 0  — stubs so the rest of the code still builds
inline bool sdInit(){ return false; }
inline bool wigleOpen(){ return false; }
inline void wigleRow(const char*,const char*,const char*,int,int,double,double,double,double,const char*){}
inline void wigleFlush(){}
inline void wigleClose(){}
static bool g_sdOk=false;
static char g_wdFileName[4]="";
#endif
