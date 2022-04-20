flash: 
	arduino-cli upload -p /dev/ttyUSB0  --fqbn esp32:esp32:esp32wrover Arduino.ino

compile: 
	arduino-cli -v compile --fqbn esp32:esp32:esp32wrover Arduino.ino

monitor: 
	arduino-cli monitor -p /dev/ttyUSB0  -c baudrate=115200

flash-dev: 
	arduino-cli upload -p /dev/cu.usbserial-2952919DA8  --fqbn esp32:esp32:esp32doit-devkit-v1:UploadSpeed=115200 Arduino.ino

compile-dev: 
	arduino-cli -v compile --fqbn esp32:esp32:esp32doit-devkit-v1 Arduino.ino

monitor: 
	arduino-cli monitor -p /dev/cu.usbserial-2952919DA8  -c baudrate=115200
