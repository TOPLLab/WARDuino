#include "util_arduino.h"

#ifdef ARDUINO
#include "Arduino.h"
void reset_wdt() { wdt_reset(); }

#else

void reset_wdt() { }

#endif