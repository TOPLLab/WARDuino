# Arduino Platform

PORT = /dev/ttyUSB0
FQBN = esp32:esp32:esp32wrover

flash: 
	arduino-cli upload -p $(PORT)  --fqbn $(FQBN) Arduino-socket.ino

compile: 
	arduino-cli -v compile --fqbn $(FQBN) Arduino-socket.ino

monitor: 
	arduino-cli monitor -p $(PORT)  -c baudrate=115200

