#include "util_arduino.h"

#ifdef ARDUINO
#include "Arduino.h"
void wdt_reset() { wdt_reset(); }

#else

#define wdt_reset(...) ;

#endif