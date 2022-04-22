# Arduino Socket Platform

PORT = /dev/ttyUSB0
FQBN = esp32:esp32:esp32wrover

CONFIG = .config
include ${CONFIG}

ifndef SSID 
$(error SSID is not set. Use a .config file.)
endif

ifndef PASSWORD
$(error PASSWORD is not set. Use a .config file.)
endif

Arduino-socket.ino: .config Arduino-socket.ino.template
	sed 's/{{SSID}}/$(SSID)/g; s/{{Password}}/$(PASSWORD)/g' Arduino-socket.ino.template >> $@

compile: Arduino-socket.ino
	arduino-cli -v compile --fqbn $(FQBN) Arduino-socket.ino

flash: 
	arduino-cli upload -p $(PORT)  --fqbn $(FQBN) Arduino-socket.ino

monitor: 
	arduino-cli monitor -p $(PORT)  -c baudrate=115200

