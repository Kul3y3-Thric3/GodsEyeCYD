// =============================================================================
//  God's Eye CYD  -  TFT_eSPI User_Setup for ESP32-2432S028 (Cheap Yellow Display)
//
//  >>> THIS FILE MUST REPLACE  Arduino/libraries/TFT_eSPI/User_Setup.h  <<<
//  (TFT_eSPI is configured at COMPILE time; it cannot be set from the sketch.)
//
//  ---------------------------------------------------------------------------
//  DISPLAY DRIVER  -  the one thing to get right on your board.
//  Your 2-USB (micro + USB-C) CYD almost certainly uses ST7789 (default below).
//  The 1-USB "classic" uses ILI9341. If the screen is blank OR colors look
//  wrong/inverted, flip between the two blocks and toggle inversion.
//  ---------------------------------------------------------------------------

// ==== OPTION A: ST7789  (2-USB CYD - DEFAULT) ================================
#define ST7789_2_DRIVER
#define TFT_RGB_ORDER TFT_BGR      // if reds/blues are swapped, try TFT_RGB
// #define TFT_INVERSION_ON        // uncomment if the image is a photo-negative
#define TFT_INVERSION_OFF

// ==== OPTION B: ILI9341  (1-USB classic CYD) ================================
//  To use instead: comment out ALL of Option A above, then uncomment these:
// #define ILI9341_2_DRIVER
// #define TFT_INVERSION_OFF
//  (some ILI9341 units need: #define TFT_INVERSION_ON  and/or  TFT_RGB_ORDER TFT_RGB)

// ---- Panel geometry (same for both) -----------------------------------------
#define TFT_WIDTH   240
#define TFT_HEIGHT  320

// ---- CYD display pins (VSPI) -------------------------------------------------
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1                 // tied to EN on the CYD
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// NOTE: the XPT2046 touch controller is on a SEPARATE SPI bus (pins 25/32/39/33)
// and is driven by the XPT2046_Touchscreen library in the sketch, NOT by
// TFT_eSPI. So do NOT define TOUCH_CS here.

// ---- Fonts ------------------------------------------------------------------
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_GFXFF
#define SMOOTH_FONT

// ---- SPI ---------------------------------------------------------------------
#define SPI_FREQUENCY        40000000   // 40MHz: stable on CYD (55MHz can glitch)
#define SPI_READ_FREQUENCY   16000000
