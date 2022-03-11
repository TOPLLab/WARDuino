#include "util_arduino.h"

#ifdef ARDUINO
#include "Arduino.h"
void reset_wdt() { yield(); }

#else

#ifdef ESP
#include "esp_task_wdt.h"
#include "freertos/task.h"
void reset_wdt() {
  //taskYIELD();
		//esp_task_wdt_reset();
	 esp_task_wdt_reset();
		//printf("reset wdt  \n");
}	
#else

void reset_wdt() {}
#endif

#endif
