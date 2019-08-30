# __      __                  .___    .__               
#/  \    /  \_____ _______  __| _/_ __|__| ____   ____  
#\   \/\/   /\__  \\_  __ \/ __ |  |  \  |/    \ /  _ \ 
# \        /  / __ \|  | \/ /_/ |  |  /  |   |  (  <_> )
#  \__/\  /  (____  /__|  \____ |____/|__|___|  /\____/ 
#       \/        \/           \/             \/        
DEBUGFLAGS = -DDEBUG=1 -DTRACE=1 -DINFO=1 -DWARN=1
CXX       = g++
CC        = gcc
CFLAGS    = -g -Wall -c
CXXFLAGS  = -g -v -std=c++11  -Wall

OUTPUTDIR = ./bin/
MKDIR     = mkdir -p $(OUTPUTDIR)
TARGET    = warduino

CXXSOURCES = \
	../../mem.cpp \
	../../util.cpp \
	../../util_arduino.cpp \
	../../interrupt_operations.cpp \
	../../debug.cpp \
	../../WARDuino.cpp \
	../../primitives.cpp \
	../../instructions.cpp \
	main.cpp

all: warduino

run: all
	./$(OUTPUTDIR)warduino


wa_sources/%.c: wa_sources/%.wast
	$(MAKE) -C wa_sources

warduino: $(COBJECTS)  $(CXXSOURCES) wa_sources/*.c
	$(MKDIR)
	$(CXX) $(CXXFLAGS) $(DEBUGFLAGS) $(COBJECTS) $(CXXSOURCES) -o $(OUTPUTDIR)$(TARGET)

.PHONY: clean
clean:
	$(MAKE) -C wa_sources clean
	$(RM) -rf $(OUTPUTDIR)
	$(RM) $(COBJECTS)
