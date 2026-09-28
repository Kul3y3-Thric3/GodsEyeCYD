// =============================================================================
//  God's Eye CYD  -  OSINT feed layers (all keyless)
//    flights  : adsb.lol          (REST, near HOME)
//    quakes   : USGS              (GeoJSON, 2.5+ last 24h, global)
//    sats     : CelesTrak TLE     + on-device SGP4 (Hopperpop)
//    launches : Launch Library 2  (upcoming, rate-limited)
// =============================================================================
#pragma once
#include <time.h>
#include <Sgp4.h>
#include "net.h"
#include "geo.h"

// ------------------------------------------------------------------ FLIGHTS ---
// Diagnostic shown in the panel: "okN" (fetched N), "errNNN" (HTTP code), or
// "off". adsb.fi is the primary source (keyless, no bot-blocking); its array is
// "aircraft" while adsb.lol's is "ac", so we accept whichever key is present.
char g_flDiag[12] = "--";

inline void pollFlights(){
  char url[120];
  snprintf(url,sizeof(url),
    "https://opendata.adsb.fi/api/v2/lat/%.4f/lon/%.4f/dist/%d",
    g_app.qLat,g_app.qLon,(int)FLIGHTS_QUERY_NM);

  JsonDocument filter;
  const char* keys[2] = {"aircraft","ac"};
  for(int k=0;k<2;k++){
    JsonObject f = filter[keys[k]].add<JsonObject>();
    f["hex"]=true; f["flight"]=true; f["lat"]=true; f["lon"]=true;
    f["alt_baro"]=true; f["gs"]=true; f["track"]=true;
    f["baro_rate"]=true; f["dbFlags"]=true;
    f["r"]=true; f["t"]=true;                 // registration + ICAO type
  }

  JsonDocument doc; int code=0;
  if(!fetchJson(url,doc,&filter,9000,&code)){
    snprintf(g_flDiag,sizeof(g_flDiag),"err%d",code);
    setStatus("ADS-B: net err"); return;
  }
  JsonArray arr = doc["aircraft"].isNull() ? doc["ac"].as<JsonArray>()
                                           : doc["aircraft"].as<JsonArray>();

  DATA_LOCK();
  int n=0;
  for(JsonObject a : arr){
    if(n>=FLIGHTS_MAX) break;
    Aircraft& ac = g_ac[n];
    strlcpy(ac.hex, a["hex"] | "", sizeof(ac.hex));
    const char* fl = a["flight"] | "";
    strlcpy(ac.flight, fl, sizeof(ac.flight));
    for(int i=strlen(ac.flight)-1;i>=0&&ac.flight[i]==' ';i--) ac.flight[i]=0; // trim
    ac.lat = a["lat"] | 1000.0f;
    ac.lon = a["lon"] | 1000.0f;
    if(ac.lat>900||ac.lon>900) continue;             // no position -> skip
    JsonVariant alt = a["alt_baro"];
    ac.altFt = alt.is<const char*>() ? -1 : (alt | -99999);  // "ground" -> -1
    ac.gs    = (int)(a["gs"] | 0.0f);
    ac.track = (int)(a["track"] | 0.0f);
    ac.vs    = (int)(a["baro_rate"] | 0);
    ac.mil   = ((a["dbFlags"] | 0) & 1) != 0;
    strlcpy(ac.reg,  a["r"] | "", sizeof(ac.reg));
    strlcpy(ac.type, a["t"] | "", sizeof(ac.type));
    ac.seen  = millis();
    ac.used  = true;
    n++;
  }
  g_acN = n;
  DATA_UNLOCK();
  snprintf(g_flDiag,sizeof(g_flDiag),"ok%d",n);
  g_app.lastRx = millis();
}

// ------------------------------------------------------------------- QUAKES ---
inline void pollQuakes(){
  // 4.5+ over 24h keeps the JSON body small enough to buffer without PSRAM.
  // For denser seismicity swap to 2.5_day.geojson (much larger; risks OOM here).
  const char* url =
    "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/4.5_day.geojson";
  JsonDocument filter;
  JsonObject fp = filter["features"].add<JsonObject>();
  fp["properties"]["mag"]=true; fp["properties"]["place"]=true;
  fp["properties"]["time"]=true; fp["geometry"]["coordinates"]=true;

  JsonDocument doc;
  if(!fetchJson(url,doc,&filter,9000)){ setStatus("USGS: net err"); return; }

  DATA_LOCK();
  int n=0;
  for(JsonObject ft : doc["features"].as<JsonArray>()){
    if(n>= (int)(sizeof(g_qk)/sizeof(g_qk[0]))) break;
    JsonArray c = ft["geometry"]["coordinates"];
    if(c.size()<2) continue;
    Quake& q=g_qk[n];
    q.lon=c[0]|0.0f; q.lat=c[1]|0.0f; q.depthKm=c.size()>2?(c[2]|0.0f):0.0f;
    q.mag=ft["properties"]["mag"]|0.0f;
    strlcpy(q.place, ft["properties"]["place"] | "", sizeof(q.place));
    q.timeUtc=(uint32_t)((ft["properties"]["time"] | 0LL)/1000LL);
    q.used=true; n++;
  }
  g_qkN=n;
  DATA_UNLOCK();
  g_app.lastRx=millis();
}

// --------------------------------------------------------------------- SATS ---
static Sgp4  s_sgp4[SATS_MAX];
static bool  s_sgp4Ready[SATS_MAX] = {false};

// Fetch fresh TLEs from CelesTrak and (re)init the propagators.
inline void refreshTLEs(){
  for(size_t i=0;i<SATS_MAX;i++){
    char url[96];
    snprintf(url,sizeof(url),
      "https://celestrak.org/NORAD/elements/gp.php?CATNR=%ld&FORMAT=TLE",
      SAT_CATNRS[i]);
    String body;
    if(!fetchText(url,body,8000) || body.length()<130){ continue; }
    // Body = "NAME\r\n1 .....\r\n2 .....\r\n"
    int a=body.indexOf('\n');            if(a<0) continue;
    int b=body.indexOf('\n',a+1);        if(b<0) continue;
    String name=body.substring(0,a);      name.trim();
    String l1  =body.substring(a+1,b);    l1.trim();
    String l2  =body.substring(b+1);      l2.trim();
    if(l1.length()<69 || l2.length()<69) continue;
    static char nm[18], t1[80], t2[80];
    strlcpy(nm,name.c_str(),sizeof(nm));
    strlcpy(t1,l1.c_str(),sizeof(t1));
    strlcpy(t2,l2.c_str(),sizeof(t2));
    s_sgp4[i].init(nm,t1,t2);
    g_sat[i].catnr=SAT_CATNRS[i];
    strlcpy(g_sat[i].name,nm,sizeof(g_sat[i].name));
    g_sat[i].haveTle=true;
    s_sgp4Ready[i]=true;
    delay(150);                          // be gentle to CelesTrak
  }
}

// Propagate every sat to "now" (needs NTP time). Cheap; runs often.
inline void pollSats(){
  time_t now=time(nullptr);
  if(now < 1700000000){ g_app.timeUp=false; return; }   // clock not set yet
  g_app.timeUp=true;
  DATA_LOCK();
  for(size_t i=0;i<SATS_MAX;i++){
    if(!s_sgp4Ready[i]){ g_sat[i].used=false; continue; }
    s_sgp4[i].findsat((unsigned long)now);
    g_sat[i].lat  = s_sgp4[i].satLat;
    g_sat[i].lon  = s_sgp4[i].satLon;
    g_sat[i].altKm= s_sgp4[i].satAlt;
    g_sat[i].used = true;
  }
  DATA_UNLOCK();
}

// ----------------------------------------------------------------- LAUNCHES ---
static uint32_t parseIso8601(const char* s){
  // Expects "YYYY-MM-DDTHH:MM:SSZ" (UTC). ESP32 has no timegm(), so convert by
  // hand with the days-from-civil algorithm (no timezone, no libc dependency).
  if(!s||strlen(s)<19) return 0;
  int Y=atoi(s), Mo=atoi(s+5), D=atoi(s+8);
  int h=atoi(s+11), mi=atoi(s+14), se=atoi(s+17);
  if(Mo<1||Mo>12) return 0;
  Y -= (Mo <= 2);
  long era = (Y >= 0 ? Y : Y-399) / 400;
  unsigned yoe = (unsigned)(Y - era*400);
  unsigned doy = (153*(Mo + (Mo>2 ? -3 : 9)) + 2)/5 + D - 1;
  unsigned doe = yoe*365 + yoe/4 - yoe/100 + doy;
  long days = era*146097 + (long)doe - 719468;   // days since 1970-01-01 UTC
  return (uint32_t)(days*86400LL + h*3600 + mi*60 + se);
}

inline void pollLaunches(){
  const char* url=
    "https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=5&hide_recent_previous=true";
  JsonDocument filter;
  JsonObject fr=filter["results"].add<JsonObject>();
  fr["name"]=true; fr["net"]=true; fr["pad"]["name"]=true;

  JsonDocument doc;
  if(!fetchJson(url,doc,&filter,9000)) return;

  DATA_LOCK();
  int n=0;
  for(JsonObject r : doc["results"].as<JsonArray>()){
    if(n>=(int)(sizeof(g_lx)/sizeof(g_lx[0]))) break;
    Launch& L=g_lx[n];
    strlcpy(L.name, r["name"] | "?", sizeof(L.name));
    strlcpy(L.pad,  r["pad"]["name"] | "", sizeof(L.pad));
    L.net = parseIso8601(r["net"] | "");
    L.used=true; n++;
  }
  g_lxN=n;
  DATA_UNLOCK();
}
