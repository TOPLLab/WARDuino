cmake_minimum_required(VERSION 3.14)
project(WARDuino)

find_package(Threads REQUIRED)

add_compile_options(-g -v -std=c++11 -Wall -Wextra -Wunused)
set(SOURCE_FILES ../../src/Memory/mem.cpp ../../src/Utils/util.cpp ../../src/Utils/util_arduino.cpp ../../src/Debug/debugger.cpp ../../src/Utils/macros.cpp ../../src/WARDuino/WARDuino.cpp ../../src/Primitives/primitives.cpp ../../src/Interpreter/instructions.cpp ../../src/WARDuino/CallbackHandler.cpp)
set(TEST_FRAMEWORK ../../tests/integration/wasm_tests.cpp ../../tests/integration/assertion.cpp ../../tests/integration/sexpr-parser/src/sexpr.c)

add_definitions(-DINFO=0)
add_definitions(-DDEBUG=0)
add_definitions(-DTRACE=0)
add_definitions(-DWARN=0)

# WARDuino CLI
add_executable(warduino main.cpp ${SOURCE_FILES} ${TEST_FRAMEWORK})
target_link_libraries(warduino PRIVATE Threads::Threads)

#install(TARGETS WARDuino DESTINATION demo/run_wa/bin)
