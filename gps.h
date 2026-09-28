// =============================================================================
//  God's Eye CYD  -  optional GPS (ATGM336H etc.) on the 4-pin JST connector
//  Wardriving-standard CYD wiring (ESP32 Marauder): GPS TX -> GPIO22 (ESP RX),
//  GPS RX -> GPIO27 (ESP TX), 3.3V, GND. NMEA parsed with TinyGPSPlus.
//
//  GPS is a separate toggle (the "G" on the radar legend), NOT a radar layer,
//  so it doesn't disturb the F Q S L C system. Include BEFORE ui.h / mapview.h.
// =============================================================================
#pragma once
#include <TinyGPS++.h>
#include "app_state.h"

static HardwareSerial GPSserial(1);      // UART1, pins remapped in gpsSetEnabled()
static TinyGPSPlus    gpsParser;

// live GPS state (written on core 1 in loop(), read by the UI on core 1)
static double   g_gpsLat=0, g_gpsLon=0;
static int      g_gpsSats=0;
static bool     g_gpsPresent=false;      // NMEA bytes seen recently (module wired up)
static bool     g_gpsFix=false;          // a valid position fix
static uint32_t g_gpsLastData=0, g_gpsLastFix=0;

inline void gpsSetEnabled(bool on){
  g_app.gpsOn=on;
  if(on) GPSserial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  else  { GPSserial.end(); g_gpsPresent=false; g_gpsFix=false; }
}

// Drain the UART and update state. Call often from loop() while GPS is on.
inline void gpsUpdate(){
  while(GPSserial.available()){
    if(gpsParser.encode((char)GPSserial.read())) { /* full sentence */ }
    g_gpsLastData=millis();
  }
  g_gpsPresent = (g_gpsLastData!=0 && millis()-g_gpsLastData < 3000);
  if(gpsParser.location.isValid() && gpsParser.location.age()<5000){
    g_gpsLat=gpsParser.location.lat();
    g_gpsLon=gpsParser.location.lng();
    g_gpsFix=true; g_gpsLastFix=millis();
  } else if(millis()-g_gpsLastFix>5000){
    g_gpsFix=false;
  }
  if(gpsParser.satellites.isValid()) g_gpsSats=gpsParser.satellites.value();
}
