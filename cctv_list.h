// =============================================================================
//  God's Eye CYD  -  bundled public webcam list (MJPEG over HTTP)
//  Adapted from 7h30th3r0n3's RaspyJack CCTV list (public leisure / tourism /
//  city / transport webcams). These are MJPEG streams (Axis-style
//  /mjpg/video.mjpg or /axis-cgi/mjpg/video.cgi).
//
//  Public cams go up and down constantly — expect some "camera offline", just
//  press NEXT. Entries are ordered sturdiest-first: DNS-named institutional /
//  town cams (which persist for years) are up top; bare-IP cams (which die when
//  a home IP changes) are lower down. Prune dead ones and add your own freely.
//
//  Format: { "Category  Name", "http://host:port/path" } — http:// MJPEG only.
// =============================================================================
#pragma once

struct CctvCam { const char* label; const char* url; };

static const CctvCam CCTV_CAMS[] = {
  // === Named-host cams (best longevity: universities, towns, clubs) =========
  { "UNI  UMD McKeldin US","http://cam-mckeldin-eastview.umd.edu/axis-cgi/mjpg/video.cgi" },
  { "UNI  Dikemes GR",     "http://view.dikemes.edu.gr/mjpg/video.mjpg" },
  { "UNI  WSEiP PL",       "http://kamera.wseip.edu.pl/mjpg/video.mjpg" },
  { "CITY Anklam DE",      "http://webcam.anklam.de/axis-cgi/mjpg/video.cgi" },
  { "MISC Sebewaing US",   "http://skycam.sebewainggigvillage.com/mjpg/video.mjpg" },
  { "MISC Sissiboo CA",    "http://camera.sissiboo.com:86/mjpg/video.mjpg" },
  { "GOLF Atlantide FR",   "http://camera.golfatlantide.com:8080/mjpg/video.mjpg" },
  { "SKI  Tusten NO",      "http://live1.tusten.no:8080/axis-cgi/mjpg/video.cgi" },
  { "AIR  Alatsa Aero GR", "http://alatsaeroclub.ddns.net:85/mjpg/video.mjpg" },

  // === Ski / Mountain =======================================================
  { "SKI  Andalsnes NO",   "http://78.31.82.246/mjpg/video.mjpg" },
  { "SKI  Gausta NO",      "http://109.109.87.147/mjpg/video.mjpg" },
  { "SKI  Lygna NO",       "http://193.214.75.118/mjpg/video.mjpg" },
  { "SKI  Oslo NO",        "http://193.90.139.222:33445/mjpg/video.mjpg" },
  { "SKI  hochkogl AT",    "http://212.67.236.61/mjpg/video.mjpg" },
  { "SKI  Switzerland",    "http://213.3.30.80:6001/axis-cgi/mjpg/video.cgi" },

  // === Ports / Sea / Beaches ================================================
  { "SEA  Esbjerg DK",     "http://37.128.212.84/mjpg/video.mjpg" },
  { "SEA  Flensburg DE",   "http://62.214.4.38/mjpg/video.mjpg" },
  { "SEA  Fredrikstad NO", "http://109.247.15.178:6001/mjpg/video.mjpg" },
  { "SEA  Norway port",    "http://213.236.250.78/axis-cgi/mjpg/video.cgi" },
  { "SEA  Playa Levante ES","http://212.170.100.189/mjpg/video.mjpg" },
  { "SEA  Bodo lake NO",   "http://77.110.245.165/axis-cgi/mjpg/video.cgi" },

  // === Airports / Aviation ==================================================
  { "AIR  Prescott US",    "http://199.104.253.4/mjpg/video.mjpg" },
  { "AIR  Carlyle US",     "http://74.113.182.246:9600/axis-cgi/mjpg/video.cgi" },

  // === Cities / Public Areas ================================================
  { "CITY Minneapolis US", "http://63.142.190.238:6120/mjpg/video.mjpg" },
  { "CITY Cle Elum US",    "http://204.106.237.68:88/mjpg/1/video.mjpg" },
  { "CITY Morehead NC US", "http://70.63.123.20/mjpg/1/video.mjpg" },
  { "CITY Greenwood US",   "http://71.43.10.26:9080/axis-cgi/mjpg/video.cgi" },
  { "CITY Vancouver CA",   "http://207.194.15.97/mjpg/video.mjpg" },
  { "CITY Madrid ES",      "http://83.48.75.113:8320/axis-cgi/mjpg/video.cgi" },
  { "CITY Campobasso IT",  "http://195.32.24.180:1024/mjpg/video.mjpg" },
  { "CITY Tamworth UK",    "http://213.123.122.163:1087/axis-cgi/mjpg/video.cgi" },
  { "CITY Skien NO",       "http://159.130.70.206/mjpg/video.mjpg" },
  { "CITY Gabrovo BG",     "http://89.106.109.144:12060/mjpg/video.mjpg" },
  { "CITY Villefranche FR","http://82.127.206.236/axis-cgi/mjpg/video.cgi" },

  // === Golf / Leisure =======================================================
  { "GOLF Castello ES",    "http://185.74.192.88:85/axis-cgi/mjpg/video.cgi" },
  { "GOLF Avalon US",      "http://74.95.172.65:8100/axis-cgi/mjpg/video.cgi" },
  { "MISC Weinbauverein DE","http://24.134.3.9/axis-cgi/mjpg/video.cgi" },
};
#define CCTV_COUNT ((int)(sizeof(CCTV_CAMS)/sizeof(CCTV_CAMS[0])))
