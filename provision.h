// =============================================================================
//  God's Eye CYD  -  first-boot provisioning
//    * WiFiManager captive portal (pick network + type password from a phone)
//    * a "Home location" text field on the same page, geocoded (keyless, via
//      Open-Meteo) into lat/lon and stored in NVS
//    * auto-reconnect to the saved network every boot; hold BOOT to re-open it
// =============================================================================
#pragma once
#include <WiFiManager.h>          // "WiFiManager" by tzapu (Library Manager)
#include "net.h"
#include "prefs.h"
#include "theme.h"

// ---- percent-encode a query for the geocoding URL ---------------------------
static String urlEncode(const String& s){
  String o; char b[4];
  for(size_t i=0;i<s.length();i++){
    char c=s[i];
    if(isalnum((unsigned char)c)) o+=c;
    else if(c==' ') o+="%20";
    else { snprintf(b,sizeof(b),"%%%02X",(unsigned char)c); o+=b; }
  }
  return o;
}

// ---- turn a place name into lat/lon + a short scope label -------------------
static bool geocodeCity(const String& q,double& lat,double& lon,char* label9){
  String url="https://geocoding-api.open-meteo.com/v1/search"
             "?count=1&language=en&format=json&name="+urlEncode(q);
  JsonDocument filter;
  JsonObject r=filter["results"].add<JsonObject>();
  r["latitude"]=true; r["longitude"]=true; r["name"]=true;
  JsonDocument doc;
  if(!fetchJson(url.c_str(),doc,&filter,9000)) return false;
  JsonArray res=doc["results"].as<JsonArray>();
  if(res.isNull()||res.size()==0) return false;
  lat=res[0]["latitude"]  | 1000.0;
  lon=res[0]["longitude"] | 1000.0;
  if(lat>900||lon>900) return false;
  const char* nm=res[0]["name"] | "HOME";
  int j=0; for(int i=0;nm[i]&&j<9;i++){ char c=nm[i];
    if(c>='a'&&c<='z') c-=32; label9[j++]=c; }
  label9[j]=0;
  return true;
}

// ---- draw the "join my hotspot" instructions when the portal opens ----------
static void portalScreen(WiFiManager* wm){
  const Palette& p=PAL();
  tft.fillScreen(p.bg);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(p.accent,p.bg); tft.setTextFont(4);
  tft.drawString("SETUP MODE",160,18);
  tft.setTextFont(2); tft.setTextColor(p.text,p.bg);
  tft.drawString("1. On your phone, join WiFi:",160,60);
  tft.setTextColor(p.center,p.bg);
  tft.drawString(AP_SSID,160,82);
  tft.setTextColor(p.text,p.bg);
  tft.drawString("2. It opens a setup page",160,112);
  tft.drawString("   (or go to 192.168.4.1)",160,132);
  tft.drawString("3. Pick network + type location",160,158);
  tft.setTextColor(p.dim,p.bg); tft.setTextFont(1);
  tft.drawString("Location example: Nashville, TN",160,188);
  tft.drawString("Times out; hold BOOT at power-on to reopen",160,204);
}

// Run WiFi provisioning. forcePortal=true always opens the portal (the SETUP
// button / BOOT-held); otherwise it auto-connects to the saved network and only
// opens the portal if that fails. Returns true once WiFi is up.
inline bool provisionRun(bool forcePortal){
  const char* apPass = (AP_PASS[0] ? AP_PASS : nullptr);   // nullptr = open AP
  WiFiManager wm;
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setAPCallback([](WiFiManager* w){ portalScreen(w); });

  String savedLoc = prefsGetLoc();
  WiFiManagerParameter locParam("loc","Home location (City, ST or City, Country)",
                                savedLoc.c_str(), 48);
  wm.addParameter(&locParam);

  bool ok = forcePortal ? wm.startConfigPortal(AP_SSID, apPass)
                        : wm.autoConnect(AP_SSID, apPass);
  g_app.wifiUp = (WiFi.status()==WL_CONNECTED);
  if(!g_app.wifiUp) return false;

  // Geocode only when the location field changed (saves an API hit every boot).
  String newLoc = locParam.getValue(); newLoc.trim();
  if(newLoc.length() && newLoc != savedLoc){
    const Palette& p=PAL();
    tft.fillScreen(p.bg); tft.setTextDatum(MC_DATUM);
    tft.setTextColor(p.text,p.bg); tft.setTextFont(2);
    tft.drawString("Locating...",160,120);
    double la,lo; char lb[10];
    if(geocodeCity(newLoc,la,lo,lb)){
      g_app.homeLat=la; g_app.homeLon=lo;
      strlcpy(g_app.homeLabel,lb,sizeof(g_app.homeLabel));
      prefsSaveHome(la,lo,lb,newLoc.c_str());
    } else {
      tft.setTextColor(p.warn,p.bg);
      tft.drawString("Location not found - keeping last",160,150);
      delay(1500);
    }
  }
  return true;
}
