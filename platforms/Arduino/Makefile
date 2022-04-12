flash: 
	arduino-cli upload -p /dev/ttyUSB0  --fqbn esp32:esp32:esp32wrover Arduino.ino

compile: 
	arduino-cli -v compile --fqbn esp32:esp32:esp32wrover Arduino.ino

monitor: 
	arduino-cli monitor -p /dev/ttyUSB0  -c baudrate=115200
