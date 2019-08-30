#include "Arduino.h"
#include "WARDuino.h"
WARDuino wac;

void setup() {
    Serial.begin(115200);
}

void loop() {
    Module* m = wac.load_module({{src}}, {{src}}_len, {});
    wac.run_module(m);
}