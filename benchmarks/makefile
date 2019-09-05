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

TASKS = $(notdir $(patsubst %/.,%,$(wildcard tasks/*/.)))

CXXSOURCES = \
	../mem.cpp \
	../util.cpp \
	../util_arduino.cpp \
	../interrupt_operations.cpp \
	../debug.cpp \
	../WARDuino.cpp \
	../primitives.cpp \
	../instructions.cpp \
	benchmarks.cpp

all: $(OUTPUTDIR)$(TARGET) $(addprefix tasks/,$(addsuffix /wast/impl.wasm, $(TASKS)))
	echo "made tasks: $(TASKS)"

tasks/%/wast/impl.wasm: tasks/%/wast/impl.c tasks/makefile
	make -C tasks $(@:tasks/%=%)

$(OUTPUTDIR)$(TARGET): $(CXXSOURCES)
	$(MKDIR)
	$(CXX) $(CXXFLAGS) $(DEBUGFLAGS) $(COBJECTS) $(CXXSOURCES) -o $(OUTPUTDIR)$(TARGET)

run: all
	$(OUTPUTDIR)$(TARGET)

clean: mostlyclean
	$(MAKE) -C tasks clean

mostlyclean:
	$(RM) -rf $(OUTPUTDIR)
	$(RM) $(COBJECTS)
