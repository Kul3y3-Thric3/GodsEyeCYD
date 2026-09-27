// =============================================================================
//  God's Eye CYD  -  WiFi + TLS fetch helpers
//  ONE request in flight at a time (the poll scheduler guarantees this), so a
//  single shared secure client keeps peak heap low on the no-PSRAM WROOM.
// =============================================================================
#pragma once
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "app_state.h"

// Fetch `url` and parse JSON into `doc`, applying `filter` (ArduinoJson filter
// document) so only the fields we care about are allocated. Returns true on a
// clean 200 + parse. Streams the body; never buffers the whole response.
inline bool fetchJson(const char* url, JsonDocument& doc,
                      const JsonDocument* filter = nullptr,
                      uint16_t timeoutMs = 9000, int* httpCode = nullptr){
  if(httpCode) *httpCode = 0;
  if(WiFi.status()!=WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();                 // public read-only feeds; no cert pinning
  HTTPClient http;
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  http.setReuse(false);
  if(!http.begin(client, url)) return false;
  http.addHeader("User-Agent","Mozilla/5.0 (GodsEyeCYD ESP32)");
  http.addHeader("Accept","application/json");
  int code = http.GET();
  if(httpCode) *httpCode = code;
  if(code != HTTP_CODE_OK){ http.end(); return false; }
  // Read the full body (HTTPClient de-chunks it correctly), then parse. This is
  // the same path the TLE fetch uses and it's robust against CDN chunking; the
  // filter keeps the parsed doc small even though the raw text is buffered.
  String body = http.getString();
  http.end();
  if(body.length()==0) return false;
  DeserializationError err = filter
      ? deserializeJson(doc, body, DeserializationOption::Filter(*filter))
      : deserializeJson(doc, body);
  return !err;
}

// Fetch a small text/plain body (e.g. a TLE set) into `out`. Returns true on 200.
inline bool fetchText(const char* url, String& out, uint16_t timeoutMs = 8000){
  if(WiFi.status()!=WL_CONNECTED) return false;
  WiFiClientSecure client; client.setInsecure(); client.setTimeout(timeoutMs/1000);
  HTTPClient http; http.setConnectTimeout(timeoutMs); http.setTimeout(timeoutMs);
  http.setReuse(false); http.useHTTP10(true);
  if(!http.begin(client,url)) return false;
  http.addHeader("User-Agent","GodsEyeCYD/1.0 (ESP32)");
  int code=http.GET();
  if(code!=HTTP_CODE_OK){ http.end(); return false; }
  out = http.getString();
  http.end();
  return true;
}

// Download binary (a JPEG snapshot) into a caller buffer, up to `cap` bytes.
// Returns bytes read, or 0 on failure / oversize. Caller owns `buf`.
inline size_t fetchBinary(const char* url, uint8_t* buf, size_t cap,
                          uint16_t timeoutMs = 9000){
  if(WiFi.status()!=WL_CONNECTED) return 0;
  WiFiClientSecure client; client.setInsecure(); client.setTimeout(timeoutMs/1000);
  HTTPClient http; http.setConnectTimeout(timeoutMs); http.setTimeout(timeoutMs);
  http.setReuse(false);
  if(!http.begin(client,url)){ return 0; }
  http.addHeader("User-Agent","GodsEyeCYD/1.0 (ESP32)");
  int code=http.GET();
  if(code!=HTTP_CODE_OK){ http.end(); return 0; }
  int len = http.getSize();                 // -1 if chunked
  if(len>0 && (size_t)len>cap){ http.end(); return 0; }
  WiFiClient* s = http.getStreamPtr();
  size_t got=0; uint32_t t0=millis();
  while(http.connected() && (len<0 || got<(size_t)len) && got<cap){
    size_t avail = s->available();
    if(avail){ int r=s->readBytes(buf+got, min(avail, cap-got)); got+=r; t0=millis(); }
    else { if(millis()-t0>timeoutMs) break; delay(2); }
  }
  http.end();
  return got;
}

// Non-blocking-ish reconnect that reuses the credentials WiFiManager saved in
// NVS (WiFi.begin() with no args reconnects to the last network). The portal in
// provision.h owns first-time setup; this only nudges a dropped link back up.
inline bool wifiEnsure(uint32_t budgetMs = 8000){
  if(WiFi.status()==WL_CONNECTED){ g_app.wifiUp=true; return true; }
  g_app.wifiUp=false;
  WiFi.mode(WIFI_STA);
  WiFi.begin();                          // reuse stored SSID/pass
  uint32_t t0=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-t0<budgetMs){ delay(120); }
  g_app.wifiUp = (WiFi.status()==WL_CONNECTED);
  return g_app.wifiUp;
}
