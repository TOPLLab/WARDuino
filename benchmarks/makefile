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
CFLAGS    = -Wall -c -O3
CXXFLAGS  = -v -O3 -std=c++11 -I ../lib/json/single_include -Wall



OUTPUTDIR = ./bin/
MKDIR     = mkdir -p $(OUTPUTDIR)
TARGET    = warduino_benchmark

TASKS = $(notdir $(patsubst %/.,%,$(wildcard tasks/*/.)))

CXXSOURCES = \
	   ../src/Memory/mem.cpp \
           ../src/Utils/util.cpp \
           ../src/Utils/util_arduino.cpp \
           ../src/Debug/debugger.cpp \
           ../src/Utils/macros.cpp \
           ../src/WARDuino/WARDuino.cpp \
           ../src/WARDuino/CallbackHandler.cpp \
           ../src/Primitives/emulated.cpp \
           ../src/Interpreter/instructions.cpp \
	   ../src/Interpreter/interpreter.cpp \
           ../src/Edward/RFC.cpp \
           ../src/Edward/proxy.cpp \
           ../src/Edward/proxy_supervisor.cpp \
           ../src/Utils/sockets.cpp \
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
	-$(MAKE) -C tasks clean

mostlyclean:
	-$(RM) -rf $(OUTPUTDIR)
	-$(RM) $(COBJECTS)
