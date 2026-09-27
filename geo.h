// =============================================================================
//  God's Eye CYD  -  geo math + north-up PPI projection
// =============================================================================
#pragma once
#include <math.h>
#include "app_state.h"

// ---- Scope geometry (landscape 320x240, rotation 1) --------------------------
#define TOP_H       15
#define BOT_H       15
#define SCOPE_CX    120
#define SCOPE_CY    120     // centered in the 15..225 band (top/bottom bars 15px)
#define SCOPE_R     100
#define PANEL_X     230     // right-hand side panel starts here
#define PANEL_W     (320 - PANEL_X)

static const double R_EARTH_NM = 3440.065;
#define D2R 0.017453292519943295
#define R2D 57.29577951308232

// Great-circle distance in nautical miles.
inline float haversineNm(double lat1,double lon1,double lat2,double lon2){
  double dlat=(lat2-lat1)*D2R, dlon=(lon2-lon1)*D2R;
  double a=sin(dlat/2)*sin(dlat/2)+cos(lat1*D2R)*cos(lat2*D2R)*sin(dlon/2)*sin(dlon/2);
  return (float)(2*R_EARTH_NM*atan2(sqrt(a),sqrt(1-a)));
}

// Initial bearing (deg true) from point 1 to point 2.
inline float bearingDeg(double lat1,double lon1,double lat2,double lon2){
  double y=sin((lon2-lon1)*D2R)*cos(lat2*D2R);
  double x=cos(lat1*D2R)*sin(lat2*D2R)-sin(lat1*D2R)*cos(lat2*D2R)*cos((lon2-lon1)*D2R);
  double b=atan2(y,x)*R2D; if(b<0)b+=360; return (float)b;
}

// Project a lat/lon onto the north-up PPI. Returns false if outside the ring.
// Points are placed by (range, bearing) from HOME so the geometry stays honest
// at any range setting.
inline bool projectPPI(double lat,double lon,int& sx,int& sy){
  float rng = haversineNm(g_app.homeLat,g_app.homeLon,lat,lon);
  if(rng > g_app.rangeNm) return false;
  float brg = bearingDeg(g_app.homeLat,g_app.homeLon,lat,lon)*D2R;
  float rpx = (rng/g_app.rangeNm)*SCOPE_R;
  sx = (int)lround(SCOPE_CX + rpx*sin(brg));
  sy = (int)lround(SCOPE_CY - rpx*cos(brg));
  return true;
}
