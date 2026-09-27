// =============================================================================
//  God's Eye CYD  -  CCTV: scrollable list + live MJPEG viewer
//  Streams public MJPEG webcams over plain HTTP (no TLS = lighter heap, and
//  these cams are http://). One JPEG frame is carved out of the multipart
//  stream by scanning for SOI (FF D8) .. EOI (FF D9) and decoded with
//  TJpg_Decoder — the same technique RaspyJack's viewer uses.
//
//  Include AFTER ui.h (uses mapTouch()); cctv_list.h holds the camera table.
// =============================================================================
#pragma once
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include "net.h"
#include "theme.h"
#include "cctv_list.h"

#define MJPG_CAP     70000UL     // max bytes for one JPEG frame
#define MJPG_ROWS    11          // camera list rows per page
#define MJPG_ROW_H   18

static uint8_t*  s_mj = nullptr; // frame accumulator (alloc while viewing)
static size_t    s_mjLen = 0;
static WiFiClient s_mjClient;

// ---- decode target: blit frame blocks, clipped above the caption strip ------
static bool cctvOut(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t* bmp){
  if(y>=216) return false;
  tft.pushImage(x,y,w,h,bmp);
  return true;
}
inline void cctvBegin(){ TJpgDec.setSwapBytes(true); TJpgDec.setCallback(cctvOut); }

// ---- split "http://host:port/path" -----------------------------------------
static bool parseUrl(const char* url,String& host,uint16_t& port,String& path){
  String u=url;
  if(!u.startsWith("http://")) return false;
  u=u.substring(7);
  int slash=u.indexOf('/');
  String hostport = slash<0 ? u : u.substring(0,slash);
  path = slash<0 ? "/" : u.substring(slash);
  int colon=hostport.indexOf(':');
  if(colon<0){ host=hostport; port=80; }
  else { host=hostport.substring(0,colon); port=(uint16_t)hostport.substring(colon+1).toInt(); }
  return host.length()>0;
}

// ---- open the stream, skip response headers --------------------------------
static bool mjpgOpen(const char* url){
  String host,path; uint16_t port;
  if(!parseUrl(url,host,port,path)) return false;
  s_mjClient.stop();
  if(!s_mjClient.connect(host.c_str(),port,6000)) return false;
  s_mjClient.printf("GET %s HTTP/1.1\r\nHost: %s\r\n"
                    "User-Agent: GodsEyeCYD\r\nConnection: keep-alive\r\n\r\n",
                    path.c_str(),host.c_str());
  uint32_t t0=millis();
  int nl=0;                          // consecutive newlines -> end of headers
  while(millis()-t0<6000){
    while(s_mjClient.available()){
      int c=s_mjClient.read();
      if(c=='\r') continue;
      if(c=='\n'){ if(++nl>=2) return true; }
      else nl=0;
    }
    if(!s_mjClient.connected() && !s_mjClient.available()) return false;
    delay(2);
  }
  return false;
}

// ---- pull one complete JPEG frame into s_mj; returns length or 0 ------------
static size_t mjpgFrame(uint32_t budgetMs=5000){
  uint32_t t0=millis();
  bool haveSOI=false; s_mjLen=0; int prev=-1;
  while(millis()-t0<budgetMs){
    if(!s_mjClient.connected() && !s_mjClient.available()) return 0;
    while(s_mjClient.available()){
      int c=s_mjClient.read(); if(c<0) break;
      if(!haveSOI){
        if(prev==0xFF && c==0xD8){ haveSOI=true; s_mj[0]=0xFF; s_mj[1]=0xD8; s_mjLen=2; }
        prev=c;
      } else {
        if(s_mjLen<MJPG_CAP) s_mj[s_mjLen++]=(uint8_t)c;
        else { haveSOI=false; s_mjLen=0; prev=-1; }      // overflow, resync
        if(s_mjLen>=2 && s_mj[s_mjLen-2]==0xFF && s_mj[s_mjLen-1]==0xD9)
          return s_mjLen;                                 // EOI -> complete frame
      }
      t0=millis();
    }
    delay(1);
  }
  return 0;
}

// ---- caption strip during playback -----------------------------------------
static void cctvCaption(const char* txt,uint16_t col){
  const Palette& p=PAL();
  tft.fillRect(0,216,320,24,p.bg); tft.drawFastHLine(0,216,320,p.grid);
  tft.setTextFont(2); tft.setTextDatum(TL_DATUM); tft.setTextColor(col,p.bg);
  tft.drawString(txt,6,219);
  tft.setTextFont(1); tft.setTextDatum(TR_DATUM); tft.setTextColor(p.dim,p.bg);
  char n[26]; snprintf(n,sizeof(n),"%d/%d  PREV NEXT BACK",g_app.cctvIdx+1,CCTV_COUNT);
  tft.drawString(n,315,222);
}

// ---- scrollable camera list -------------------------------------------------
inline void cctvDrawList(){
  const Palette& p=PAL();
  tft.fillScreen(p.bg);
  tft.setTextFont(2); tft.setTextDatum(TC_DATUM); tft.setTextColor(p.accent,p.bg);
  tft.drawString("CCTV  -  tap a camera",160,3);
  tft.setTextDatum(TL_DATUM);
  int top=g_app.cctvScroll;
  for(int i=0;i<MJPG_ROWS;i++){
    int idx=top+i; if(idx>=CCTV_COUNT) break;
    int y=22+i*MJPG_ROW_H;
    bool sel=(idx==g_app.cctvIdx);
    if(sel) tft.fillRect(0,y,320,MJPG_ROW_H,p.grid);
    tft.setTextColor(sel?p.center:p.text, sel?p.grid:p.bg);
    tft.drawString(CCTV_CAMS[idx].label,6,y+2);
  }
  const int fy=22+MJPG_ROWS*MJPG_ROW_H;
  tft.drawFastHLine(0,fy,320,p.grid);
  tft.setTextFont(2); tft.setTextDatum(TL_DATUM); tft.setTextColor(p.accent,p.bg);
  tft.drawString("UP",8,fy+3); tft.drawString("DOWN",50,fy+3);
  tft.setTextDatum(TR_DATUM); tft.setTextColor(p.text,p.bg);
  tft.drawString("RADAR",315,fy+3);
}
#define CCTV_FOOTER_Y (22+MJPG_ROWS*MJPG_ROW_H)

// ---- play the selected camera (blocking; polls touch). ----------------------
//  Returns 0 = go to RADAR, 1 = back to LIST.
inline int cctvPlay(){
  cctvBegin();
  if(!s_mj) s_mj=(uint8_t*)malloc(MJPG_CAP);
  if(!s_mj) return 1;

  while(true){
    const CctvCam& cam=CCTV_CAMS[clampi(g_app.cctvIdx,0,CCTV_COUNT-1)];
    tft.fillScreen(PAL().bg);
    tft.setTextDatum(MC_DATUM); tft.setTextFont(2); tft.setTextColor(PAL().dim,PAL().bg);
    tft.drawString("connecting...",160,108);
    cctvCaption(cam.label,PAL().accent);

    bool ok=mjpgOpen(cam.url);
    int action=-1;                       // 0 radar, 1 list, 2 prev, 3 next
    uint32_t lastFrame=millis();

    if(ok){
      while(action<0){
        size_t len=mjpgFrame(4000);
        if(len>1000){
          uint16_t jw=0,jh=0; TJpgDec.getJpgSize(&jw,&jh,s_mj,len);
          uint8_t sc=1; while(sc<8 && (jw/sc>320 || jh/sc>216)) sc<<=1;
          TJpgDec.setJpgScale(sc);
          int ox=(320-jw/sc)/2; if(ox<0)ox=0;
          int oy=(216-jh/sc)/2; if(oy<0)oy=0;
          if(ox>0||oy>0) tft.fillRect(0,0,320,216,PAL().bg);
          TJpgDec.drawJpg(ox,oy,s_mj,len);
          cctvCaption(cam.label,PAL().center);
          g_app.lastRx=millis(); lastFrame=millis();
        } else if(millis()-lastFrame>6000){
          break;                         // stalled -> offline handling below
        }
        int mx,my;
        if(mapTouch(mx,my)){
          if(my>=216){ if(mx>250) action=1; else if(mx<110) action=2; else action=3; }
          else action=3;                 // tap image -> next
          uint32_t t=millis(); while(touch.touched()&&millis()-t<500)delay(10);
        }
      }
    }

    if(action<0){                        // offline / stalled: prompt
      tft.fillScreen(PAL().bg);
      tft.setTextDatum(MC_DATUM); tft.setTextColor(PAL().warn,PAL().bg); tft.setTextFont(2);
      tft.drawString("camera offline",160,100);
      tft.setTextColor(PAL().dim,PAL().bg);
      tft.drawString("tap: next   BACK: list",160,124);
      cctvCaption(cam.label,PAL().warn);
      uint32_t t0=millis();
      while(action<0 && millis()-t0<15000){
        int mx,my;
        if(mapTouch(mx,my)){ action=(my>=216&&mx>250)?1:3;
          uint32_t t=millis(); while(touch.touched()&&millis()-t<500)delay(10); }
        delay(20);
      }
      if(action<0) action=3;             // auto-advance if untouched
    }

    s_mjClient.stop();
    if(action==1){ free(s_mj); s_mj=nullptr; return 1; }                    // LIST
    if(action==2) g_app.cctvIdx=(g_app.cctvIdx+CCTV_COUNT-1)%CCTV_COUNT;    // PREV
    else          g_app.cctvIdx=(g_app.cctvIdx+1)%CCTV_COUNT;               // NEXT
  }
}
