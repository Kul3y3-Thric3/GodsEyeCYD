// =============================================================================
//  God's Eye CYD  -  <SD.h> compatibility shim  (NOT the project's SD code)
//
//  The real microSD / WiGLE code lives in sdcard.h. This file exists only to
//  survive case-INsensitive filesystems (macOS, Dropbox, Windows): there,
//  TJpg_Decoder's  #include <SD.h>  can resolve to a sketch file named "sd.h"
//  instead of the Arduino SD library, which used to drag the whole project in
//  twice and break the build.
//
//  By forwarding to the next <SD.h> on the include path, TJpg_Decoder always
//  gets the genuine Arduino SD library and its File type. On case-SENSITIVE
//  systems (Linux) <SD.h> never matches this file, so it's simply unused.
//
//  Do not put project code here, and do not #include "sd.h" from the project —
//  include "sdcard.h" instead.
// =============================================================================
#include_next <SD.h>
