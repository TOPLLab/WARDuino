#include "util_arduino.h"

#ifdef ARDUINO
#include "Arduino.h"
void reset_wdt() { yield(); }

#else

void reset_wdt() {}

#endif