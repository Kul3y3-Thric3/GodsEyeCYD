// =============================================================================
//  God's Eye CYD  -  satellite MAP mode
//  Pannable / zoomable 2D satellite imagery (Esri World Imagery, keyless) with
//  live flights drawn on top. The CYD analog of God's Eye View's "look down at
//  anywhere on Earth" — a slow slippy map (one JPEG tile fetched+decoded at a
//  time), not a smooth 3D globe, which the hardware can't do.
//
//  Include AFTER ui.h (uses triGlyph()).
// =============================================================================
#pragma once
#include <TJpg_Decoder.h>
#include "net.h"
#include "geo.h"
#include "theme.h"

#define MAP_MIN_Z     3
#define MAP_MAX_Z     17
#define MAP_BAR_H     20
#define TILE_BUF_CAP  45000UL     // per-tile JPEG ceiling (bytes)

static double g_mapLat  = 0;
static double g_mapLon  = 0;
static int    g_mapZoom = 11;
static bool   g_mapInit = false;

// ---- Web Mercator: global pixel space is 256 * 2^zoom pixels square ----------
static double mapWorldPx(int z){ return 256.0 * (double)(1UL << z); }
static double lonToPx(double lon,int z){ return (lon+180.0)/360.0 * mapWorldPx(z); }
static double latToPx(double lat,int z){
  double r=lat*D2R;
  double y=(1.0 - log(tan(r)+1.0/cos(r))/M_PI)/2.0;
  return y*mapWorldPx(z);
}
static double pxToLon(double px,int z){ return px/mapWorldPx(z)*360.0 - 180.0; }
static double pxToLat(double px,int z){
  double y=px/mapWorldPx(z);
  return atan(sinh(M_PI*(1.0-2.0*y)))*R2D;
}

// TJpg_Decoder pushes decoded blocks here; coords are already absolute (we pass
// the tile's screen origin to drawJpg). TFT_eSPI::pushImage clips to the screen.
static bool mapTftOutput(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t* bmp){
  if(x>=320 || y>=240-MAP_BAR_H || x+w<=0 || y+h<=0) return true;  // off-screen/under bar
  tft.pushImage(x,y,w,h,bmp);
  return true;
}

// ---- the map bar (bottom): [-] [+] [GO] [LOC]  center/zoom  [HOME] [BACK] ----
//  Touch zones (handled in the .ino): <32 zoom out · 32-66 zoom in ·
//  66-104 GO (keypad) · 104-150 LOC (center on GPS) · 236-284 HOME · 284+ BACK ·
//  elsewhere on the map = recenter on the tapped point.
static void drawMapBar(){
  const Palette& p=PAL();
  int by=240-MAP_BAR_H, cy=by+MAP_BAR_H/2;
  tft.fillRect(0,by,320,MAP_BAR_H,p.bg);
  tft.drawFastHLine(0,by,320,p.grid);
  tft.setTextFont(2); tft.setTextDatum(MC_DATUM);
  tft.setTextColor(p.accent,p.bg);
  tft.drawString("-", 16,cy);
  tft.drawString("+", 48,cy);
  tft.drawString("GO",84,cy);
  // LOC lit only when GPS has a fix
  tft.setTextColor(g_app.gpsOn && g_gpsFix ? p.center : p.dim, p.bg);
  tft.drawString("LOC",124,cy);
  tft.setTextColor(p.text,p.bg);
  tft.drawString("HOME",258,cy);
  tft.drawString("BACK",302,cy);
  // center lat/lon + zoom
  tft.setTextFont(1); tft.setTextColor(p.dim,p.bg);
  char c[30]; snprintf(c,sizeof(c),"%.2f,%.2f z%d",g_mapLat,g_mapLon,g_mapZoom);
  tft.drawString(c,192,cy);
}

// ---- draw the GPS "you are here" marker on the map, if a fix is available ----
static void drawMapGps(){
  if(!(g_app.gpsOn && g_gpsFix)) return;
  const Palette& p=PAL(); int z=g_mapZoom;
  double cxpx=lonToPx(g_mapLon,z), cypx=latToPx(g_mapLat,z);
  int sx=(int)lround(lonToPx(g_gpsLon,z)-cxpx+160);
  int sy=(int)lround(latToPx(g_gpsLat,z)-cypx+120);
  if(sx<-8||sx>328||sy<-8||sy>240-MAP_BAR_H) return;
  tft.drawCircle(sx,sy,6,p.center); tft.drawCircle(sx,sy,3,p.center);
  tft.drawLine(sx-9,sy,sx+9,sy,p.center); tft.drawLine(sx,sy-9,sx,sy+9,p.center);
  tft.setTextFont(1); tft.setTextDatum(TL_DATUM); tft.setTextColor(p.center,p.bg);
  tft.drawString("YOU",sx+8,sy+4);
}

// ---- on-screen numeric keypad: type LAT then LON to jump the map anywhere ----
static const char* MAPKB[16] = {
  "1","2","3","DEL",  "4","5","6","+/-",  "7","8","9",".",  "CLR","0","OK","EXIT" };

static void mapKbDraw(bool lonPhase,const char* val){
  const Palette& p=PAL();
  tft.fillScreen(p.bg);
  tft.setTextDatum(TC_DATUM); tft.setTextFont(2); tft.setTextColor(p.accent,p.bg);
  tft.drawString(lonPhase?"ENTER LONGITUDE":"ENTER LATITUDE",160,4);
  tft.drawRect(8,24,304,28,p.grid);
  tft.setTextDatum(ML_DATUM); tft.setTextColor(p.text,p.bg);
  tft.drawString(val[0]?val:"_",16,39);
  tft.setTextDatum(TR_DATUM); tft.setTextFont(1); tft.setTextColor(p.dim,p.bg);
  tft.drawString(lonPhase?"-180..180":"-90..90",306,28);
  tft.setTextFont(2); tft.setTextDatum(MC_DATUM);
  for(int i=0;i<16;i++){ int c=i%4,r=i/4, x=8+c*76, y=62+r*44;
    tft.drawRoundRect(x,y,72,40,4,p.grid);
    bool ok=(i==14);
    tft.setTextColor(ok?p.center:p.text,p.bg);
    tft.drawString(MAPKB[i],x+36,y+20);
  }
}
static int mapKbHit(int mx,int my){
  if(my<62) return -1;
  int c=(mx-8)/76, r=(my-62)/44;
  if(c<0||c>3||r<0||r>3) return -1;
  return r*4+c;
}
// Blocking keypad. Returns true if a valid new center was entered.
inline bool mapGoto(){
  char lat[16]={0}, lon[16]={0}; bool lonPhase=false; char* cur=lat;
  mapKbDraw(false,cur);
  while(true){
    int mx,my;
    if(!mapTouch(mx,my)){ delay(5); continue; }
    int k=mapKbHit(mx,my);
    uint32_t t=millis(); while(touch.touched()&&millis()-t<400)delay(8);
    if(k<0) continue;
    const char* key=MAPKB[k]; int len=strlen(cur);
    if(!strcmp(key,"EXIT")) return false;
    else if(!strcmp(key,"CLR")) cur[0]=0;
    else if(!strcmp(key,"DEL")){ if(len) cur[len-1]=0; }
    else if(!strcmp(key,"+/-")){
      if(cur[0]=='-') memmove(cur,cur+1,strlen(cur));
      else if(len<14){ memmove(cur+1,cur,strlen(cur)+1); cur[0]='-'; }
    }
    else if(!strcmp(key,".")){ if(!strchr(cur,'.') && len<14){ cur[len]='.'; cur[len+1]=0; } }
    else if(!strcmp(key,"OK")){
      if(!lonPhase){ lonPhase=true; cur=lon; mapKbDraw(true,cur); continue; }
      double la=atof(lat), lo=atof(lon);
      if(la<-90||la>90||lo<-180||lo>180){
        tft.setTextDatum(TC_DATUM); tft.setTextFont(1); tft.setTextColor(PAL().warn,PAL().bg);
        tft.drawString("out of range - fix it",160,232); delay(1100);
        mapKbDraw(true,cur); continue;
      }
      g_mapLat=la; g_mapLon=lo; g_app.qLat=la; g_app.qLon=lo;
      return true;
    }
    else if(len<14){ cur[len]=key[0]; cur[len+1]=0; }   // a digit
    mapKbDraw(lonPhase,cur);
  }
}

// ---- overlay live flights on the imagery ------------------------------------
static void drawMapFlights(){
  if(!g_app.layerOn[L_FLIGHTS]) return;
  const Palette& p=PAL(); int z=g_mapZoom;
  double cxpx=lonToPx(g_mapLon,z), cypx=latToPx(g_mapLat,z);
  DATA_LOCK();
  for(int i=0;i<g_acN;i++){ if(!g_ac[i].used) continue;
    int sx=(int)lround(lonToPx(g_ac[i].lon,z)-cxpx+160);
    int sy=(int)lround(latToPx(g_ac[i].lat,z)-cypx+120);
    if(sx<0||sx>=320||sy<0||sy>=240-MAP_BAR_H) continue;
    triGlyph(sx,sy,g_ac[i].track, g_ac[i].mil?p.mil:p.text, 4);
    tft.drawCircle(sx,sy,5, g_ac[i].mil?p.mil:p.accent);  // ring for contrast on imagery
  }
  DATA_UNLOCK();
}

// ---- full repaint: fetch every visible tile, then overlay -------------------
inline void drawMap(){
  const Palette& p=PAL(); int z=g_mapZoom;
  tft.fillRect(0,0,320,240-MAP_BAR_H,p.bg);
  tft.setTextDatum(TL_DATUM); tft.setTextFont(1); tft.setTextColor(p.dim,p.bg);
  tft.drawString("loading imagery...",4,4);

  double cxpx=lonToPx(g_mapLon,z), cypx=latToPx(g_mapLat,z);
  double tlx=cxpx-160, tly=cypx-120;               // screen top-left in global px
  int tx0=(int)floor(tlx/256.0), ty0=(int)floor(tly/256.0);
  int tx1=(int)floor((tlx+320)/256.0), ty1=(int)floor((tly+240)/256.0);
  int nmax=(1<<z);

  TJpgDec.setJpgScale(1); TJpgDec.setSwapBytes(true); TJpgDec.setCallback(mapTftOutput);
  uint8_t* buf=(uint8_t*)malloc(TILE_BUF_CAP);

  for(int ty=ty0; ty<=ty1; ty++){
    if(ty<0 || ty>=nmax) continue;                 // no vertical wrap
    for(int tx=tx0; tx<=tx1; tx++){
      int wtx=((tx%nmax)+nmax)%nmax;               // horizontal wrap
      int ox=(int)lround(tx*256 - tlx);
      int oy=(int)lround(ty*256 - tly);
      char url[170];
      snprintf(url,sizeof(url),
        "https://server.arcgisonline.com/ArcGIS/rest/services/"
        "World_Imagery/MapServer/tile/%d/%d/%d", z, ty, wtx);
      if(buf){
        size_t len=fetchBinary(url,buf,TILE_BUF_CAP,9000);
        if(len>500) TJpgDec.drawJpg(ox,oy,buf,len);
        else tft.fillRect(max(0,ox),max(0,oy),256,256,p.grid);
      }
    }
  }
  if(buf) free(buf);
  g_app.lastRx=millis();
  drawMapFlights();
  drawMapGps();
  // center crosshair
  tft.drawFastHLine(154,120,12,p.accent); tft.drawFastVLine(160,114,12,p.accent);
  drawMapBar();
}

// ---- recenter the map on a tapped screen point ------------------------------
static void mapTapRecenter(int mx,int my){
  int z=g_mapZoom;
  double cxpx=lonToPx(g_mapLon,z), cypx=latToPx(g_mapLat,z);
  g_mapLon = pxToLon(cxpx+(mx-160), z);
  g_mapLat = pxToLat(cypx+(my-120), z);
  g_app.qLat=g_mapLat; g_app.qLon=g_mapLon;   // pull flights around the new center
}
