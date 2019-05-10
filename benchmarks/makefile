# __      __                  .___    .__               
#/  \    /  \_____ _______  __| _/_ __|__| ____   ____  
#\   \/\/   /\__  \\_  __ \/ __ |  |  \  |/    \ /  _ \ 
# \        /  / __ \|  | \/ /_/ |  |  /  |   |  (  <_> )
#  \__/\  /  (____  /__|  \____ |____/|__|___|  /\____/ 
#       \/        \/           \/             \/        
DEBUG=0
TRACE=0
INFO=0
WARN=0

DEBUGFLAGS = -DDEBUG=$(DEBUG) -DTRACE=$(TRACE) -DINFO=$(INFO) -DWARN=$(WARN)


CXX       = g++
CC        = gcc
CFLAGS    = -g -Wall -c
CXXFLAGS  = -g -v -std=c++11  -Wall



OUTPUTDIR = ./bin/
MKDIR     = mkdir -p $(OUTPUTDIR)
TARGET    = warduino_benchmark

CXXSOURCES = \
	../mem.cpp \
	../util.cpp \
	../debug.cpp \
	../WARDuino.cpp \
	../primitives.cpp \
	../instructions.cpp \
	benchmarks.cpp

all: warduino

../%.o : ../%.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c $<   -o $@

../wa_sources/%.c: ../wa_sources/%.wast
	$(MAKE) -C ../wa_sources

warduino: $(COBJECTS)  $(CXXSOURCES) ../wa_sources/*.c
	$(MKDIR)

	$(CXX) $(CXXFLAGS) $(DEBUGFLAGS) $(COBJECTS) $(CXXSOURCES) -o $(OUTPUTDIR)$(TARGET)


run: 
	$(OUTPUTDIR)$(TARGET)

.PHONY: clean
clean:
	$(MAKE) -C ../wa_sources clean
	$(RM) -rf $(OUTPUTDIR)
	$(RM) $(COBJECTS)
