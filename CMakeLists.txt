cmake_minimum_required(VERSION 3.14)
project (WARDuino)
add_compile_options(-g -v -std=c++11  -Wall -Wextra -Wunused)
set(SOURCE_FILES mem.cpp util.cpp util_arduino.cpp interrupt_operations.cpp debug.cpp WARDuino.cpp primitives.cpp instructions.cpp)
add_executable(WARDuino demo/run_wa/main.cpp ${SOURCE_FILES})
add_executable(TestWARDuino tests/main.cpp ${SOURCE_FILES})


#add_definitions(-DARDUINO)
add_definitions(-DDEBUG=1)
add_definitions(-DTRACE=1)
add_definitions(-DWARN=1)

add_executable(WARDuinoBench benchmarks/benchmarks.cpp ${SOURCE_FILES})
install (TARGETS WARDuino DESTINATION demo/run_wa/bin)

add_custom_command (
        TARGET WARDuinoBench
        POST_BUILD
        COMMAND make -C  ../benchmarks/tasks all
)

