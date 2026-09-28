// =============================================================================
//  God's Eye CYD  -  a native ESP32 reimagining of God's Eye View for the
//  Cheap Yellow Display (ESP32-2432S028, 2-USB / ST7789).
//
//  Live public OSINT feeds on a north-up tactical PPI:
//    ADS-B flights (adsb.lol) - earthquakes (USGS) - satellites (CelesTrak+SGP4)
//    - launches (Launch Library 2) - public CCTV snapshots.
//  All keyless. WiFi + location are set on-device via a phone at first boot.
//
//  Libraries (Library Manager): TFT_eSPI, XPT2046_Touchscreen, ArduinoJson (v7),
//  TJpg_Decoder, Sgp4 (by Hopperpop), WiFiManager (by tzapu).
//  Board: "ESP32 Dev Module".
//  >>> Replace TFT_eSPI/User_Setup.h with the one in this folder first. <<<
// =============================================================================
#include "app_state.h"
#include "layers.h"
#include "gps.h"            // optional GPS on the JST connector (before ui.h)
#include "ui.h"
#include "mapview.h"        // satellite MAP mode (must follow ui.h: uses triGlyph)
#include "cctv.h"           // CCTV list + MJPEG viewer (must follow ui.h: uses mapTouch)
#include "provision.h"      // WiFiManager portal + geocode (also pulls prefs.h)

// ---- Real definitions of the extern globals ---------------------------------
Aircraft  g_ac[FLIGHTS_MAX];   volatile int g_acN=0;
Quake     g_qk[24];            volatile int g_qkN=0;
SatObj    g_sat[SATS_MAX];
Launch    g_lx[6];             volatile int g_lxN=0;
AppState  g_app;

// ---- Networking task (core 0) -----------------------------------------------
// Feeds are polled on a dedicated FreeRTOS task so a slow TLS fetch never
// freezes the UI (touch, sweep, clock stay live). Set USE_NET_TASK 0 to fall
// back to the old inline polling in loop() if you ever need to.
#define USE_NET_TASK 1
SemaphoreHandle_t g_dataMux = nullptr;   // declared extern in app_state.h
static volatile bool g_netBusy  = false; // a background fetch is in flight
static volatile bool g_netPause = false; // pause the task (e.g. during the portal)

TFT_eSPI  tft;

// XPT2046 touch on the CYD's second SPI bus.
#define T_CLK 25
#define T_MISO 39
#define T_MOSI 32
#define T_CS  33
#define T_IRQ 36
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen touch(T_CS, T_IRQ);

// RGB status LED (active LOW)
#define LED_R 4
#define LED_G 16
#define LED_B 17

// BOOT button (GPIO0). Hold at power-on to force the setup portal.
#define BOOT_BTN 0

void setStatus(const char* s){ strlcpy(g_app.status,s,sizeof(g_app.status)); }

static void ledOff(){ digitalWrite(LED_R,HIGH); digitalWrite(LED_G,HIGH); digitalWrite(LED_B,HIGH); }
static void ledLink(bool live){ digitalWrite(LED_G, live?LOW:HIGH); digitalWrite(LED_R, live?HIGH:LOW); }

// ---- Poll / redraw timers (declared early: used by the screen transitions) ---
static uint32_t tF=0,tQ=0,tS=0,tL=0,tTLE=0,tCam=0,tDraw=0,tWifi=0;

// ---- Boot self-test: verify driver/colors/orientation at a glance -----------
static void bootSelfTest(){
  const uint16_t bars[3]={RGB565(255,0,0),RGB565(0,255,0),RGB565(0,0,255)};
  for(int i=0;i<3;i++) tft.fillRect(i*107,0,107,120,bars[i]);
  tft.fillRect(0,120,320,120,TFT_BLACK);
  tft.setTextColor(TFT_WHITE,TFT_BLACK); tft.setTextDatum(MC_DATUM); tft.setTextFont(4);
  tft.drawString("GOD'S EYE CYD",160,150);
  tft.setTextFont(2);
  tft.drawString("R  G  B correct? not inverted?",160,180);
  tft.drawString("If wrong: edit User_Setup.h",160,200);
  delay(1600);
}

// Wait until the background net task's current fetch finishes, so a UI-thread
// fetch (map tiles, CCTV frames, a one-shot flight poll) never runs a second
// TLS client at the same time. The task stops taking new work once the screen
// leaves RADAR/MENU, so this returns within a fetch or two.
static void netWaitIdle(){ uint32_t t=millis(); while(g_netBusy && millis()-t<4000) delay(10); }

// ---- Screen transitions -----------------------------------------------------
static void enterCctv(){
  g_app.screen=SCR_CCTV; g_app.cctvPlaying=false;
  netWaitIdle();
  cctvLoadList();                       // one-shot: pull the latest camera list
  setStatus("CCTV"); cctvDrawList();
}
static void enterRadar(){
  g_app.screen=SCR_RADAR;
  g_app.qLat=g_app.homeLat; g_app.qLon=g_app.homeLon;   // flights around home again
  tF=0;                                                 // re-poll home traffic soon
  setStatus("RADAR"); drawRadar();
}
static void enterMenu(){ g_app.screen=SCR_MENU; drawMenu(); }
static void enterMap(){
  g_app.screen=SCR_MAP;
  if(!g_mapInit){ g_mapLat=g_app.homeLat; g_mapLon=g_app.homeLon; g_mapZoom=11; g_mapInit=true; }
  g_app.qLat=g_mapLat; g_app.qLon=g_mapLon;             // flights around map center
  netWaitIdle(); pollFlights();                         // one-shot: task is idle off-radar
  setStatus("MAP"); drawMap();
}

// ---- Touch dispatch ---------------------------------------------------------
static void handleTouch(){
  int mx,my; if(!mapTouch(mx,my)) return;

  if(g_app.screen==SCR_RADAR){
    if(my>=240-BOT_H){                                // bottom bar
      if(mx<LEGEND_X_MAX){                            // F Q S L C -> layer, G -> GPS
        int li=(mx-2)/13;
        if(li>=L_COUNT){ gpsSetEnabled(!g_app.gpsOn); prefsSaveUI(); drawRadar(); }
        else {
          li=clampi(li,0,L_COUNT-1);
          g_app.layerOn[li]=!g_app.layerOn[li];
          g_app.selIndex=-1; g_app.selHex[0]=0; prefsSaveUI(); drawRadar();
        }
      }
      else if(mx>=170 && mx<212)  enterMap();
      else if(mx>=212 && mx<262)  enterMenu();
      else if(mx>=262)            enterCctv();
    } else if(mx>=PANEL_X && my<TOP_H+20){            // range chip
      int n=sizeof(RANGE_STEPS)/sizeof(RANGE_STEPS[0]); int cur=0;
      for(int i=0;i<n;i++) if(RANGE_STEPS[i]==g_app.rangeNm) cur=i;
      g_app.rangeNm=RANGE_STEPS[(cur+1)%n]; g_app.selIndex=-1; g_app.selHex[0]=0; prefsSaveUI(); drawRadar();
    } else if(mx<PANEL_X && my>TOP_H && my<240-BOT_H){ // scope -> select
      selectAt(mx,my); drawRadar();
    }
  }
  else if(g_app.screen==SCR_MENU){
    for(int i=0;i<L_COUNT;i++){ int ry=34+i*30;
      if(my>=ry && my<ry+26){ g_app.layerOn[i]=!g_app.layerOn[i]; prefsSaveUI(); drawMenu(); }
    }
    int ry=34+L_COUNT*30;
    if(my>=ry && my<ry+26){
      if(mx<108)      { g_app.theme=(g_app.theme+1)%THEME_COUNT; prefsSaveUI(); drawMenu(); }
      else if(mx<210) { g_app.wantPortal=true; }        // SETUP -> portal (in loop)
      else            { enterRadar(); }
    }
  }
  else if(g_app.screen==SCR_CCTV){                     // camera list
    if(my>=CCTV_FOOTER_Y){                             // footer: UP | DOWN | RADAR
      if(mx<44){ if(g_app.cctvScroll>0){ g_app.cctvScroll-=MJPG_ROWS; if(g_app.cctvScroll<0)g_app.cctvScroll=0; cctvDrawList(); } }
      else if(mx<110){ if(g_app.cctvScroll+MJPG_ROWS<camCount()){ g_app.cctvScroll+=MJPG_ROWS; cctvDrawList(); } }
      else if(mx>260){ enterRadar(); }
    } else if(my>=22){                                 // a camera row
      int idx=g_app.cctvScroll + (my-22)/MJPG_ROW_H;
      if(idx>=0 && idx<camCount()){
        g_app.cctvIdx=idx;
        int r=cctvPlay();                              // blocking; 0=radar, 1=list
        if(r==0) enterRadar(); else cctvDrawList();
      }
    }
  }
  else if(g_app.screen==SCR_MAP){
    if(my>=240-MAP_BAR_H){                             // map bar
      if(mx<32)        { if(g_mapZoom>MAP_MIN_Z){g_mapZoom--; drawMap();} }        // [-]
      else if(mx<66)   { if(g_mapZoom<MAP_MAX_Z){g_mapZoom++; drawMap();} }        // [+]
      else if(mx<104)  { if(mapGoto()) pollFlights(); drawMap(); }                 // [GO] keypad
      else if(mx<150)  {                                                           // [LOC] center on GPS
        if(g_app.gpsOn && g_gpsFix){
          g_mapLat=g_gpsLat; g_mapLon=g_gpsLon;
          g_app.qLat=g_mapLat; g_app.qLon=g_mapLon; pollFlights(); drawMap();
        } else {
          tft.setTextDatum(MC_DATUM); tft.setTextFont(2); tft.setTextColor(PAL().warn,PAL().bg);
          tft.drawString(g_app.gpsOn?"GPS: acquiring...":"GPS off - tap G on radar",160,116);
          delay(1000); drawMap();
        }
      }
      else if(mx>=236 && mx<284){ g_mapLat=g_app.homeLat; g_mapLon=g_app.homeLon;  // HOME
                                  g_app.qLat=g_mapLat; g_app.qLon=g_mapLon; pollFlights(); drawMap(); }
      else if(mx>=284) { enterRadar(); }                                           // BACK
    } else {                                           // tap map body -> recenter
      mapTapRecenter(mx,my); pollFlights(); drawMap();
    }
  }
  // debounce / wait for release
  uint32_t t0=millis();
  while(touch.touched() && millis()-t0<600) delay(10);
  delay(60);
}

// ---- Poll scheduler: at most one NETWORK fetch per loop pass -----------------
static void doNetwork(){
  uint32_t now=millis();
  if(g_app.screen==SCR_CCTV) return;   // CCTV draws its own frames (list is static)
  if(tTLE==0 || now-tTLE>=POLL_TLE_REFRESH_MS){
    setStatus("TLE sync..."); tTLE=now; refreshTLEs(); return;
  }
  if(g_app.layerOn[L_FLIGHTS] && now-tF>=POLL_FLIGHTS_MS){
    tF=now; setStatus("ADS-B..."); pollFlights(); setStatus("RADAR"); return; }
  if(g_app.layerOn[L_QUAKES] && now-tQ>=POLL_QUAKES_MS){
    tQ=now; setStatus("USGS..."); pollQuakes(); setStatus("RADAR"); return; }
  if(g_app.layerOn[L_LAUNCHES] && (tL==0 || now-tL>=POLL_LAUNCHES_MS)){
    tL=now; setStatus("LL2..."); pollLaunches(); setStatus("RADAR"); return; }
}

#if USE_NET_TASK
// Runs on core 0: polls feeds only while the radar/menu is showing (CCTV and
// MAP own the radio themselves). One fetch per pass; the UI keeps running.
static void netTask(void*){
  for(;;){
    Screen s=g_app.screen;
    if((s==SCR_RADAR || s==SCR_MENU) && !g_netPause){
      g_netBusy=true;
      if(g_app.layerOn[L_SATS] && millis()-tS>=POLL_SATS_MS){ tS=millis(); pollSats(); }
      doNetwork();
      g_netBusy=false;
    }
    vTaskDelay(pdMS_TO_TICKS(120));
  }
}
#endif

void setup(){
  Serial.begin(115200);
  pinMode(LED_R,OUTPUT); pinMode(LED_G,OUTPUT); pinMode(LED_B,OUTPUT); ledOff();
  pinMode(TFT_BL,OUTPUT); digitalWrite(TFT_BL,HIGH);

  tft.init(); tft.setRotation(1); tft.fillScreen(TFT_BLACK);
  bootSelfTest();

  touchSPI.begin(T_CLK,T_MISO,T_MOSI,T_CS);
  touch.begin(touchSPI); touch.setRotation(1);

  g_dataMux = xSemaphoreCreateMutex();   // guards the shared contact arrays

  pinMode(BOOT_BTN, INPUT_PULLUP);

  // Load saved settings (home location, theme, range, layers) from NVS. First
  // boot has none, so this leaves the config.h defaults in place.
  prefsLoad();
  if(g_app.gpsOn) gpsSetEnabled(true);   // resume GPS if it was on last time

  // Hold BOOT at power-on to force the setup portal even if a network is saved.
  bool forcePortal = (digitalRead(BOOT_BTN)==LOW);

  tft.fillScreen(PAL().bg);
  tft.setTextColor(PAL().text,PAL().bg); tft.setTextDatum(MC_DATUM); tft.setTextFont(2);
  tft.drawString(forcePortal ? "Opening setup..." : "Linking WiFi...",160,120);

  // Auto-connect to the saved network, or open the phone portal to set it up.
  provisionRun(forcePortal);
  configTime(GMT_OFFSET_SEC,DST_OFFSET_SEC,NTP_SERVER);

  enterRadar();

#if USE_NET_TASK
  // Start the background poller on core 0 (16 KB stack: TLS needs the room).
  xTaskCreatePinnedToCore(netTask,"net",16384,nullptr,1,nullptr,0);
#endif
}

void loop(){
  // keep WiFi up
  if(WiFi.status()!=WL_CONNECTED && millis()-tWifi>10000){ tWifi=millis(); wifiEnsure(8000); }
  ledLink(g_app.wifiUp && millis()-g_app.lastRx<20000);

  if(g_app.gpsOn) gpsUpdate();       // drain the GPS UART + refresh position

  handleTouch();

  // SETUP button in the menu asked to re-open the phone portal.
  if(g_app.wantPortal){
    g_app.wantPortal=false;
    g_netPause=true; netWaitIdle();   // stop the poller while the AP portal is up
    provisionRun(true);
    configTime(GMT_OFFSET_SEC,DST_OFFSET_SEC,NTP_SERVER);
    tF=tQ=tL=0;                       // re-poll immediately around the new HOME
    g_netPause=false;
    enterRadar();
  }

#if !USE_NET_TASK
  // Fallback inline polling (when the network task is disabled).
  if(g_app.layerOn[L_SATS] && millis()-tS>=POLL_SATS_MS){ tS=millis(); pollSats(); }
  doNetwork();
#endif

  // render
  if(g_app.screen==SCR_RADAR && millis()-tDraw>=220){ tDraw=millis(); drawRadar(); }
}
