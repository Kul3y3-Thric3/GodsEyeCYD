// =============================================================================
//  God's Eye CYD  -  FIND LOCATION  (satellite MAP destination picker)
//  One screen that ties together every way to move the satellite map:
//    * Search a place / address / landmark  (on-screen QWERTY -> Open-Meteo)
//    * Enter lat / lon                       (reuses mapview's numeric keypad)
//    * GPS location                          (center on the live fix)
//    * Home                                  (your saved main location)
//    * Favorites                             (saved named locations in NVS)
//    * Save current as favorite
//
//  Include LAST in the .ino (after provision.h): needs geocodeCity() from
//  provision.h, the map globals + mapGoto() from mapview.h, and prefs favorites.
// =============================================================================
#pragma once
#include "mapview.h"
#include "provision.h"     // geocodeCity()
#include "prefs.h"         // favorites
#include "gps.h"
#include "ui.h"            // mapTouch()
#include "theme.h"

// ---- small helper: set the map + flight-query center ------------------------
static void flGoTo(double la,double lo){
  g_mapLat=la; g_mapLon=lo; g_app.qLat=la; g_app.qLon=lo;
}

// ---- on-screen QWERTY -------------------------------------------------------
static const char* FL_KB[4] = { "1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM.,-" };
static const int   FL_X0[4] = { 0, 0, 16, 0 };            // row start x (row 2 centered)

static void flKbDraw(const char* title,const char* val){
  const Palette& p=PAL();
  tft.fillScreen(p.bg);
  tft.setTextDatum(TC_DATUM); tft.setTextFont(2); tft.setTextColor(p.accent,p.bg);
  tft.drawString(title,160,2);
  tft.drawRect(6,20,308,24,p.grid);
  tft.setTextDatum(ML_DATUM); tft.setTextColor(p.text,p.bg); tft.setTextFont(2);
  char show[48]; snprintf(show,sizeof show,"%s_",val); tft.drawString(show,12,32);
  tft.setTextDatum(MC_DATUM);
  for(int r=0;r<4;r++){ int n=strlen(FL_KB[r]); int y=48+r*32;
    for(int c=0;c<n;c++){ int x=FL_X0[r]+c*32;
      tft.drawRoundRect(x,y,30,30,3,p.grid);
      tft.setTextColor(p.text,p.bg);
      char s[2]={FL_KB[r][c],0}; tft.drawString(s,x+15,y+15);
    }
  }
  int y=178;
  tft.setTextColor(p.text,p.bg);
  tft.drawRoundRect(0,y,150,34,4,p.grid);  tft.drawString("SPACE",75,y+17);
  tft.drawRoundRect(152,y,54,34,4,p.grid); tft.drawString("DEL",179,y+17);
  tft.drawRoundRect(208,y,54,34,4,p.grid); tft.setTextColor(p.center,p.bg); tft.drawString("OK",235,y+17);
  tft.drawRoundRect(264,y,54,34,4,p.grid); tft.setTextColor(p.warn,p.bg);   tft.drawString("X",291,y+17);
}
// 0 none / letter / ' ' space / 8 del / 13 ok / 27 exit
static int flKbHit(int mx,int my){
  if(my>=178){ if(mx<150) return ' '; if(mx<206) return 8; if(mx<262) return 13; return 27; }
  if(my<48) return 0;
  int r=(my-48)/32; if(r<0||r>3) return 0;
  int y=48+r*32; if(my>y+30) return 0;
  int n=strlen(FL_KB[r]);
  int c=(mx-FL_X0[r])/32; if(c<0||c>=n) return 0;
  int x=FL_X0[r]+c*32; if(mx<x||mx>x+30) return 0;
  return FL_KB[r][c];
}
// Blocking text entry. Returns true on OK (with text), false on X.
static bool flKeyboard(const char* title,char* out,size_t cap){
  out[0]=0; int len=0; flKbDraw(title,out);
  while(true){
    int mx,my;
    if(!mapTouch(mx,my)){ delay(5); continue; }
    int k=flKbHit(mx,my);
    uint32_t t=millis(); while(touch.touched()&&millis()-t<400) delay(8);
    if(k==0) continue;
    if(k==27) return false;
    if(k==13) return len>0;
    if(k==8){ if(len) out[--len]=0; }
    else if(len<(int)cap-1){ out[len++]=(char)k; out[len]=0; }
    flKbDraw(title,out);
  }
}

// ---- search a place / address / landmark and jump there ---------------------
static bool flSearch(){
  char q[40];
  if(!flKeyboard("SEARCH PLACE / ADDRESS",q,sizeof q)) return false;
  const Palette& p=PAL();
  tft.fillScreen(p.bg); tft.setTextDatum(MC_DATUM); tft.setTextFont(2);
  tft.setTextColor(p.text,p.bg); tft.drawString("Searching...",160,120);
  double la,lo; char lb[10];
  if(geocodeCity(String(q),la,lo,lb)){
    flGoTo(la,lo);
    if(g_mapZoom<12) g_mapZoom=12;                 // zoom in for a place/landmark
    return true;
  }
  tft.setTextColor(p.warn,p.bg); tft.drawString("Not found - try a city/landmark",160,150);
  delay(1300);
  return false;
}

// ---- save the current map center as a named favorite ------------------------
static void flSaveCurrent(){
  char nm[16];
  if(!flKeyboard("NAME THIS FAVORITE",nm,sizeof nm)) return;
  const Palette& p=PAL();
  tft.fillScreen(p.bg); tft.setTextDatum(MC_DATUM); tft.setTextFont(2);
  bool ok=prefsFavAdd(nm,g_mapLat,g_mapLon);
  tft.setTextColor(ok?p.center:p.warn,p.bg);
  tft.drawString(ok?"Saved to favorites":"Favorites full (max 10)",160,120);
  delay(1100);
}

// ---- favorites list: tap a name to go, [x] to delete ------------------------
static bool flFavorites(){
  while(true){
    const Palette& p=PAL();
    tft.fillScreen(p.bg);
    tft.setTextDatum(TC_DATUM); tft.setTextFont(2); tft.setTextColor(p.accent,p.bg);
    tft.drawString("FAVORITES",160,4);
    int n=prefsFavCount();
    if(n==0){
      tft.setTextDatum(MC_DATUM); tft.setTextFont(2); tft.setTextColor(p.dim,p.bg);
      tft.drawString("none saved yet",160,104);
      tft.setTextFont(1); tft.drawString("use 'Save current' on the FIND menu",160,126);
    } else {
      for(int i=0;i<n;i++){ int y=28+i*18;
        char nm[16]; double la,lo;
        if(!prefsFavGet(i,nm,sizeof nm,la,lo)) continue;
        tft.setTextDatum(TL_DATUM); tft.setTextFont(2); tft.setTextColor(p.text,p.bg);
        tft.drawString(nm,10,y);
        tft.setTextDatum(TR_DATUM); tft.setTextFont(1); tft.setTextColor(p.dim,p.bg);
        char c[24]; snprintf(c,sizeof c,"%.3f,%.3f",la,lo); tft.drawString(c,278,y+3);
        tft.setTextColor(p.warn,p.bg); tft.drawString("[x]",316,y+3);
      }
    }
    tft.setTextDatum(TR_DATUM); tft.setTextFont(2); tft.setTextColor(p.text,p.bg);
    tft.drawString("EXIT",312,222);

    int mx,my;
    while(!mapTouch(mx,my)) delay(5);
    uint32_t t=millis(); while(touch.touched()&&millis()-t<400) delay(8);
    if(my>=214 && mx>270) return false;
    if(n>0 && my>=28 && my<28+n*18){
      int idx=(my-28)/18;
      if(idx>=0 && idx<n){
        if(mx>=294){ prefsFavDelete(idx); continue; }     // [x]
        char nm[16]; double la,lo;
        if(prefsFavGet(idx,nm,sizeof nm,la,lo)){ flGoTo(la,lo); return true; }
      }
    }
  }
}

// ---- the FIND menu ----------------------------------------------------------
static const char* FL_MENU[7] = {
  "Search place / address",
  "Enter lat / lon",
  "GPS location",
  "Home",
  "Favorites",
  "Save current as favorite",
  "Cancel" };

// Returns true if the map center changed (caller re-polls flights + redraws).
inline bool findLocMenu(){
  while(true){
    const Palette& p=PAL();
    tft.fillScreen(p.bg);
    tft.setTextDatum(TC_DATUM); tft.setTextFont(2); tft.setTextColor(p.accent,p.bg);
    tft.drawString("FIND LOCATION",160,4);
    tft.setTextDatum(ML_DATUM); tft.setTextFont(2);
    for(int i=0;i<7;i++){ int y=28+i*29;
      bool disabled=(i==2) && !(g_app.gpsOn && g_gpsFix);
      tft.drawRoundRect(10,y,300,25,4,p.grid);
      tft.setTextColor(i==6?p.warn:(disabled?p.dim:p.text),p.bg);
      tft.drawString(FL_MENU[i],20,y+12);
    }
    int mx,my;
    while(!mapTouch(mx,my)) delay(5);
    uint32_t t=millis(); while(touch.touched()&&millis()-t<400) delay(8);
    if(my<28) continue;
    int i=(my-28)/29; if(i<0||i>6) continue;
    int y=28+i*29; if(my>y+25) continue;
    switch(i){
      case 0: if(flSearch()) return true; break;
      case 1: if(mapGoto())  return true; break;        // existing lat/lon keypad
      case 2:
        if(g_app.gpsOn && g_gpsFix){ flGoTo(g_gpsLat,g_gpsLon); return true; }
        else { tft.setTextDatum(TC_DATUM); tft.setTextFont(1); tft.setTextColor(p.warn,p.bg);
               tft.drawString(g_app.gpsOn?"GPS acquiring...":"GPS off - tap G on the radar",160,226);
               delay(1200); }
        break;
      case 3: flGoTo(g_app.homeLat,g_app.homeLon); return true;
      case 4: if(flFavorites()) return true; break;
      case 5: flSaveCurrent(); break;
      case 6: return false;                              // cancel
    }
  }
}
