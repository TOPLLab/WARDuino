cmake_minimum_required(VERSION 3.14)
project(WARDuino)
add_compile_options(-g -v -std=c++11 -Wall -Wextra -Wunused)
set(SOURCE_FILES mem.cpp util.cpp util_arduino.cpp interrupt_operations.cpp debug.cpp WARDuino.cpp primitives.cpp instructions.cpp glue.cpp glue.h)
add_executable(WARDuino demo/run_wa/main.cpp ${SOURCE_FILES})
add_executable(warduino cli/main.cpp ${SOURCE_FILES})

set(TEST_SOURCE_FILES tests/integration/assertion.cpp tests/integration/sexpr-parser/src/sexpr.c tests/integration/wasm_tests.cpp tests/integration/wasm_tests.h)
add_executable(TestWARDuino tests/integration/main.cpp ${SOURCE_FILES} ${TEST_SOURCE_FILES})

#add_definitions(-DARDUINO)
add_definitions(-DINFO=0)
add_definitions(-DDEBUG=0)
add_definitions(-DTRACE=0)
add_definitions(-DWARN=0)

add_executable(WARDuinoBench benchmarks/benchmarks.cpp ${SOURCE_FILES})
install(TARGETS WARDuino DESTINATION demo/run_wa/bin)

add_custom_command(
        TARGET WARDuinoBench
        POST_BUILD
        COMMAND make -C ../benchmarks/tasks all
)
