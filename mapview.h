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

// ---- the map bar (bottom): [-] [+]  center/zoom  [HOME] [BACK] ---------------
static void drawMapBar(){
  const Palette& p=PAL();
  int by=240-MAP_BAR_H;
  tft.fillRect(0,by,320,MAP_BAR_H,p.bg);
  tft.drawFastHLine(0,by,320,p.grid);
  tft.setTextFont(2); tft.setTextDatum(MC_DATUM);
  tft.setTextColor(p.accent,p.bg);
  tft.drawString("-", 20,by+MAP_BAR_H/2);
  tft.drawString("+", 60,by+MAP_BAR_H/2);
  tft.setTextColor(p.text,p.bg);
  tft.drawString("HOME",250,by+MAP_BAR_H/2);
  tft.drawString("BACK",300,by+MAP_BAR_H/2);
  // center lat/lon + zoom
  tft.setTextFont(1); tft.setTextColor(p.dim,p.bg);
  char c[30]; snprintf(c,sizeof(c),"%.2f,%.2f z%d",g_mapLat,g_mapLon,g_mapZoom);
  tft.drawString(c,160,by+MAP_BAR_H/2);
}

// ---- overlay live flights on the imagery ------------------------------------
static void drawMapFlights(){
  if(!g_app.layerOn[L_FLIGHTS]) return;
  const Palette& p=PAL(); int z=g_mapZoom;
  double cxpx=lonToPx(g_mapLon,z), cypx=latToPx(g_mapLat,z);
  for(int i=0;i<g_acN;i++){ if(!g_ac[i].used) continue;
    int sx=(int)lround(lonToPx(g_ac[i].lon,z)-cxpx+160);
    int sy=(int)lround(latToPx(g_ac[i].lat,z)-cypx+120);
    if(sx<0||sx>=320||sy<0||sy>=240-MAP_BAR_H) continue;
    triGlyph(sx,sy,g_ac[i].track, g_ac[i].mil?p.mil:p.text, 4);
    tft.drawCircle(sx,sy,5, g_ac[i].mil?p.mil:p.accent);  // ring for contrast on imagery
  }
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
