# Arduino Platform

# default values
PORT = /dev/ttyUSB0
FQBN = esp32:esp32:esp32wrover
RADIO = MOCK

# possible config file
CONFIG = .config
-include ${CONFIG}

ifdef PAUSED
	COMMAND := uint8_t command[] = {'0', '3', '\\\n'}; wac->handleInterrupt(3, command);
else
	COMMAND :=
endif

clean:
	rm -f Arduino.ino 
	rm -rf bin 

bin:
	mkdir bin

bin/upload.h: bin
ifndef BINARY
	$(error BINARY is not set. Use a .config file)
endif
	cp $(BINARY) bin/upload.wasm
	cd bin; xxd -i upload.wasm > upload.h

Arduino.ino: bin/upload.h Arduino.ino.template
	cat Arduino.ino.template > $@
	sed  "s/{{PAUSED}}/$(COMMAND)/" $@ > Arduino.tmp
	mv Arduino.tmp $@


flash: Arduino.ino
ifndef PORT
	$(error PORT is not set. Use a .config file)
endif
ifndef FQBN
	$(error FQBN is not set. Use a .config file)
endif
	arduino-cli upload -p $(PORT) --fqbn $(FQBN) Arduino.ino

recompile: Arduino.ino
ifndef FQBN
	$(error FQBN is not set. Use a .config file)
endif
	arduino-cli compile --fqbn $(FQBN) --build-property "compiler.cpp.extra_flags=-DRADIO_$(RADIO)" Arduino.ino

compile: clean recompile

monitor: 
ifndef PORT
	$(error PORT is not set. Use a .config file)
endif
	arduino-cli monitor -p $(PORT) -c baudrate=115200

