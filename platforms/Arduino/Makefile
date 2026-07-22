# Arduino Platform

CONFIG = .config
-include ${CONFIG}

ifdef PAUSED
	COMMAND := uint8_t command[] = {'0', '3', '0', '1', '\\\n'}; wac->handleInterrupt(5, command);
else
	COMMAND :=
endif

ifeq (${DISABLESTRICTMODULELOAD}, true)
	DIS_STRICT_LOAD := true
else
	DIS_STRICT_LOAD := false
endif

CONFIG_FILE := $(if $(ARDUINO_CONFIG),--config-file $(ARDUINO_CONFIG),)

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
	sed  "s/{{PAUSED}}/$(COMMAND)/; s/{{STRICTLOAD}}/$(DIS_STRICT_LOAD)/" $@ > Arduino.tmp
	mv Arduino.tmp $@


flash: Arduino.ino
ifeq ($(ARDUINO_CLI),)
	$(error ARDUINO_CLI is not set)
endif
ifndef PORT
	$(error PORT is not set. Use a .config file)
endif
ifndef FQBN
	$(error FQBN is not set. Use a .config file)
endif
	$(ARDUINO_CLI) upload -p $(PORT) --fqbn $(FQBN) $(CONFIG_FILE) Arduino.ino

compile: Arduino.ino
ifeq ($(ARDUINO_CLI),)
	$(error ARDUINO_CLI is not set)
endif
ifndef FQBN
	$(error FQBN is not set. Use a .config file)
endif
	$(ARDUINO_CLI) compile --fqbn $(FQBN) $(CONFIG_FILE) Arduino.ino -e

recompile: clean compile

monitor: 
ifeq ($(ARDUINO_CLI),)
	$(error ARDUINO_CLI is not set)
endif
ifndef PORT
	$(error PORT is not set. Use a .config file)
endif
	$(ARDUINO_CLI) monitor -p $(PORT) -c baudrate=115200

