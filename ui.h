// =============================================================================
//  God's Eye CYD  -  rendering + touch UI
//  North-up PPI radar, intelligence HUD, telemetry card, layer menu, CCTV view.
//  Draws direct-to-TFT with a full scope repaint per refresh (a few Hz).
// =============================================================================
#pragma once
#include <time.h>
#include "theme.h"
#include "geo.h"
// NOTE: cctv.h and mapview.h are included AFTER this file (they use mapTouch()).

// ---- Touch calibration (raw XPT2046 -> screen). Tune if taps are off. --------
#define TS_MINX 200
#define TS_MAXX 3700
#define TS_MINY 240
#define TS_MAXY 3800

// ---- On-screen blip registry (rebuilt every scope repaint, for hit-testing) --
struct Blip { int16_t x,y; uint8_t layer; int16_t idx; };
static Blip  s_blip[96];
static int   s_blipN = 0;
static float s_sweep = 0;

// ---------------------------------------------------------------- primitives --
static void triGlyph(int cx,int cy,float hdgDeg,uint16_t col,int sz){
  float a=hdgDeg*D2R, s=sin(a), c=cos(a);
  // nose, left tail, right tail
  int nx=cx+ (int)( s*sz),        ny=cy+ (int)(-c*sz);
  int lx=cx+ (int)(-c*sz*0.6 - s*sz*0.5), ly=cy+ (int)(-s*sz*0.6 + c*sz*0.5);
  int rx=cx+ (int)( c*sz*0.6 - s*sz*0.5), ry=cy+ (int)( s*sz*0.6 + c*sz*0.5);
  tft.fillTriangle(nx,ny,lx,ly,rx,ry,col);
}
static void addBlip(int x,int y,uint8_t layer,int idx){
  if(s_blipN<(int)(sizeof(s_blip)/sizeof(s_blip[0])))
    s_blip[s_blipN++]=(Blip){(int16_t)x,(int16_t)y,layer,(int16_t)idx};
}

// -------------------------------------------------------------------- topbar --
static void drawTopBar(){
  const Palette& p=PAL();
  tft.fillRect(0,0,320,TOP_H,p.bg);
  tft.drawFastHLine(0,TOP_H,320,p.grid);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(p.accent,p.bg); tft.setTextFont(1);
  tft.drawString("GOD'S EYE",3,4);
  tft.setTextColor(p.dim,p.bg);
  tft.drawString(g_app.status,74,4);

  // UTC clock
  char t[10]="--:--:--";
  time_t now=time(nullptr);
  if(now>1700000000){ struct tm* g=gmtime(&now); strftime(t,sizeof(t),"%H:%M:%S",g); }
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(p.text,p.bg);
  tft.drawString(t,300,4);
  // link dot: green if a poll landed recently
  bool live = g_app.wifiUp && (millis()-g_app.lastRx < 20000);
  tft.fillCircle(312,7,3, live?p.center:p.warn);
}

// --------------------------------------------------------------------- scope --
static void drawScope(){
  const Palette& p=PAL();
  s_blipN=0;
  // scope backdrop
  tft.fillRect(0,TOP_H,PANEL_X,240-TOP_H-BOT_H,p.bg);

  // range rings (3) + labels
  for(int i=1;i<=3;i++){
    int rr=SCOPE_R*i/3;
    tft.drawCircle(SCOPE_CX,SCOPE_CY,rr,p.grid);
    int nm=g_app.rangeNm*i/3;
    tft.setTextFont(1); tft.setTextDatum(TL_DATUM); tft.setTextColor(p.grid,p.bg);
    tft.drawNumber(nm,SCOPE_CX+2,SCOPE_CY-rr+1);
  }
  // cross + bearing ticks
  tft.drawFastHLine(SCOPE_CX-SCOPE_R,SCOPE_CY,SCOPE_R*2,p.grid);
  tft.drawFastVLine(SCOPE_CX,SCOPE_CY-SCOPE_R,SCOPE_R*2,p.grid);
  tft.setTextColor(p.dim,p.bg); tft.setTextDatum(TC_DATUM);
  tft.drawString("N",SCOPE_CX,SCOPE_CY-SCOPE_R-1);

  // sweep line (eye candy)
  s_sweep += 7; if(s_sweep>=360)s_sweep-=360;
  float sa=s_sweep*D2R;
  tft.drawLine(SCOPE_CX,SCOPE_CY,
               SCOPE_CX+(int)(sin(sa)*SCOPE_R),
               SCOPE_CY-(int)(cos(sa)*SCOPE_R), p.ring);

  // HOME marker
  tft.fillCircle(SCOPE_CX,SCOPE_CY,2,p.center);
  tft.setTextColor(p.center,p.bg); tft.setTextDatum(TL_DATUM);
  tft.drawString(g_app.homeLabel,SCOPE_CX+4,SCOPE_CY+3);

  int sx,sy;
  // ---- QUAKES (drawn first, under aircraft) ----
  if(g_app.layerOn[L_QUAKES]){
    for(int i=0;i<g_qkN;i++){ if(!g_qk[i].used) continue;
      if(!projectPPI(g_qk[i].lat,g_qk[i].lon,sx,sy)) continue;
      int r=clampi((int)(g_qk[i].mag*1.6),2,10);
      tft.drawCircle(sx,sy,r,p.quake);
      tft.fillCircle(sx,sy,1,p.quake);
      addBlip(sx,sy,L_QUAKES,i);
    }
  }
  // ---- SATS ----
  if(g_app.layerOn[L_SATS]){
    for(size_t i=0;i<SATS_MAX;i++){ if(!g_sat[i].used) continue;
      if(!projectPPI(g_sat[i].lat,g_sat[i].lon,sx,sy)) continue;
      tft.fillRect(sx-2,sy-2,4,4,p.sat);
      tft.drawFastHLine(sx-4,sy,9,p.sat);
      addBlip(sx,sy,L_SATS,(int)i);
    }
  }
  // ---- FLIGHTS (on top) ----
  if(g_app.layerOn[L_FLIGHTS]){
    for(int i=0;i<g_acN;i++){ if(!g_ac[i].used) continue;
      if(!projectPPI(g_ac[i].lat,g_ac[i].lon,sx,sy)) continue;
      uint16_t col = g_ac[i].mil ? p.mil : p.text;
      bool sel = (g_app.selLayer==L_FLIGHTS && g_app.selIndex==i);
      triGlyph(sx,sy,g_ac[i].track,col,sel?6:4);
      if(sel){ tft.drawCircle(sx,sy,9,p.accent);
               // fading track stub in the reciprocal of heading
               float a=(g_ac[i].track+180)*D2R;
               tft.drawLine(sx,sy,sx+(int)(sin(a)*12),sy-(int)(cos(a)*12),p.accent); }
      addBlip(sx,sy,L_FLIGHTS,i);
    }
  }
}

// --------------------------------------------------------------------- panel --
static void panelLine(int& y,const char* k,const char* v,uint16_t kc,uint16_t vc){
  tft.setTextFont(1); tft.setTextDatum(TL_DATUM);
  tft.setTextColor(kc,PAL().bg); tft.drawString(k,PANEL_X+2,y);
  tft.setTextColor(vc,PAL().bg); tft.drawString(v,PANEL_X+2,y+9);
  y+=20;
}

static void drawPanel(){
  const Palette& p=PAL();
  tft.fillRect(PANEL_X,TOP_H,PANEL_W,240-TOP_H-BOT_H,p.bg);
  tft.drawFastVLine(PANEL_X,TOP_H,240-TOP_H-BOT_H,p.grid);

  // range chip (tap to cycle)
  tft.fillRect(PANEL_X+2,TOP_H+2,PANEL_W-4,16,p.grid);
  char rng[12]; snprintf(rng,sizeof(rng),"%dNM",g_app.rangeNm);
  tft.setTextDatum(MC_DATUM); tft.setTextColor(p.text,p.grid); tft.setTextFont(1);
  tft.drawString(rng,PANEL_X+PANEL_W/2,TOP_H+10);

  int y=TOP_H+24;
  bool haveSel = (g_app.selIndex>=0);

  if(haveSel && g_app.selLayer==L_FLIGHTS && g_app.selIndex<g_acN){
    Aircraft& a=g_ac[g_app.selIndex];
    tft.setTextDatum(TL_DATUM); tft.setTextFont(2);
    tft.setTextColor(p.accent,p.bg);
    tft.drawString(a.flight[0]?a.flight:a.hex,PANEL_X+2,y); y+=18;
    char b[24];
    tft.setTextFont(1);
    snprintf(b,sizeof(b),"ALT %ld",(long)a.altFt); panelLine(y,"",a.altFt<0?"ALT GND":b,p.dim,p.text);
    snprintf(b,sizeof(b),"%d kt",a.gs);      panelLine(y,"GS",b,p.dim,p.text);
    snprintf(b,sizeof(b),"%03d\xF7",a.track);panelLine(y,"TRK",b,p.dim,p.text);
    float rr=haversineNm(g_app.homeLat,g_app.homeLon,a.lat,a.lon);
    snprintf(b,sizeof(b),"%.0f nm",rr);      panelLine(y,"RNG",b,p.dim,p.text);
    if(a.mil){ tft.setTextColor(p.mil,p.bg); tft.drawString("* MILITARY",PANEL_X+2,y); }
  }
  else if(haveSel && g_app.selLayer==L_QUAKES && g_app.selIndex<g_qkN){
    Quake& q=g_qk[g_app.selIndex];
    tft.setTextFont(2); tft.setTextColor(p.quake,p.bg);
    char m[10]; snprintf(m,sizeof(m),"M %.1f",q.mag);
    tft.drawString(m,PANEL_X+2,y); y+=18;
    tft.setTextFont(1); char b[24];
    snprintf(b,sizeof(b),"%.0f km",q.depthKm); panelLine(y,"DEPTH",b,p.dim,p.text);
    // place wrapped to 2 short lines
    tft.setTextColor(p.text,p.bg);
    String pl=q.place; 
    tft.drawString(pl.substring(0,14),PANEL_X+2,y); y+=10;
    if(pl.length()>14) tft.drawString(pl.substring(14,28),PANEL_X+2,y);
  }
  else if(haveSel && g_app.selLayer==L_SATS && g_app.selIndex<(int)SATS_MAX){
    SatObj& s=g_sat[g_app.selIndex];
    tft.setTextFont(2); tft.setTextColor(p.sat,p.bg);
    tft.drawString(s.name,PANEL_X+2,y); y+=18;
    tft.setTextFont(1); char b[24];
    snprintf(b,sizeof(b),"%.0f km",s.altKm); panelLine(y,"ALT",b,p.dim,p.text);
    snprintf(b,sizeof(b),"%.1f",s.lat);      panelLine(y,"LAT",b,p.dim,p.text);
    snprintf(b,sizeof(b),"%.1f",s.lon);      panelLine(y,"LON",b,p.dim,p.text);
  }
  else {
    // summary
    char b[16];
    snprintf(b,sizeof(b),"%d [%s]",g_acN,g_flDiag); panelLine(y,"FLIGHTS",g_app.layerOn[L_FLIGHTS]?b:"off",p.dim,p.text);
    snprintf(b,sizeof(b),"%d",g_qkN); panelLine(y,"QUAKES", g_app.layerOn[L_QUAKES]?b:"off",p.dim,p.quake);
    int sc=0; for(size_t i=0;i<SATS_MAX;i++) if(g_sat[i].used)sc++;
    snprintf(b,sizeof(b),"%d",sc);    panelLine(y,"SATS",   g_app.layerOn[L_SATS]?b:"off",p.dim,p.sat);
    if(g_app.layerOn[L_LAUNCHES] && g_lxN>0){
      tft.setTextColor(p.launch,p.bg); tft.setTextFont(1);
      tft.drawString("NEXT LAUNCH",PANEL_X+2,y); y+=10;
      tft.setTextColor(p.text,p.bg);
      String nm=g_lx[0].name; tft.drawString(nm.substring(0,13),PANEL_X+2,y); y+=10;
      time_t now=time(nullptr);
      if(now>1700000000 && g_lx[0].net>now){
        long d=g_lx[0].net-now; char c[16];
        snprintf(c,sizeof(c),"T-%ldh%02ldm",d/3600,(d%3600)/60);
        tft.setTextColor(p.launch,p.bg); tft.drawString(c,PANEL_X+2,y);
      }
    }
  }
}

// ------------------------------------------------------------------ botbar ----
#define LEGEND_X_MAX 72          // tap x < this on the bottom bar = a layer toggle
static void drawBotBar(){
  const Palette& p=PAL();
  int y=240-BOT_H;
  tft.fillRect(0,y,320,BOT_H,p.bg);
  tft.drawFastHLine(0,y,320,p.grid);
  // layer legend — tappable toggles (F Q S L C). Zone: x < LEGEND_X_MAX.
  const char* L="FQSLC";
  tft.setTextFont(1); tft.setTextDatum(TL_DATUM);
  for(int i=0;i<5;i++){ char c[2]={L[i],0};
    tft.setTextColor(g_app.layerOn[i]?p.center:p.dim,p.bg);   // enum order == legend order
    tft.drawString(c,6+i*13,y+4);
  }
  // theme name (center)
  tft.setTextDatum(TC_DATUM); tft.setTextColor(p.accent,p.bg);
  tft.drawString(p.name,160,y+4);
  // buttons (right-aligned): MAP | MENU | CCTV
  tft.setTextDatum(TR_DATUM); tft.setTextColor(p.text,p.bg);
  tft.drawString("MAP", 205,y+4);
  tft.drawString("MENU",258,y+4);
  tft.drawString("CCTV",318,y+4);
}

static void drawRadar(){ drawTopBar(); drawScope(); drawPanel(); drawBotBar(); }

// -------------------------------------------------------------------- menu -----
static void drawMenu(){
  const Palette& p=PAL();
  tft.fillScreen(p.bg);
  tft.setTextFont(2); tft.setTextDatum(TC_DATUM); tft.setTextColor(p.accent,p.bg);
  tft.drawString("LAYERS / OPTICS",160,8);
  const char* names[L_COUNT]={"Flights (ADS-B)","Earthquakes (USGS)","Satellites (SGP4)","Launches (LL2)","CCTV snapshots"};
  tft.setTextFont(2); tft.setTextDatum(TL_DATUM);
  for(int i=0;i<L_COUNT;i++){
    int ry=34+i*30;
    tft.drawRoundRect(12,ry,296,26,4,p.grid);
    tft.setTextColor(p.text,p.bg); tft.drawString(names[i],20,ry+5);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(g_app.layerOn[i]?p.center:p.warn,p.bg);
    tft.drawString(g_app.layerOn[i]?"ON":"OFF",300,ry+5);
    tft.setTextDatum(TL_DATUM);
  }
  int ry=34+L_COUNT*30;
  tft.drawRoundRect(12,ry,94,26,4,p.grid);
  tft.setTextColor(p.accent,p.bg); tft.drawString("OPTIC",20,ry+5);
  tft.drawRoundRect(113,ry,94,26,4,p.grid);
  tft.setTextColor(p.warn,p.bg);   tft.drawString("SETUP",121,ry+5);
  tft.drawRoundRect(214,ry,94,26,4,p.grid);
  tft.setTextColor(p.accent,p.bg); tft.drawString("BACK",222,ry+5);
  // current location, small, under the buttons
  tft.setTextFont(1); tft.setTextDatum(TC_DATUM); tft.setTextColor(p.dim,p.bg);
  char loc[40]; snprintf(loc,sizeof(loc),"HOME %s  %.2f, %.2f",
                         g_app.homeLabel,g_app.homeLat,g_app.homeLon);
  tft.drawString(loc,160,ry+30);
  tft.setTextFont(2); tft.setTextDatum(TL_DATUM);
}

// (CCTV rendering lives in cctv.h — a scrollable list + live MJPEG viewer.)

// ================================================================== TOUCH =====
static bool mapTouch(int& mx,int& my){
  // NOTE: no tirqTouched() gate — many 2-USB CYDs don't wire the touch IRQ,
  // so we read the panel directly and judge a real press by pressure (z).
  if(!touch.touched()) return false;
  TS_Point pt=touch.getPoint();
  if(pt.z<200) return false;
  // rotation 1 (landscape). Adjust if your unit is mirrored.
  mx = map(pt.x, TS_MINX, TS_MAXX, 0, 320);
  my = map(pt.y, TS_MINY, TS_MAXY, 0, 240);
  mx = clampi(mx,0,319); my=clampi(my,0,239);
  return true;
}

// pick nearest blip within 14px of a tap
static void selectAt(int mx,int my){
  int best=-1; long bd=14*14;
  for(int i=0;i<s_blipN;i++){
    long dx=mx-s_blip[i].x, dy=my-s_blip[i].y, d=dx*dx+dy*dy;
    if(d<bd){ bd=d; best=i; }
  }
  if(best<0){ g_app.selIndex=-1; }
  else { g_app.selLayer=s_blip[best].layer; g_app.selIndex=s_blip[best].idx; }
}
