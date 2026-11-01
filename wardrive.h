// =============================================================================
//  God's Eye CYD  -  WARDRIVE tab  (WiFi now; BLE added in the next flash)
//  Scans WiFi (and later BLE), dedupes by MAC, and logs each new network with
//  the current GPS fix to a WiGLE-format CSV on the SD card.
//
//  Include AFTER ui.h (uses mapTouch) and after gps.h / sd.h.
// =============================================================================
#pragma once
#include <WiFi.h>
#include "sdcard.h"
#include "gps.h"
#include "theme.h"

static int      g_wdMode    = 0;     // 0 = WiFi, 1 = BLE, 2 = Both
static bool     g_wdRunning = false;
static int      g_wdWifiLast= 0;     // APs seen in the last scan
static int      g_wdBleLast = 0;
static long     g_wdUnique  = 0;     // unique networks logged this session
static long     g_wdScanN   = 0;
static uint32_t g_wdLast    = 0;
static uint8_t  g_wdSeen[WD_MAX_UNIQUE][6];
static int      g_wdSeenN   = 0;

static const char* authStr(uint8_t e){
  switch(e){
    case WIFI_AUTH_OPEN:            return "[OPEN][ESS]";
    case WIFI_AUTH_WEP:             return "[WEP][ESS]";
    case WIFI_AUTH_WPA_PSK:         return "[WPA-PSK][ESS]";
    case WIFI_AUTH_WPA2_PSK:        return "[WPA2-PSK][ESS]";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "[WPA-WPA2-PSK][ESS]";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "[WPA2-EAP][ESS]";
    case WIFI_AUTH_WPA3_PSK:        return "[WPA3-SAE][ESS]";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "[WPA2-WPA3-SAE][ESS]";
    default:                        return "[ESS]";
  }
}

static bool wdSeenAdd(const uint8_t* b){   // true if this MAC is new
  for(int i=0;i<g_wdSeenN;i++) if(memcmp(g_wdSeen[i],b,6)==0) return false;
  if(g_wdSeenN<WD_MAX_UNIQUE){ memcpy(g_wdSeen[g_wdSeenN++],b,6); return true; }
  return false;
}

static double gpsAltM(){ return gpsParser.altitude.isValid()? gpsParser.altitude.meters() : 0.0; }

// blocking WiFi scan (~2-3s); logs new BSSIDs only when we have a GPS fix
static void wdScanWifi(){
  int n = WiFi.scanNetworks(false, true);      // sync, include hidden
  g_wdWifiLast = (n>0)? n : 0;
  if(n>0){
    for(int i=0;i<n;i++){
      uint8_t* b = WiFi.BSSID(i); if(!b) continue;
      if(g_gpsFix && wdSeenAdd(b)){
        char mac[18];
        snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",b[0],b[1],b[2],b[3],b[4],b[5]);
        String ss = WiFi.SSID(i);
        wigleRow(mac, ss.c_str(), authStr(WiFi.encryptionType(i)),
                 WiFi.channel(i), WiFi.RSSI(i), g_gpsLat, g_gpsLon, gpsAltM(), 10.0, "WIFI");
        g_wdUnique++;
      }
    }
    if(g_gpsFix) wigleFlush();
  }
  WiFi.scanDelete();
}

inline void drawWardrive();   // fwd

static void wdStart(){
  if(!g_app.gpsOn) gpsSetEnabled(true);
  g_wdSeenN=0; g_wdUnique=0; g_wdScanN=0; g_wdWifiLast=0; g_wdBleLast=0;
  if(g_sdOk) wigleOpen();
  g_wdRunning=true; g_wdLast=0;                // scan on the next tick
}
static void wdStop(){
  if(!g_wdRunning) return;
  g_wdRunning=false;
  wigleClose();
}

// Called from loop() while the wardrive screen is up.
inline void wardriveTick(){
  if(!g_wdRunning) return;
  if(g_wdLast!=0 && millis()-g_wdLast < WD_SCAN_MS) return;
  g_wdLast=millis();
  const Palette& p=PAL();
  tft.setTextDatum(MC_DATUM); tft.setTextFont(1); tft.setTextColor(p.warn,p.bg);
  tft.fillRect(60,96,200,12,p.bg); tft.drawString("scanning...",160,102);
#if USE_BLE
  if(g_wdMode==0 || g_wdMode==2) wdScanWifi();
  if(g_wdMode==1 || g_wdMode==2) wdScanBle();
#else
  wdScanWifi();
#endif
  g_wdScanN++;
  drawWardrive();
}

inline void drawWardrive(){
  const Palette& p=PAL();
  tft.fillScreen(p.bg);
  tft.setTextFont(2); tft.setTextDatum(TC_DATUM); tft.setTextColor(p.accent,p.bg);
  tft.drawString("WARDRIVE",160,3);

  // mode buttons
  const char* modes[3]={"WIFI","BLE","BOTH"};
  for(int i=0;i<3;i++){ int x=8+i*102, w=96, y=24;
    bool sel=(g_wdMode==i), avail=(i==0)|| (bool)USE_BLE;
    tft.drawRoundRect(x,y,w,22,4,p.grid);
    if(sel) tft.fillRoundRect(x+2,y+2,w-4,18,3,p.grid);
    tft.setTextDatum(MC_DATUM); tft.setTextFont(2);
    tft.setTextColor(avail?(sel?p.center:p.text):p.dim, sel?p.grid:p.bg);
    tft.drawString(modes[i],x+w/2,y+11);
  }
#if !USE_BLE
  tft.setTextFont(1); tft.setTextDatum(TC_DATUM); tft.setTextColor(p.dim,p.bg);
  tft.drawString("BLE / BOTH arrive next update - logging WiFi",160,50);
#endif

  // START / STOP
  int by=62;
  tft.fillRoundRect(60,by,200,30,6, g_wdRunning?p.warn:p.center);
  tft.setTextDatum(MC_DATUM); tft.setTextFont(4); tft.setTextColor(p.bg, g_wdRunning?p.warn:p.center);
  tft.drawString(g_wdRunning?"STOP":"START",160,by+15);

  // status
  char b[48]; int y=106;
  tft.setTextFont(2); tft.setTextDatum(TL_DATUM);
  tft.setTextColor(p.dim,p.bg);    tft.drawString("SD:",10,y);
  tft.setTextColor(g_sdOk?p.center:p.warn,p.bg);
  tft.drawString(g_sdOk?(g_wdFileName[0]?g_wdFileName:"ready"):"no card",48,y); y+=20;

  tft.setTextColor(p.dim,p.bg);    tft.drawString("GPS:",10,y);
  tft.setTextColor(g_gpsFix?p.center:p.warn,p.bg);
  if(g_gpsFix) snprintf(b,sizeof(b),"FIX %.5f,%.5f",g_gpsLat,g_gpsLon);
  else         snprintf(b,sizeof(b),"%s",g_gpsPresent?"acquiring...":"no module");
  tft.drawString(b,54,y); y+=20;

  tft.setTextColor(p.dim,p.bg); tft.drawString("Scans:",10,y);
  tft.setTextColor(p.text,p.bg); snprintf(b,sizeof(b),"%ld",g_wdScanN); tft.drawString(b,72,y); y+=20;
  tft.setTextColor(p.dim,p.bg); tft.drawString("WiFi seen:",10,y);
  tft.setTextColor(p.text,p.bg); snprintf(b,sizeof(b),"%d",g_wdWifiLast); tft.drawString(b,112,y); y+=20;
  tft.setTextColor(p.dim,p.bg); tft.drawString("Logged:",10,y);
  tft.setTextColor(p.accent,p.bg); snprintf(b,sizeof(b),"%ld unique",g_wdUnique); tft.drawString(b,78,y); y+=20;

  if(g_wdRunning && !g_gpsFix){
    tft.setTextFont(1); tft.setTextColor(p.warn,p.bg);
    tft.drawString("waiting for GPS fix - not logging yet",10,y);
  }
  // BACK
  tft.setTextFont(2); tft.setTextDatum(TR_DATUM); tft.setTextColor(p.text,p.bg);
  tft.drawString("BACK",315,222);
}

// returns true if the user asked to leave (BACK)
inline bool wardriveTouch(int mx,int my){
  if(my>=24 && my<46){                               // mode buttons
    int i=(mx-8)/102;
    if(i>=0 && i<3 && (i==0 || USE_BLE)){ g_wdMode=i; drawWardrive(); }
    return false;
  }
  if(my>=62 && my<92 && mx>=60 && mx<260){            // START/STOP
    if(g_wdRunning) wdStop(); else wdStart();
    drawWardrive(); return false;
  }
  if(my>=214 && mx>270){ wdStop(); return true; }     // BACK
  return false;
}
