// =============================================================================
//  God's Eye CYD  -  sensor palettes (Normal / NVG / FLIR / Amber tactical)
// =============================================================================
#pragma once
#include "app_state.h"

#define RGB565(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))

struct Palette {
  const char* name;
  uint16_t bg, grid, ring, text, dim, accent, warn, mil, sat, quake, launch, center;
};

// Index order matches DEFAULT_THEME in config.h
static const Palette PALETTES[] = {
  // NORMAL
  { "NORMAL",
    RGB565(6,10,16),  RGB565(24,34,48),  RGB565(40,90,120),
    RGB565(220,230,240), RGB565(110,130,150), RGB565(0,200,255),
    RGB565(255,90,60), RGB565(255,190,40), RGB565(120,220,255),
    RGB565(255,120,40), RGB565(200,120,255), RGB565(0,255,180) },
  // NVG (night vision, green mono)
  { "NVG",
    RGB565(2,10,4),   RGB565(10,40,16),  RGB565(20,90,30),
    RGB565(140,255,150), RGB565(40,120,60), RGB565(180,255,120),
    RGB565(255,255,140), RGB565(200,255,120), RGB565(120,255,140),
    RGB565(220,255,120), RGB565(160,255,120), RGB565(180,255,160) },
  // FLIR / IRONBOW
  { "FLIR",
    RGB565(8,4,14),   RGB565(40,24,20),  RGB565(90,50,30),
    RGB565(255,235,200), RGB565(150,90,70), RGB565(255,180,40),
    RGB565(255,80,30), RGB565(255,210,60), RGB565(255,150,80),
    RGB565(255,110,30), RGB565(255,200,120), RGB565(255,230,150) },
  // AMBER TACTICAL
  { "AMBER",
    RGB565(8,6,2),    RGB565(48,32,6),   RGB565(120,80,10),
    RGB565(255,190,60), RGB565(130,90,20), RGB565(255,150,20),
    RGB565(255,70,40), RGB565(255,220,80), RGB565(255,200,90),
    RGB565(255,120,40), RGB565(255,230,120), RGB565(255,220,120) },
};
#define THEME_COUNT (sizeof(PALETTES)/sizeof(PALETTES[0]))

inline const Palette& PAL(){ return PALETTES[clampi(g_app.theme,0,THEME_COUNT-1)]; }
