flash: 
		arduino-cli upload -p /dev/cu.usbserial-1410  --fqbn esp32:esp32:esp32wrover Arduino.ino

compile: 
	arduino-cli -v compile --fqbn esp32:esp32:esp32wrover Arduino.ino

monitor: 
		arduino-cli monitor -p /dev/cu.usbserial-1410  -c baudrate=115200
