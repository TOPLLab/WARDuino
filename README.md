# WARDuino

Warduino is a port of the Web assembly virtual machine for the ESP32/ESP8266 under the arduino toolchain. 

# Overview of the project 

```
├── README.md
├── WARDuino.cpp
├── WARDuino.h
├── library.properties
├── local
│   ├── main.cpp
│   ├── makefile
│   └── warduino
├── mem.c
├── mem.h
├── util.c
└── util.h
```


# ToolChain Installation and Hello World

- Download [Arduino](https://www.arduino.cc/)
- Clone the repository under `$(HOME)/Arduino/libraries`
- Go to Tools->Boards->Boards Manager
  - Search for esp8266 and install it (version 2.5.0) 
  - Restart Arduino 
  
- Plug in the board and configure the board settings in the boards menu as follows 

![Board configuration options](images/board_config.png)  

- Make a new sketch with the following code:

```C
#include <WARDuino.h>


WARDuino wac;

void setup() {
  Serial.begin(115200);

}

void loop() {
  Serial.println(ESP.getFreeHeap());
  int i = wac.run_module();
  Serial.println(i,HEX);
  Serial.println("DONE");

  while(true) {
    delay(5000);
  }
}
```

- Compile and upload the sketch
